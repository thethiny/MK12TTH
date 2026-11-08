#pragma once
#include <cstdint>
#include <ctime>

// Tells the player when the Floyd Tower shows up, with a toast once they're back in the main menu after opening
// Living Towers (the game only has toasts there). Only once Floyd was beaten, before that the game has no Floyd Tower.
// Opening Living Towers also reads the Floyd stats and the player's 10 Floyd challenges
namespace FloydTowerAlert {
	// Floyd profile stats, read from the player's profile each time Living Towers opens
	enum class Stat {
		Stat9001, Encounters, ChallengesDone, LastBattle, Victories, MatchesSinceSeen,
		FatalityTracker, AnimalityTracker, ShaolinMonks, DoorBuster, Chapter15, TowersPoints, QuestsDone,
		Count
	};
	struct FloydStats {
		bool		bRead = false;
		time_t		ReadAt = 0;
		uint64_t	Values[(size_t)Stat::Count] = {};
	};
	const FloydStats& LastStats();
	bool IsChallengeDone(const FloydStats& Stats, uint8_t Challenge); // Numbered from 1

	void OnTowersEntered(void* TacoSchedule); // Game thread, right after the game set up the towers
	void Tick(); // Game thread, every frame: shows it once the screen is up
}
