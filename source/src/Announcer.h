#pragma once
#include <cstdint>
#include <vector>

namespace MK12 { class FString; }

// The announcer voice, picked in MODS: GAME (the player's own setting), AUTO (the announcer of the first human
// player's fighter, when there is one) or a fixed announcer. Applied as fights start and as the main menu loads,
// so menus use it too. Never touched online. Game thread only
namespace Announcer {
	struct Option {
		const wchar_t*	Label;
		const char*		Value; // Saved value: Game, Auto or the announcer's item ID
	};
	constexpr int GAME = 0;
	constexpr int AUTO = 1;
	constexpr const char* LIVE_KEY = "szAnnouncer"; // [Live] in the state file

	const std::vector<Option>& Options();
	int Picked();
	void Pick(int Index); // Saved, applied right away

	bool IsReady(); // The game's announcer functions were found
	void OnFightStartup(); // Every fight, before it starts
	bool OnLadderReroll(); // Ladder fights re-roll a random announcer: true when ours replaces it
	void OnMenuApplied(void* AudioSettings); // The game applied the player's own setting
}
