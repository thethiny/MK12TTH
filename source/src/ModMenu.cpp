#include "mkutils.h"
#include "ModMenu.h"
#include "ModSettings.h"
#include "GameToast.h"
#include "PakMods.h"
#include "GameMessageBox.h"
#include "StateFile.h"
#include "Changelog.h"
#include "MenuActions.h"
#include "../IniReader.h"
#include <algorithm>
#include "mk12.h"
#include <cstring>
#include <cwchar>
#include <vector>

namespace ModMenu {
	constexpr const wchar_t* SETTINGS_KEY = L"Settings"; // The game's own Settings entry, ours goes right after it

	// Our entry runs the Settings entry's own action (a copy through our vtable): opening the settings screen waits
	// until the player backs out, so our pages show for the whole visit
	static void OpenOwnMenu()
	{
		ModSettings::SetOwnMenu(true);
	}

	static void CloseOwnMenu()
	{
		ModSettings::SetOwnMenu(false);
	}

	static const MenuActions::Wrap OwnMenu = { OpenOwnMenu, CloseOwnMenu };

	static void SetGameString(MK12::FString& Out, const wchar_t* Text)
	{ // The entry frees its strings, so they go in the game's memory
		int32_t Count = (int32_t)wcslen(Text) + 1;
		MK12::StringReserve(&Out, Count);
		memcpy(Out.Data, Text, Count * sizeof(wchar_t));
		Out.Count = Count;
	}

	constexpr const char* LAST_VERSION_KEY = "szLastVersion"; // [FirstRun] in the state file

	static void ShowWhatsNew()
	{
		GameMessageBox::Confirm(Changelog::Current, nullptr, L"MK12TTH " + ModSettings::ModVersion());
	}

	static void AnnounceLoaded()
	{ // Once, the first time the main menu is built. ACCEPT shows what's new
		static bool bAnnounced = false;
		if (bAnnounced)
			return;
		bAnnounced = true;

		const std::vector<PakMods::Mod>& Mods = PakMods::List();
		size_t Loaded = 0;
		for (size_t i = 0; i < Mods.size(); i++)
		{
			if (Mods[i].bEnabled)
				Loaded++;
		}
		std::wstring Title = L"MK12TTH " + ModSettings::ModVersion();
		std::wstring ModsText = L"Loaded mods " + std::to_wstring(Loaded) + L"/" + std::to_wstring(Mods.size());

		CIniReader State((char*)StateFile::FileName());
		char* Last = State.ReadString((char*)StateFile::FIRST_RUN, (char*)LAST_VERSION_KEY, "");
		const std::wstring& ModVersion = ModSettings::ModVersion();
		std::string Version;
		for (size_t i = 0; i < ModVersion.size(); i++)
			Version += (char)ModVersion[i]; // Digits and dots
		bool bNew = Version != Last;
		bool bFirst = !*Last;
		delete[] Last;
		if (bNew)
		{
			State.WriteString((char*)StateFile::FIRST_RUN, (char*)LAST_VERSION_KEY, (char*)Version.c_str());
			Title += bFirst ? L" installed" : L" updated";
		}
		GameToast::Show(Title, ModsText + L". ACCEPT shows what's new.", ShowWhatsNew);
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
		MK12::FMenuEntryCallback OnPick;
		if (!SettingsEntry || !MenuActions::Clone(SettingsEntry->OnPick, &OwnMenu, OnPick))
		{
			MK12::MainMenuCopyTabs(Out, Tabs, Count, ExtraA, ExtraB);
			return;
		}

		// The entry takes the action over and frees it
		MK12::FMenuEntry Entry = {};
		MK12::MenuEntryConstruct(&Entry, EntryKey, &OnPick);
		Entry.bLocalized = false; // Shows the key as the label and Description under the menu
		SetGameString(Entry.Description, EntryDescription);
		Entry.FlagsA = SettingsEntry->FlagsA;
		Entry.FlagsB = SettingsEntry->FlagsB;

		// The game copies the list, ours only lives on this copy
		std::vector<MK12::FSettingsTab> WithOurs(Tabs, Tabs + Count);
		WithOurs.insert(WithOurs.begin() + Settings + 1, MK12::FSettingsTab{});
		MK12::FSettingsTab& Ours = WithOurs[Settings + 1];
		MK12::SettingsTabConstruct(&Ours, &Entry, EntryId);

		MK12::MainMenuCopyTabs(Out, WithOurs.data(), (int32_t)WithOurs.size(), ExtraA, ExtraB);
		MK12::SettingsTabDestruct(&Ours);
		AnnounceLoaded();
	}
}
