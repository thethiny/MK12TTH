#pragma once
#include <cstdint>

// When attract mode (the demo after idling) starts. Two timers start it:
// - the main menu's idle timer counts frames up to a limit in the game's code: we change the limit, or keep restarting
//   the timer for never
// - the title screen ("press any button") waits in the attract script: it counts frames up to one of the script's
//   numbers. We change that number (and the one that gets the demo ready shortly before) in the loaded script, each
//   time the game loads it
namespace AttractMode {
	constexpr int32_t GameSeconds = 30; // The game's own delay, on both screens
	constexpr int32_t Never = 0;

	bool Init(uint64_t LimitAddress); // The main menu timer's frame limit in the game's code
	bool IsReady();
	void InitTitle(); // MK12::FindScript is set: the title screen follows the delay too
	bool IsTitleReady();
	int32_t FromIni(const char* Value); // "Game", "Never" or seconds
	int32_t Delay(); // Seconds, or Never
	void SetDelay(int32_t Seconds); // The main menu timer starts over

	bool ShouldRestartTimer(); // Every frame by the main menu's idle timer, true restarts it
	void Tick(); // Once per frame on the game thread: the title screen's wait gets the delay while the script is loaded
}
