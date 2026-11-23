#pragma once
#include <cstdint>

namespace MK12 { struct FSettingsTab; }

// Our MODS entry in the main menu, after Settings. It opens the settings screen with only our pages, through a copy of
// the Settings entry's own pick action so it opens and closes like Settings
namespace ModMenu {
	constexpr const wchar_t* EntryKey = L"MODS"; // Label, shown as is
	constexpr const wchar_t* EntryDescription = L"Pak mods, live MK12TTH features, logging and your profile info.";
	constexpr int32_t EntryId = 50; // The game doesn't read it

	// Hook on the main menu's entry list, game thread
	void CopyMainMenuTabs(void* Out, MK12::FSettingsTab* Tabs, int32_t Count, int32_t ExtraA, int32_t ExtraB);
}
