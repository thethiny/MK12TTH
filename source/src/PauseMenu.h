#pragma once
#include <cstdint>

namespace MK12 { class USettingsScreen; struct FPauseEntry; template <typename T> class TArray; }

// MOD SETTINGS in the pause menu, right after KONTROLS (first where there's none): the MK12TTH rows marked for
// fights, in a settings panel of its own (the class of KONTROLS' panel). Never in online modes
namespace PauseMenu {
	constexpr const wchar_t* EntryKey = L"MOD SETTINGS"; // Label, shown as is
	constexpr const wchar_t* AfterKey = L"CharacterControls"; // KONTROLS, ours goes right after it

	// Called by the hooks, game thread only
	void OnEntriesCollected(MK12::TArray<MK12::FPauseEntry>* Entries); // Right after the game listed the pause entries
	void OnPanelSetUp(MK12::USettingsScreen* Screen, void* Player, int64_t Mode); // Right after the game set up a pause settings panel
	void OnPauseClosed(void* Pause); // Right before the game closes a pause menu
	void Tick(); // Every frame: loads our panel's class once the main menu is up, forgets a pause menu gone with its fight
}
