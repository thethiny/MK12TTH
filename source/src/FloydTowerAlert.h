#pragma once
#include <cstdint>
#include <ctime>

// Tells the player when the Floyd Tower shows up, with the game's message box when Living Towers opens (the game keeps
// running behind it). Only once Floyd was beaten, before that the game has no Floyd Tower.
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

	// Marks all of Floyd's challenges done in this PC's copy of the profile, the game brings Floyd at the end of the next
	// match he can show up in. Game thread. Nothing goes to the server, the game syncs as usual after the Floyd fight
	enum class SummonResult { Summoned, AlreadyDone, NotAvailable, NoProfile };
	bool CanSummon(); // The game's functions were found
	SummonResult Summon();

	void OnTowersEntered(void* TacoSchedule); // Game thread, right after the game set up the towers
	void Refresh(); // Game thread, once the game is up: the Floyd stats read again from the local profile
	void Tick(); // Game thread, every frame: shows it once the screen is up
}
