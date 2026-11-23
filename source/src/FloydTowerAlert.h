#pragma once
#include <cstdint>
#include <ctime>

// When the Floyd Tower shows up, in the game's message box as Living Towers opens. Only once Floyd was beaten, the game
// has no Floyd Tower before that. Opening Living Towers also reads the Floyd stats
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

	// Marks all of Floyd's challenges done in this PC's copy of the profile, Floyd shows up at the end of the next match
	// he can. Nothing goes to the server, the game syncs as usual after the Floyd fight. Game thread
	enum class SummonResult { Summoned, AlreadyDone, NotAvailable, NoProfile };
	bool CanSummon();
	SummonResult Summon();

	// Game thread
	void OnTowersEntered(void* TacoSchedule); // Right after the game set up the towers
	void Refresh(); // Reads the stats again, once the game is up
	void Tick();
}
