#include "mkutils.h"
#include "ModMenu.h"
#include "ModSettings.h"
#include "GameToast.h"
#include "PakMods.h"
#include "GameMessageBox.h"
#include "StateFile.h"
#include "Changelog.h"
#include "../IniReader.h"
#include <algorithm>
#include "mk12.h"
#include <cstring>
#include <cwchar>
#include <vector>

namespace ModMenu {
	constexpr const wchar_t* SETTINGS_KEY = L"Settings"; // The game's own Settings entry, ours goes right after it

	// Pick actions are small objects whose vtable has 11 slots. Ours is a copy of the Settings action's vtable
	// (with the slot before it, the type info) where cloning keeps our vtable and running opens our menu
	constexpr int VTABLE_SLOTS = 11;
	constexpr int SLOT_CLONE = 8; // (self, destination callback)
	constexpr int SLOT_RUN = 9; // (self, context)
	constexpr int SLOT_RUN_PICKED = 10; // (self, context), what a pick calls

	typedef void(__fastcall CloneType)(void* Self, MK12::FMenuEntryCallback* Destination);
	typedef __int64(__fastcall RunType)(void* Self, void* Context);

	static void* OurVTableWithInfo[VTABLE_SLOTS + 1] = {};
	static void** OurVTable = &OurVTableWithInfo[1];
	static CloneType* SettingsClone = nullptr;
	static RunType* SettingsRun = nullptr;
	static RunType* SettingsRunPicked = nullptr;

	static void __fastcall OurClone(void* Self, MK12::FMenuEntryCallback* Destination)
	{
		SettingsClone(Self, Destination); // Writes the Settings vtable into the copy
		if (Destination && Destination->Instance)
			*(void***)Destination->Instance = OurVTable;
	}

	// Opening the settings screen waits until the player backs out, so our pages show for the whole visit
	static __int64 __fastcall OurRun(void* Self, void* Context)
	{
		ModSettings::SetOwnMenu(true);
		__int64 Result = SettingsRun(Self, Context);
		ModSettings::SetOwnMenu(false);
		return Result;
	}

	static __int64 __fastcall OurRunPicked(void* Self, void* Context)
	{
		ModSettings::SetOwnMenu(true);
		__int64 Result = SettingsRunPicked(Self, Context);
		ModSettings::SetOwnMenu(false);
		return Result;
	}

	static void BuildVTable(void** SettingsVTable)
	{
		memcpy(OurVTableWithInfo, SettingsVTable - 1, sizeof(OurVTableWithInfo));
		SettingsClone = (CloneType*)SettingsVTable[SLOT_CLONE];
		SettingsRun = (RunType*)SettingsVTable[SLOT_RUN];
		SettingsRunPicked = (RunType*)SettingsVTable[SLOT_RUN_PICKED];
		OurVTable[SLOT_CLONE] = (void*)OurClone;
		OurVTable[SLOT_RUN] = (void*)OurRun;
		OurVTable[SLOT_RUN_PICKED] = (void*)OurRunPicked;
	}

	// The entry frees its strings, so they go in the game's own memory
	static void SetGameString(MK12::FString& Out, const wchar_t* Text)
	{
		int32_t Count = (int32_t)wcslen(Text) + 1;
		MK12::StringReserve(&Out, Count);
		memcpy(Out.Data, Text, Count * sizeof(wchar_t));
		Out.Count = Count;
	}

	constexpr const char* LAST_VERSION_KEY = "szLastVersion"; // [FirstRun] in the state file

	// Once per session, the first time the main menu is built: which version runs and how many mods loaded. A new
	// version (or a first run) offers what's new on ACCEPT
	static void AnnounceLoaded()
	{
		static bool bAnnounced = false;
		if (bAnnounced)
			return;
		bAnnounced = true;

		const auto& Mods = PakMods::List();
		size_t Loaded = std::count_if(Mods.begin(), Mods.end(), [](const PakMods::Mod& mod) { return mod.bEnabled; });
		std::wstring Title = L"MK12TTH " + ModSettings::ModVersion();
		std::wstring ModsText = L"Loaded mods " + std::to_wstring(Loaded) + L"/" + std::to_wstring(Mods.size());

		CIniReader State((char*)StateFile::FILE_NAME);
		char* Last = State.ReadString((char*)StateFile::FIRST_RUN, (char*)LAST_VERSION_KEY, "");
		std::string Version;
		for (wchar_t c : ModSettings::ModVersion())
			Version += (char)c; // Digits and dots
		bool bNew = Version != Last;
		bool bFirst = !*Last;
		delete[] Last;
		if (bNew)
		{
			State.WriteString((char*)StateFile::FIRST_RUN, (char*)LAST_VERSION_KEY, (char*)Version.c_str());
			Title += bFirst ? L" installed" : L" updated";
		}
		// Every start: ACCEPT is there whenever someone wants it, ignoring it costs nothing
		GameToast::Show(Title, ModsText + L". ACCEPT shows what's new.", []
		{
			GameMessageBox::Confirm(Changelog::Current, nullptr, L"MK12TTH " + ModSettings::ModVersion());
		});
	}

	static int FindSettings(MK12::FSettingsTab* Tabs, int32_t Count)
	{
		for (int32_t i = 0; i < Count; i++)
		{
			MK12::TArray<MK12::FMenuEntry>& Entries = Tabs[i].Entries;
			if (Entries.ElementsCount == 1 && Entries.Data[0].Key.Data && !wcscmp(Entries.Data[0].Key.Data, SETTINGS_KEY))
				return i;
		}
		return -1;
	}

	void CopyMainMenuTabs(void* Out, MK12::FSettingsTab* Tabs, int32_t Count, int32_t ExtraA, int32_t ExtraB)
	{
		int Settings = ModSettings::HasOwnMenu() ? FindSettings(Tabs, Count) : -1;
		MK12::FMenuEntry* SettingsEntry = Settings >= 0 ? &Tabs[Settings].Entries.Data[0] : nullptr;
		if (!SettingsEntry || !SettingsEntry->OnPick)
		{
			MK12::MainMenuCopyTabs(Out, Tabs, Count, ExtraA, ExtraB);
			return;
		}

		void** SettingsVTable = *(void***)SettingsEntry->OnPick;
		if (!SettingsClone)
			BuildVTable(SettingsVTable);

		// A copy of the Settings action that runs through our vtable. The entry takes it over and frees it
		MK12::FMenuEntryCallback OnPick;
		SettingsClone(SettingsEntry->OnPick, &OnPick);
		if (!OnPick.Instance)
		{
			MK12::MainMenuCopyTabs(Out, Tabs, Count, ExtraA, ExtraB);
			return;
		}
		*(void***)OnPick.Instance = OurVTable;

		MK12::FMenuEntry Entry = {};
		MK12::MenuEntryConstruct(&Entry, EntryKey, &OnPick);
		Entry.bLocalized = false; // Shows the key as the label and Description under the menu
		SetGameString(Entry.Description, EntryDescription);
		Entry.FlagsA = SettingsEntry->FlagsA;
		Entry.FlagsB = SettingsEntry->FlagsB;

		// The game copies the list, so ours lives on a copy and goes away after
		std::vector<MK12::FSettingsTab> WithOurs(Tabs, Tabs + Count);
		WithOurs.insert(WithOurs.begin() + Settings + 1, MK12::FSettingsTab{});
		MK12::FSettingsTab& Ours = WithOurs[Settings + 1];
		MK12::SettingsTabConstruct(&Ours, &Entry, EntryId);

		MK12::MainMenuCopyTabs(Out, WithOurs.data(), (int32_t)WithOurs.size(), ExtraA, ExtraB);
		MK12::SettingsTabDestruct(&Ours);
		AnnounceLoaded();
	}
}
