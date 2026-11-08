#pragma once
#include "GameMessageBox.h"

// The game's notification toasts (the ones social actions show), with our own text. They only show in the main
// menu and its screens: requests wait there until it's up. Non-blocking, the game keeps its own queue
namespace GameToast {
	constexpr int32_t DefaultDuration = -1; // The game's own, about 4 seconds (6 when long)

	bool IsReady(); // The game's toast functions were found
	bool IsUp(); // Game thread: a toast shows right away (the main menu is up)
	// Any thread
	void Show(const GameMessageBox::Text& Title, const GameMessageBox::Text& Subtitle = {}, bool bLong = false);

	// Game thread only, called every frame by the hook
	void Tick();
}
