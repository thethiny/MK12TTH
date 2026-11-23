#pragma once
#include <cstdint>
#include <vector>

namespace MK12 { class FString; }

// The announcer voice, picked in MODS: GAME (the player's setting), AUTO (the first human player's fighter's announcer,
// when there is one) or one announcer. Applied as fights start and as the main menu loads, so menus use it too. Never
// online. Game thread only
namespace Announcer {
	struct Option {
		const wchar_t*	Label;
		const char*		Value; // Game, Auto or the announcer's item ID
	};
	constexpr int GAME = 0;
	constexpr int AUTO = 1;
	constexpr const char* LIVE_KEY = "szAnnouncer"; // [Live] in the state file

	const std::vector<Option>& Options();
	int Picked();
	void Pick(int Index); // Saved and applied

	bool IsReady();
	void OnFightStartup(); // Before every fight
	bool OnLadderReroll(); // Ladder fights pick a random announcer: true when ours replaces it
	void OnMenuApplied(void* AudioSettings); // The game applied the player's setting
}
