#include "AttractMode.h"
#include <Windows.h>
#include <atomic>
#include <cstdlib>
#include <cstring>

namespace AttractMode {
	constexpr int32_t FRAMES_PER_SECOND = 60; // The timer waits one frame per count, the menu runs at 60

	static uint32_t* Limit = nullptr;
	static std::atomic<int32_t> Seconds = GameSeconds;
	static std::atomic<bool> bRestart = false; // Changed: start counting from 0, so a shorter delay doesn't fire right away

	static bool WriteLimit(uint32_t Frames)
	{
		DWORD Old;
		if (!Limit || !VirtualProtect(Limit, sizeof(*Limit), PAGE_EXECUTE_READWRITE, &Old))
			return false;
		*Limit = Frames;
		VirtualProtect(Limit, sizeof(*Limit), Old, &Old);
		return true;
	}

	bool Init(uint64_t LimitAddress)
	{
		Limit = (uint32_t*)LimitAddress;
		if (!Limit || *Limit != GameSeconds * FRAMES_PER_SECOND) // Not the timer we know
		{
			Limit = nullptr;
			return false;
		}
		return true;
	}

	bool IsReady()
	{
		return Limit != nullptr;
	}

	int32_t FromIni(const char* Value)
	{
		if (!Value || !*Value || !_stricmp(Value, "Game"))
			return GameSeconds;
		if (!_stricmp(Value, "Never"))
			return Never;
		int32_t Parsed = atoi(Value);
		return Parsed > 0 ? Parsed : GameSeconds;
	}

	int32_t Delay()
	{
		return Seconds;
	}

	void SetDelay(int32_t NewSeconds)
	{
		if (!IsReady() || NewSeconds < 0)
			return;
		if (NewSeconds != Never && !WriteLimit((uint32_t)NewSeconds * FRAMES_PER_SECOND))
			return;
		Seconds = NewSeconds;
		bRestart = true;
	}

	bool ShouldRestartTimer()
	{
		return Seconds == Never || bRestart.exchange(false);
	}
}
