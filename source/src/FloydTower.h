#pragma once
#include <cstdint>
#include <ctime>
#include <vector>

// Floyd (Taco) Tower schedule. Game mode reads the game's own windows, Calculated mode
// redoes the game's math (PC sub_142134440, FloydBackend src/utils/taco.py) from the profile hash
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

	// Shared
	bool		PickWindow(const std::vector<Window>& Windows, time_t Now, Window& Out);
	void		LogWindow(const Window& window, time_t Now);
	const char*	SlotName(int Slot);
	void		FormatDuration(time_t Seconds, char* Buffer, size_t Size);
	void		FormatLocalTime(time_t Time, char* Buffer, size_t Size);
}
