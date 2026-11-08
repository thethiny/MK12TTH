#include "mkutils.h"
#include "FloydTowerAlert.h"
#include "FloydTower.h"
#include "GameMessageBox.h"
#include "GameToast.h"
#include "GameState.h"
#include "OnlineIds.h"
#include "Features.h"
#include "mk12_enums.h"
#include "mk12.h"
#include "mk12hook.h"
#include "eSettingsManager.h"
#include <ctime>
#include <string>

namespace FloydTowerAlert {
	static bool bPending = false; // Entered Living Towers, shown once the game can show it
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

	void Refresh()
	{
		// The game's profiles don't exist until it's up: never at startup
		if (!Features::IsInstalled(Features::Id::FloydTowerAlert) || !GameState::CanShowUI())
			return;
		void* Profile = OnlineIds::LocalProfile();
		if (!Profile)
			return;
		ReadStats(Profile);
	}

	bool CanSummon()
	{
		return MK12::GetProfileStatBits && MK12::GetStatsContainer && MK12::FloydProfileHash && OnlineIds::HasOnlineCheck();
	}

	SummonResult Summon()
	{
		if (!CanSummon())
			return SummonResult::NotAvailable;
		void* Profile = OnlineIds::LocalProfile();
		void* Container = Profile ? MK12::GetStatsContainer(Profile) : nullptr;
		if (!Container)
			return SummonResult::NoProfile;
		ReadStats(Profile);

		// Every challenge, not only the 10 read last: those can be out of date, the game checks its own list
		uint64_t Done = MK12::GetProfileStatBits(Container, MK12::ProfileStatBitsScope, (int32_t)STAT_IDS[(size_t)Stat::ChallengesDone]);
		uint64_t All = Done | ((1ull << FloydTower::CHALLENGE_COUNT) - 1);
		if (All == Done)
			return SummonResult::AlreadyDone;
		if (!MK12::SetProfileStatLocal(Container, MK12::ProfileStatBitsScope, (int32_t)STAT_IDS[(size_t)Stat::ChallengesDone], All))
			return SummonResult::NotAvailable;
		ReadStats(Profile);
		if (SettingsMgr->ShouldLogFloyd())
			printf("Floyd Summoned: challenges done %llX -> %llX\n", Done, All);
		return SummonResult::Summoned;
	}

	const FloydStats& LastStats()
	{
		return Stats;
	}

	bool IsChallengeDone(const FloydStats& Stats, uint8_t Challenge)
	{
		return Challenge >= 1 && Challenge <= 64 && (Stats.Values[(size_t)Stat::ChallengesDone] >> (Challenge - 1)) & 1;
	}

	// "1 hour 5 minutes", "26 minutes", rounded up to the minute
	static std::wstring Words(time_t Seconds)
	{
		long long Minutes = (Seconds + 59) / 60;
		long long Hours = Minutes / 60;
		Minutes %= 60;
		auto Part = [](long long Count, const wchar_t* Unit) { return std::to_wstring(Count) + L" " + Unit + (Count == 1 ? L"" : L"s"); };
		if (!Hours)
			return Part(Minutes ? Minutes : 1, L"minute");
		return Minutes ? Part(Hours, L"hour") + L" " + Part(Minutes, L"minute") : Part(Hours, L"hour");
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
		if (!IsOn())
			return;
		int32_t Victories = MK12::GetFloydVictories(Profile);
		if (SettingsMgr->ShouldLogFloyd())
			printf("Floyd Victories: %d\n", Victories);
		bPending = Victories >= 1; // The game has no Floyd Tower before Floyd was beaten
	}

	void Tick()
	{
		// A full screen message over Living Towers that doesn't pause the game. Without the game's message box, a toast
		// once back on the main menu (the only screen with toasts)
		bool bBox = GameMessageBox::IsReady();
		if (!bPending || !GameState::CanShowUI() || (!bBox && !GameToast::IsUp()))
			return;
		bPending = false;
		if (!IsOn())
			return;

		time_t Now = time(nullptr);
		FloydTower::Window window;
		if (FloydTower::Find(Now, HookMetadata::CurrentTacoInfo.UserProfileHash, SettingsMgr->bFloydTowerFromGame, window) != FloydTower::Status::Ready)
			return;
		bool bUp = window.Start <= Now;
		if (!bBox)
		{
			GameToast::Show(L"Floyd Tower", bUp ? L"Up now, ends in " + Words(window.End - Now) : L"In " + Words(window.Start - Now));
			return;
		}
		std::wstring Body = bUp ? L"The Floyd Tower is up now in Living Towers.\n\nIt ends in " + Words(window.End - Now) + L"."
			: L"The Floyd Tower shows up in Living Towers in " + Words(window.Start - Now) + L".";
		GameMessageBox::Confirm(Body, nullptr, L"FLOYD TOWER");
	}
}
