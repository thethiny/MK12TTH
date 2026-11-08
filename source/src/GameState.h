#pragma once
#include <cstdint>

// What the game is doing right now, for features that must stay out of loading screens and
// online modes. Read on the game thread only, Tick runs every frame
namespace GameState {
	bool IsReady(); // The game's state functions were found
	bool HasModeManager(); // False until the game has a world (boot, intro)
	bool IsOnline(); // An online mode is active (versus, King of the Hill, online practice, lobbies)
	bool IsLoading(); // Loading, or just finished loading (short grace period)
	bool CanShowUI(); // Ready, has a world, not online, not loading
	bool IsMainMenuUp(); // The main menu is the screen on top, UI can show
	uint32_t MainMenuEntries(); // Goes up each time the main menu comes up (after boot, after leaving a mode)

	void Tick();
}
