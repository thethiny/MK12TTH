#include "FloydTower.h"
#include <cstdio>
#include <cwchar>
#include <utility>

namespace FloydTower {
	constexpr uint64_t	PCG_MULT			= 0x5851F42D4C957F2D;
	constexpr uint64_t	PCG_INC				= 0x280AF6FDEECF029F;
	constexpr int64_t	TICKS_PER_SECOND	= 10000000;
	constexpr int64_t	UNIX_EPOCH_TICKS	= 621355968000000000;	// FDateTime ticks at 1970-01-01
	constexpr time_t	SECONDS_PER_DAY		= 86400;

	// Constants baked in the game, 1 tower per day so only the first shuffled hour is used
	constexpr int		MIN_SLOT			= 4;
	constexpr int		SLOT_COUNT			= 6;
	constexpr time_t	DURATION			= 3600;

	// Spline number per slot
	static const int SlotSplines[SLOT_COUNT] = { 1, 2, 4, 5, 7, 8 };
	static const char* SlotNames[SLOT_COUNT] = {
		"C_1", "C_2", "C_4", "C_5",
		"C_7 (2nd from the right)",
		"C_8 (rightmost)",
	};

	static std::vector<Window> GameWindows;
	static bool bGameWindowsReceived = false;

	// The game's ShuffleUsingSeed (PCG32)
	struct Pcg32 {
		uint64_t State;

		explicit Pcg32(uint64_t Seed) : State(PCG_MULT * (Seed + PCG_INC) + PCG_INC) {}

		uint32_t Bounded(uint32_t Bound)
		{
			if (!Bound)
				return 0;

			uint32_t Threshold = (0u - Bound) % Bound;
			while (true)
			{
				uint64_t Old = State;
				State = PCG_INC + PCG_MULT * Old;
				uint32_t XorShifted = (uint32_t)((Old >> 27) ^ (Old >> 45));
				uint32_t Rot = (uint32_t)(Old >> 59);
				uint32_t Value = (XorShifted >> Rot) | (XorShifted << ((0u - Rot) & 31));
				if (Value >= Threshold)
					return Value % Bound;
			}
		}
	};

	std::vector<uint8_t> FloydChallenges(uint64_t Seed)
	{
		Pcg32 Rng(~Seed);
		uint8_t Order[CHALLENGE_COUNT];
		for (int i = 0; i < CHALLENGE_COUNT; i++)
			Order[i] = (uint8_t)i;
		for (int i = 0; i < CHALLENGE_COUNT; i++)
			std::swap(Order[i], Order[i + Rng.Bounded(CHALLENGE_COUNT - i)]);

		std::vector<uint8_t> Challenges;
		for (int i = 0; i < PLAYER_CHALLENGES; i++)
			Challenges.push_back(Order[i] + 1);
		return Challenges;
	}

	static Window DayWindow(uint32_t ProfileHash, time_t DayStart)
	{
		int64_t DayTicks = UNIX_EPOCH_TICKS + (int64_t)DayStart * TICKS_PER_SECOND;
		Pcg32 Rng((uint64_t)DayTicks ^ ProfileHash);

		int Hours[24];
		for (int i = 0; i < 24; i++)
			Hours[i] = i;
		for (int i = 0; i < 23; i++)
			std::swap(Hours[i], Hours[i + Rng.Bounded(24 - i)]);

		Window window;
		window.Slot = MIN_SLOT + Rng.Bounded(SLOT_COUNT - MIN_SLOT);
		window.Start = DayStart + Hours[0] * 3600 + window.Slot * 600; // Slot index is also the minute offset
		window.End = window.Start + DURATION;
		return window;
	}

	Window GetWindow(uint32_t ProfileHash, time_t Now)
	{
		time_t Today = Now - Now % SECONDS_PER_DAY;

		// Yesterday's window can spill past midnight, tomorrow's is the fallback
		std::vector<Window> Windows;
		for (int Day = -1; Day <= 1; Day++)
			Windows.push_back(DayWindow(ProfileHash, Today + Day * SECONDS_PER_DAY));

		Window Best;
		PickWindow(Windows, Now, Best);
		return Best;
	}

	void LogSchedule(uint32_t ProfileHash)
	{
		time_t Now = time(nullptr);
		LogWindow(GetWindow(ProfileHash, Now), Now);
	}

	time_t TicksToUnix(int64_t Ticks)
	{
		return (time_t)((Ticks - UNIX_EPOCH_TICKS) / TICKS_PER_SECOND);
	}

	int SlotFromSplineName(const wchar_t* SplineName)
	{
		const wchar_t* Suffix = SplineName ? wcsrchr(SplineName, L'_') : nullptr;
		if (!Suffix)
			return -1;

		int Spline = _wtoi(Suffix + 1);
		for (int i = 0; i < SLOT_COUNT; i++)
		{
			if (SlotSplines[i] == Spline)
				return i;
		}
		return -1;
	}

	bool SetGameWindows(const std::vector<Window>& Windows)
	{
		bool Changed = !bGameWindowsReceived || Windows != GameWindows;
		GameWindows = Windows;
		bGameWindowsReceived = true;
		return Changed;
	}

	bool HasGameWindows()
	{
		return bGameWindowsReceived;
	}

	bool GetGameWindow(time_t Now, Window& Out)
	{
		return PickWindow(GameWindows, Now, Out);
	}

	Status Find(time_t Now, uint32_t ProfileHash, bool bFromGame, Window& Out)
	{
		if (!bFromGame)
		{
			if (!ProfileHash)
				return Status::NoSchedule; // The game only hashes once Floyd was beaten
			Out = GetWindow(ProfileHash, Now);
			return Status::Ready;
		}
		if (!HasGameWindows())
			return Status::NoSchedule;
		return GetGameWindow(Now, Out) ? Status::Ready : Status::NotUnlocked;
	}

	void LogGameSchedule()
	{
		time_t Now = time(nullptr);
		Window window;
		if (GetGameWindow(Now, window))
			LogWindow(window, Now);
		else
			printf("Floyd Tower: Not in the game's schedule. Beat Floyd once to unlock it.\n"); // Empty list means the game's unlock check failed
	}

	bool PickWindow(const std::vector<Window>& Windows, time_t Now, Window& Out)
	{
		bool Found = false;
		for (const Window& Candidate : Windows)
		{
			if (Candidate.End > Now && (!Found || Candidate.Start < Out.Start))
			{
				Out = Candidate;
				Found = true;
			}
		}
		return Found;
	}

	void LogWindow(const Window& window, time_t Now)
	{
		char When[32], Duration[32];
		if (window.Start <= Now)
		{
			FormatLocalTime(window.End, When, sizeof(When));
			FormatDuration(window.End - Now, Duration, sizeof(Duration));
			printf("Floyd Tower: Up now until %s, ends in %s. Slot %s\n", When, Duration, SlotName(window.Slot));
		}
		else
		{
			FormatLocalTime(window.Start, When, sizeof(When));
			FormatDuration(window.Start - Now, Duration, sizeof(Duration));
			printf("Floyd Tower: Next at %s, in %s. Slot %s\n", When, Duration, SlotName(window.Slot));
		}
	}

	const char* SlotName(int Slot)
	{
		return Slot >= 0 && Slot < SLOT_COUNT ? SlotNames[Slot] : "Unknown";
	}

	void FormatDuration(time_t Seconds, char* Buffer, size_t Size)
	{
		long long Minutes = (Seconds + 59) / 60;
		if (Minutes >= 60)
			snprintf(Buffer, Size, "%lldh %lldm", Minutes / 60, Minutes % 60);
		else
			snprintf(Buffer, Size, "%lldm", Minutes);
	}

	void FormatLocalTime(time_t Time, char* Buffer, size_t Size)
	{
		tm Local;
		localtime_s(&Local, &Time);
		strftime(Buffer, Size, "%a %H:%M", &Local);
	}
}
