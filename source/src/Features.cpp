#include "mkutils.h"
#include "Features.h"
#include "StateFile.h"
#include "../IniReader.h"
#include <atomic>
#include <string>

namespace Features {
	struct Entry {
		Definition			Feature;
		bool				bDefined = false;
		State				Current = State::Pending;
		Id					Blocker = Id::Count;
		std::atomic<bool>	bActive = false;
	};
	static Entry Entries[(int)Id::Count];
	static std::vector<Id> InstallOrder; // As defined

	static Entry* Find(Id Which)
	{
		int Index = (int)Which;
		return Index >= 0 && Index < (int)Id::Count && Entries[Index].bDefined ? &Entries[Index] : nullptr;
	}

	// Startup only moves a feature out of Pending, once. Everything after that is the live switch
	static bool IsAllowed(State From, State To)
	{
		return From == State::Pending && To != State::Pending;
	}

	static void Transition(Entry& entry, State To)
	{
		if (!IsAllowed(entry.Current, To))
		{
			printfError("Features: %ls can't go from state %d to %d", entry.Feature.Title, (int)entry.Current, (int)To);
			return;
		}
		entry.Current = To;
	}

	void Define(Definition Feature)
	{
		Entry& entry = Entries[(int)Feature.Which];
		for (Id Need : Feature.Needs)
		{
			if (!Find(Need)) // Invariant: needs are defined first, so installing in order sees their final state
				printfError("Features: %ls needs a feature that isn't defined before it", Feature.Title);
		}
		if (!entry.bDefined)
			InstallOrder.push_back(Feature.Which);
		entry.Feature = std::move(Feature);
		entry.bDefined = true;
	}

	static bool ReadIniSwitch(const Definition& Feature)
	{
		if (!Feature.IniKey || !Feature.IniSection)
			return true;
		CIniReader ini("");
		return ini.ReadBoolean((char*)Feature.IniSection, (char*)Feature.IniKey, Feature.bIniDefault);
	}

	static bool ReadLiveSwitch(const Definition& Feature)
	{
		CIniReader ini((char*)StateFile::FILE_NAME);
		return ini.ReadBoolean((char*)StateFile::LIVE, (char*)Feature.IniKey, Feature.bLiveDefault);
	}

	void InstallAll()
	{
		for (Id Which : InstallOrder)
		{
			Entry& entry = Entries[(int)Which];
			const Definition& Feature = entry.Feature;

			if (!ReadIniSwitch(Feature))
			{
				Transition(entry, State::Disabled);
				continue;
			}
			for (Id Need : Feature.Needs)
			{
				Entry* Needed = Find(Need);
				if (!Needed || Needed->Current != State::Installed)
				{
					entry.Blocker = Need;
					break;
				}
			}
			if (entry.Blocker != Id::Count)
			{
				printfWarning("%ls: needs %ls, skipped", Feature.Title, Find(entry.Blocker) ? Find(entry.Blocker)->Feature.Title : L"?");
				Transition(entry, State::Blocked);
				continue;
			}

			bool bInstalled = !Feature.Install || Feature.Install();
			Transition(entry, bInstalled ? State::Installed : State::Failed);
			if (!bInstalled)
				continue;
			entry.bActive = Feature.Type != Kind::Live || ReadLiveSwitch(Feature);
			if (Feature.OnSwitch)
				Feature.OnSwitch(entry.bActive);
		}
	}

	const Definition& Get(Id Which)
	{
		static Definition None = { Id::Count, nullptr, false, Kind::Hidden, {}, nullptr, L"", L"" };
		Entry* entry = Find(Which);
		return entry ? entry->Feature : None;
	}

	State GetState(Id Which)
	{
		Entry* entry = Find(Which);
		return entry ? entry->Current : State::Disabled;
	}

	Id BlockedBy(Id Which)
	{
		Entry* entry = Find(Which);
		return entry ? entry->Blocker : Id::Count;
	}

	bool HasStartupSwitch(Id Which)
	{
		const Definition& Feature = Get(Which);
		return Feature.IniKey && Feature.IniSection && Feature.Type != Kind::Hidden;
	}

	bool IsInstalled(Id Which)
	{
		return GetState(Which) == State::Installed;
	}

	bool IsActive(Id Which)
	{
		Entry* entry = Find(Which);
		return entry && entry->Current == State::Installed && entry->bActive;
	}

	void SetActive(Id Which, bool bActive)
	{
		Entry* entry = Find(Which);
		if (!entry || entry->Current != State::Installed || entry->Feature.Type != Kind::Live)
			return;
		entry->bActive = bActive;
		if (entry->Feature.OnSwitch)
			entry->Feature.OnSwitch(bActive);
		CIniReader ini((char*)StateFile::FILE_NAME);
		ini.WriteBoolean((char*)StateFile::LIVE, (char*)entry->Feature.IniKey, bActive);
	}

	std::wstring Explain(Id Which)
	{
		switch (GetState(Which))
		{
		case State::Installed:	return L"";
		case State::Disabled:	return L"Off in Settings, MK12TTH. Turning it on takes a restart.";
		case State::Failed:		return L"Couldn't be set up in this version of the game.";
		case State::Blocked:	return std::wstring(L"Needs ") + Get(BlockedBy(Which)).Title + L", which isn't running.";
		default:				return L"Not set up yet.";
		}
	}

	void PrintSummary()
	{
		printf("\n==Features==\n");
		for (Entry& entry : Entries)
		{
			if (!entry.bDefined)
				continue;
			const wchar_t* Title = entry.Feature.Title;
			switch (entry.Current)
			{
			case State::Installed:	printfSuccess("%ls: on%s", Title, entry.Feature.Type == Kind::Live && !entry.bActive ? " (switched off in MODS)" : ""); break;
			case State::Disabled:	printfInfo("%ls: off in the ini", Title); break;
			case State::Failed:		printfError("%ls: failed", Title); break;
			case State::Blocked:	printfWarning("%ls: blocked by %ls", Title, Get(entry.Blocker).Title); break;
			default:				break;
			}
		}
	}
}
