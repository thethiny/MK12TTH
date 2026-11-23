#include "Fight.h"
#include "mk12.h"
#include <Windows.h>

namespace Fight {
	// Where the fight keeps its fighters
	constexpr uint32_t MISSION_GAME_INFO		= 0x60;	// Then +0x10

	static char* TeamFighter(void* GameInfo, int32_t Team)
	{ // A bad read gives up instead of crashing
		__try
		{
			char* TeamInfo = (char*)MK12::GetTeam(GameInfo, Team);
			char* Definitions = TeamInfo ? (char*)MK12::GetTeamDefinitions(TeamInfo) : nullptr;
			if (!Definitions)
				return nullptr;
			if (Definitions == TeamInfo + TEAM_KAMEO_COMPOSITION || Definitions == TeamInfo + TEAM_TAG_COMPOSITION
				|| Definitions == TeamInfo + TEAM_SINGLE_COMPOSITION)
				return Definitions + COMPOSITION_FIRST;
			if (*(int32_t*)(Definitions + COMPOSITION_FIRST + 8) > 0) // Endurance: the list's first
				return *(char**)(Definitions + COMPOSITION_FIRST);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
		return nullptr;
	}

	static void* CurrentGameInfo()
	{
		__try
		{
			char* Mission = (char*)MK12::GetCurrentMission(nullptr);
			if (Mission && *(char**)(Mission + MISSION_GAME_INFO))
				return *(void**)(*(char**)(Mission + MISSION_GAME_INFO) + 0x10);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
		return nullptr;
	}

	bool ForEachFighter(FighterVisitor* Visit, void* Context)
	{
		if (!MK12::GetCurrentMission || !MK12::GetTeam || !MK12::GetTeamDefinitions)
			return false;
		void* GameInfo = CurrentGameInfo();
		if (!GameInfo)
			return false;
		for (int32_t Team = 0; Team < 2; Team++)
		{
			char* Fighter = TeamFighter(GameInfo, Team);
			if (Fighter && !Visit(Team, Fighter, Context))
				break;
		}
		return true;
	}
}
