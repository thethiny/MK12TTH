#pragma once
#include "GameMessageBox.h"

// The game's notification toasts (the social ones) with our own text. They only show on the main menu and its screens,
// until then requests wait. The game keeps its own queue
namespace GameToast {
	constexpr int32_t DefaultDuration = -1; // The game's own, about 4 seconds (6 when long)

	bool IsReady();
	bool IsUp(); // Game thread: a toast shows right away
	void Show(const GameMessageBox::Text& Title, const GameMessageBox::Text& Subtitle = GameMessageBox::Text(), bool bLong = false); // Any thread

	void Tick(); // Game thread, every frame
}
