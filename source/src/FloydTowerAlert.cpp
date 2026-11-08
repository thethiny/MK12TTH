#include "mkutils.h"
#include "FloydTowerAlert.h"
#include "FloydTower.h"
#include "GameToast.h"
#include "Features.h"
#include "mk12_enums.h"
#include "mk12.h"
#include "mk12hook.h"
#include "eSettingsManager.h"
#include <ctime>
#include <string>

namespace FloydTowerAlert {
	static bool bPending = false; // Entered Living Towers, shown once the screen is up
	static FloydStats Stats;

	// The Floyd profile stats, in Stat order
	static const MK12::Enums::MKProfileStatID STAT_IDS[] = {
		MK12::Enums::MKProfileStatID::ProfileStat9001, MK12::Enums::MKProfileStatID::ProfileStat9002, MK12::Enums::MKProfileStatID::ProfileStat9003,
		MK12::Enums::MKProfileStatID::ProfileStat9004, MK12::Enums::MKProfileStatID::ProfileStat9005, MK12::Enums::MKProfileStatID::ProfileStat9006,
		MK12::Enums::MKProfileStatID::ProfileStat9100, MK12::Enums::MKProfileStatID::ProfileStat9101, MK12::Enums::MKProfileStatID::ProfileStat9102,
		MK12::Enums::MKProfileStatID::ProfileStat9103, MK12::Enums::MKProfileStatID::ProfileStat9104, MK12::Enums::MKProfileStatID::ProfileStat9105,
		MK12::Enums::MKProfileStatID::ProfileStat9106 };
	static_assert(sizeof(STAT_IDS) / sizeof(STAT_IDS[0]) == (size_t)Stat::Count, "One id per stat");

	// Stats the game keeps as bit sets, one bit per challenge or chapter step
	static bool IsBitStat(Stat Which)
	{
		return Which == Stat::ChallengesDone || Which == Stat::Chapter15;
	}

	static void ReadStats(void* Profile)
	{
		if (!MK12::GetStatsContainer || !MK12::GetProfileStat)
			return;
		void* Container = MK12::GetStatsContainer(Profile);
		if (!Container)
			return;
		for (size_t i = 0; i < (size_t)Stat::Count; i++)
		{
			if (!IsBitStat((Stat)i))
				Stats.Values[i] = MK12::GetProfileStat(Container, MK12::ProfileStatScope, (int32_t)STAT_IDS[i]);
			else if (MK12::GetProfileStatBits)
				Stats.Values[i] = MK12::GetProfileStatBits(Container, MK12::ProfileStatBitsScope, (int32_t)STAT_IDS[i]);
		}
		Stats.bRead = true;
		Stats.ReadAt = time(nullptr);
	}

	// The same seed the game picks Floyd's challenges with: the one it kept, else the profile hash and encounters.
	// Working out the hash also reads the online ids for PROFILE
	static void ReadChallenges(void* Profile)
	{
		if (!MK12::FloydProfileHash || !Features::IsActive(Features::Id::FloydTracking))
			return;
		uint32_t Hash = MK12::FloydProfileHash(&Profile);
		uint64_t Seed = MK12::FloydStoredSeed(Profile);
		bool bKept = Seed != 0;
		if (!bKept)
			Seed = ((uint64_t)Hash << 32) ^ (uint64_t)(int64_t)(int32_t)Stats.Values[(size_t)Stat::Encounters];

		auto& Floyd = HookMetadata::CurrentFloydInfo;
		uint32_t Encounters = (uint32_t)Seed;
		if (Floyd.HasClues && Encounters <= Floyd.FloydEncounters)
			return; // The game's own read wins
		Floyd.Clues = FloydTower::FloydChallenges(Seed);
		Floyd.UserProfileHash = (uint32_t)(Seed >> 32);
		Floyd.FloydEncounters = Encounters;
		Floyd.HasClues = true;
		if (SettingsMgr->ShouldLogFloyd())
			printf("Floyd Seed: %llX (%s)\n", Seed, bKept ? "kept by the game" : "from the profile");
	}

	const FloydStats& LastStats()
	{
		return Stats;
	}

	bool IsChallengeDone(const FloydStats& Stats, uint8_t Challenge)
	{
		return Challenge >= 1 && Challenge <= 64 && (Stats.Values[(size_t)Stat::ChallengesDone] >> (Challenge - 1)) & 1;
	}

	static std::wstring Clock(time_t Seconds)
	{
		long long Minutes = (Seconds + 59) / 60;
		wchar_t Text[16];
		swprintf_s(Text, L"%02lld:%02lld", Minutes / 60, Minutes % 60);
		return Text;
	}

	static bool IsOn()
	{
		return Features::IsActive(Features::Id::FloydTowerAlert) && Features::IsActive(Features::Id::FloydTracking);
	}

	void OnTowersEntered(void* TacoSchedule)
	{
		if (!TacoSchedule || !Features::IsInstalled(Features::Id::FloydTowerAlert) || !MK12::GetFloydVictories)
			return;

		void* Profile = MK12::MapModeGetProfile((char*)TacoSchedule - MK12::MapModeTacoScheduleOffset);
		if (!Profile)
			return;
		ReadStats(Profile);
		ReadChallenges(Profile);
		if (!IsOn())
			return;
		int32_t Victories = MK12::GetFloydVictories(Profile);
		if (SettingsMgr->ShouldLogFloyd())
			printf("Floyd Victories: %d\n", Victories);
		bPending = Victories >= 1; // The game has no Floyd Tower before Floyd was beaten
	}

	void Tick()
	{
		// Only the main menu has toasts: waits there, then says how it is right now
		if (!bPending || !GameToast::IsUp())
			return;
		bPending = false;
		if (!IsOn())
			return;

		time_t Now = time(nullptr);
		FloydTower::Window window;
		if (FloydTower::Find(Now, HookMetadata::CurrentTacoInfo.UserProfileHash, SettingsMgr->bFloydTowerFromGame, window) != FloydTower::Status::Ready)
			return;
		bool bUp = window.Start <= Now;
		GameToast::Show(L"Floyd Tower", bUp ? L"Up now, ends in " + Clock(window.End - Now) : L"In " + Clock(window.Start - Now));
	}
}
