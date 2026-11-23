#pragma once
#include <cstdint>

// When attract mode (the demo after idling on the main menu) starts. The idle timer counts frames up to a limit in the
// game's code: we change the limit, or keep restarting the timer for never
namespace AttractMode {
	constexpr int32_t GameSeconds = 30; // The game's own delay
	constexpr int32_t Never = 0;

	bool Init(uint64_t LimitAddress); // The timer's frame limit in the game's code
	bool IsReady();
	int32_t FromIni(const char* Value); // "Game", "Never" or seconds
	int32_t Delay(); // Seconds, or Never
	void SetDelay(int32_t Seconds); // The timer starts over

	bool ShouldRestartTimer(); // Every frame by the idle timer, true restarts it
}
