#pragma once
#include <cstdint>
#include <ctime>
#include <vector>

// Floyd (Taco) Tower schedule. Game mode reads the game's own windows, Calculated mode
// redoes the game's math from the profile hash
namespace FloydTower {
	struct Window {
		time_t	Start	= 0;	// Unix seconds (UTC)
		time_t	End		= 0;
		int		Slot	= 0;	// Living Towers slot, 0 is the leftmost

		bool operator==(const Window& Other) const { return Start == Other.Start && End == Other.End && Slot == Other.Slot; }
	};

	// Calculated mode
	Window		GetWindow(uint32_t ProfileHash, time_t Now);	// Current window if it's up, else the next one
	void		LogSchedule(uint32_t ProfileHash);

	// Game mode
	time_t		TicksToUnix(int64_t Ticks);
	int			SlotFromSplineName(const wchar_t* SplineName);	// MapMode_MeshSpline_BP_C_x
	bool		SetGameWindows(const std::vector<Window>& Windows);	// True when the game's list changed
	bool		HasGameWindows();
	bool		GetGameWindow(time_t Now, Window& Out);
	void		LogGameSchedule();

	// The window to show right now, from the game's schedule or worked out from the profile hash
	enum class Status {
		Ready,			// Out is the current window if it's up, else the next one
		NoSchedule,		// Nothing read yet: Living Towers wasn't opened, or Floyd wasn't beaten (Calculated)
		NotUnlocked,	// The game's schedule has no Floyd Tower: Floyd wasn't beaten yet
	};
	Status		Find(time_t Now, uint32_t ProfileHash, bool bFromGame, Window& Out);

	// Floyd's challenges, the same generator shuffles all of them. Seed: the profile hash high, Floyd encounters low
	constexpr int CHALLENGE_COUNT = 37;
	constexpr int PLAYER_CHALLENGES = 10;
	std::vector<uint8_t> FloydChallenges(uint64_t Seed); // The player's, numbered from 1 like the Floyd tracker

	// Shared
	bool		PickWindow(const std::vector<Window>& Windows, time_t Now, Window& Out);
	void		LogWindow(const Window& window, time_t Now);
	const char*	SlotName(int Slot);
	void		FormatDuration(time_t Seconds, char* Buffer, size_t Size);
	void		FormatLocalTime(time_t Time, char* Buffer, size_t Size);
}
