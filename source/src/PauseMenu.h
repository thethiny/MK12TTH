#pragma once
#include <cstdint>

namespace MK12 { class USettingsScreen; struct FPauseEntry; template <typename T> class TArray; }

// MOD SETTINGS in the in-fight pause menu, right after KONTROLS: the MK12TTH rows marked for fights. It opens the
// panel KONTROLS uses, with our rows in place of the game's. Never in online modes
namespace PauseMenu {
	constexpr const wchar_t* EntryKey = L"MOD SETTINGS"; // Label, shown as is
	constexpr const wchar_t* AfterKey = L"CharacterControls"; // KONTROLS, its panel is the one we use

	// Called by the hooks, game thread only
	void OnEntriesCollected(MK12::TArray<MK12::FPauseEntry>* Entries); // Right after the game listed the pause entries
	void OnPanelSetUp(MK12::USettingsScreen* Screen, void* Player, int64_t Mode); // Right after the game set up the KONTROLS panel
	void OnPauseClosed(void* Pause); // Right before the game closes a pause menu
	void Tick(); // Every frame while the pause menu is open: the panel follows the highlighted entry
}
