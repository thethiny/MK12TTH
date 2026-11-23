#pragma once
#include "GameMessageBox.h"

// The game's notification toasts (the social ones) with our own text. They only show on the main menu and its screens,
// until then requests wait. The game keeps its own queue
namespace GameToast {
	constexpr int32_t DefaultDuration = -1; // The game's own, about 4 seconds
	constexpr int32_t ActionDuration = 600; // Frames, with an action: time to read and press

	typedef void (AcceptCallback)();

	bool IsReady();
	bool IsUp(); // Game thread: a toast shows right away
	// Any thread. OnAccept adds the game's ACCEPT and DECLINE prompts (fixed labels, the text says what ACCEPT does).
	// ACCEPT runs it on the game thread
	void Show(const GameMessageBox::Text& Title, const GameMessageBox::Text& Subtitle = GameMessageBox::Text(), AcceptCallback* OnAccept = nullptr);

	void Tick(); // Game thread, every frame
}
