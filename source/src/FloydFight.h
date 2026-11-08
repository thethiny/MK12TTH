#pragma once
#include <cstdint>
#include <vector>

// Floyd's fight, picked in MODS, LIVE FEATURES: his AI level, and how many times you can retry
// after losing to him before he leaves. GAME keeps the game's own (level 13, lower after each loss; 2 retries).
// Saved for next time. Game thread only
namespace FloydFight {
	struct Option {
		const wchar_t*	Label;
		const char*		Value; // Saved value
	};
	constexpr int GAME = 0; // First choice of both lists
	constexpr const char* LEVEL_KEY = "szFloydLevel"; // [Live] in the state file
	constexpr const char* RETRIES_KEY = "szFloydRetries";

	const std::vector<Option>& LevelOptions(); // GAME, then the game's AI levels 1 to 19 (higher is harder)
	const std::vector<Option>& RetryOptions(); // GAME, NONE, 1 to 10, 99
	int PickedLevel();
	int PickedRetries();
	void PickLevel(int Index); // Saved, used from Floyd's next fight (a retry included)
	void PickRetries(int Index); // Saved, applied right away
	void SpawnTower(); // SPAWN FLOYD TOWER NOW: once, the next time Living Towers opens. Gone with a restart

	// The game just built today's Floyd Tower slot (UTC ticks). After a press: true once, and the slot to use instead,
	// starting now for as long as the game's own lasts. The game rebuilds it on the next visit to Living Towers
	bool MoveTowerSlot(int64_t NowTicks, int64_t GameStart, int64_t GameEnd, int64_t& Start, int64_t& End);

	bool IsReady(); // The game's level and retry hooks are in
	void SetRetryLimit(uint32_t* GameRetries); // The game's retry limit, once at install
	// The level the game picked for this attempt: false for GAME (left alone), else the picked level in Out
	bool OverrideLevel(uint8_t GameLevel, uint8_t& Out);
}
