#pragma once
#include <cstdint>

// When attract mode (the demo after idling on the main menu) starts, changed while the game runs. The game's idle
// timer counts frames up to a limit, we change that limit, or keep restarting the timer so it never starts
namespace AttractMode {
	constexpr int32_t GameSeconds = 30; // The game's own delay
	constexpr int32_t Never = 0;

	bool Init(uint64_t LimitAddress); // Where the timer's frame limit sits in the game's code
	bool IsReady();
	int32_t FromIni(const char* Value); // "Game", "Never" or seconds
	int32_t Delay(); // Seconds, or Never
	void SetDelay(int32_t Seconds); // The timer starts over

	// Asked every frame by the idle timer: true restarts it
	bool ShouldRestartTimer();
}
