#pragma once
#include <cstdint>
#include <ctime>

// When the Floyd Tower shows up, in a toast once back on the main menu after opening Living Towers (only the main menu
// has toasts). Only once Floyd was beaten, the game has no Floyd Tower before that. Opening Living Towers also reads the
// Floyd stats and the player's 10 Floyd challenges
namespace FloydTowerAlert {
	// Floyd profile stats
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

	// Game thread
	void OnTowersEntered(void* TacoSchedule); // Right after the game set up the towers
	void Tick();
}
