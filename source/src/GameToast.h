#pragma once
#include "GameMessageBox.h"
#include <functional>

// The game's notification toasts (the ones social actions show), with our own text. They only show in the main
// menu and its screens: requests wait there until it's up. Non-blocking, the game keeps its own queue
namespace GameToast {
	constexpr int32_t DefaultDuration = -1; // The game's own, about 4 seconds
	constexpr int32_t ActionDuration = 600; // With an action, frames: time to read it and press

	bool IsReady(); // The game's toast functions were found
	bool IsUp(); // Game thread: a toast shows right away (the main menu is up)
	// Any thread. With OnAccept the toast shows the game's ACCEPT and DECLINE prompts (their labels are fixed, say what
	// ACCEPT does in the text); ACCEPT runs it on the game thread, DECLINE just closes the toast
	void Show(const GameMessageBox::Text& Title, const GameMessageBox::Text& Subtitle = {}, std::function<void()> OnAccept = nullptr);

	// Game thread only, called every frame by the hook
	void Tick();
}
