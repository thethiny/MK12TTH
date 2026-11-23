#include "mkutils.h"
#include "GameState.h"
#include "mk12.h"
#include <Windows.h>

namespace GameState {
	constexpr uint8_t	PHASE_LOADING		= 4; // Set before every asset load, set back to 3 or 0 after
	constexpr ULONGLONG	LOADING_GRACE_MS	= 1500; // The loading screen can outlast the load itself

	static ULONGLONG LastLoadingTick = 0;
	static bool bTicked = false;
	static const char* LastBlocker = "starting";
	static bool bMainMenuUp = false;
	static uint32_t MainMenuCount = 0;

	bool IsReady()
	{
		return MK12::GetModeManager && MK12::IsModeOnStack && MK12::GameModeOnlineClass && MK12::lpGamePhaseOwner;
	}

	static void* ModeManager()
	{
		return IsReady() ? MK12::GetModeManager(nullptr) : nullptr;
	}

	bool HasModeManager()
	{
		return ModeManager() != nullptr;
	}

	bool IsOnline()
	{
		void* Manager = ModeManager();
		return Manager && MK12::IsModeOnStack(Manager, MK12::GameModeOnlineClass());
	}

	static int GamePhase()
	{
		uint8_t* Owner = IsReady() ? *(uint8_t**)MK12::lpGamePhaseOwner : nullptr;
		return Owner ? Owner[MK12::GamePhaseOffset] : -1;
	}

	static bool IsLoadingPhase()
	{
		return GamePhase() == PHASE_LOADING;
	}

	bool IsLoading()
	{
		return IsLoadingPhase() || (LastLoadingTick && GetTickCount64() - LastLoadingTick < LOADING_GRACE_MS);
	}

	// Why UI can't show right now, or null when it can
	static const char* Blocker()
	{
		if (!IsReady())
			return "not ready";
		if (!HasModeManager())
			return "no game world yet";
		if (IsLoading())
			return "loading";
		if (IsOnline())
			return "online";
		return nullptr;
	}

	bool CanShowUI()
	{
		return Blocker() == nullptr;
	}

	bool IsMainMenuUp()
	{
		return bMainMenuUp;
	}

	uint32_t MainMenuEntries()
	{
		return MainMenuCount;
	}

	void Tick()
	{
		if (!bTicked)
		{
			bTicked = true;
			if (SettingsMgr->ShouldLog(Log::Verbose))
				printf("Game State: Ticking\n");
		}

		if (IsLoadingPhase())
			LastLoadingTick = GetTickCount64();

		// Only the main menu has the game's notification driver
		bool bUp = CanShowUI() && MK12::ToastDriver && MK12::ToastDriver();
		if (bUp && !bMainMenuUp)
		{
			MainMenuCount++;
			if (SettingsMgr->ShouldLog(Log::Verbose))
				printf("Game State: Main menu up\n");
		}
		bMainMenuUp = bUp;

		if (SettingsMgr->ShouldLog(Log::Verbose))
		{ // Only on change, this runs every frame
			const char* Now = Blocker();
			if (Now != LastBlocker)
			{
				printf("Game State: %s (phase %d)\n", Now ? Now : "UI can show", GamePhase());
				LastBlocker = Now;
			}
		}
	}
}
