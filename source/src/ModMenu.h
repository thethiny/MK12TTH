#pragma once
#include <cstdint>

namespace MK12 { struct FSettingsTab; }

// Our entry in the main menu, right after Settings. Picking it opens the settings screen with only the MODS
// pages. It reuses the Settings entry's own pick action, so the screen opens and closes exactly like Settings does
namespace ModMenu {
	constexpr const wchar_t* EntryKey = L"MODS"; // Label, shown as is
	constexpr const wchar_t* EntryDescription = L"Pak mods, live MK12TTH features, logging and your profile info.";
	constexpr int32_t EntryId = 50; // The game doesn't read it

	// Called by the hook on the main menu's entry list, game thread only
	void CopyMainMenuTabs(void* Out, MK12::FSettingsTab* Tabs, int32_t Count, int32_t ExtraA, int32_t ExtraB);
}
