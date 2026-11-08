#include "mkutils.h"
#include "Announcer.h"
#include "GameState.h"
#include "StateFile.h"
#include "mk12.h"
#include "eSettingsManager.h"
#include "../IniReader.h"
#include <cstring>
#include <cwchar>
#include <string>

namespace Announcer {
	// Each fighter with an announcer voice, by the name the game gives the fighter
	struct Voice {
		const char*		Fighter;
		const wchar_t*	Label;
		const char*		ItemId;
	};
	static const Voice VOICES[] = {
		{ "Geras",		L"GERAS",			"GerasAnnouncer" },
		{ "JohnnyCage",	L"JOHNNY CAGE",		"JohnnyCageAnnouncer" },
		{ "LiuKang",	L"LIU KANG",		"LiuKangAnnouncer" },
		{ "ShangTsung",	L"SHANG TSUNG",		"ShangTsungAnnouncer" },
		{ "Sindel",		L"SINDEL",			"SindelAnnouncer" },
		{ "SubZero",	L"SUB-ZERO",		"SubZeroAnnouncer" },
		{ "OmniMan",	L"OMNI-MAN",		"OmniManAnnouncer" },
		{ "Mileena",	L"MILEENA",			"MileenaAnnouncer" },
		{ "Havik",		L"HAVIK",			"HavikAnnouncer" },
		{ "NoobSaibot",	L"NOOB SAIBOT",		"NoobSaibotAnnouncer" },
		{ "Ghostface",	L"GHOSTFACE",		"GhostfaceAnnouncer" },
		{ "Conan",		L"CONAN",			"ConanAnnouncer" },
		{ "T1000",		L"T-1000",			"T1000Announcer" },
	};
	constexpr const char* DEFAULT_ITEM = "DefaultAnnouncer";

	// Where the fight keeps its fighters
	constexpr uint32_t MISSION_GAME_INFO		= 0x60;	// Then +0x10
	constexpr uint32_t TEAM_SINGLE_DEFINITION	= 0x90;
	constexpr uint32_t TEAM_KAMEO_DEFINITION	= 0x410;
	constexpr uint32_t DEFINITION_FIGHTER		= 0x08;	// Single and Kameo teams. Tag teams keep a list there
	constexpr uint32_t FIGHTER_NAME				= 0xE0;
	constexpr uint32_t FIGHTER_AI				= 0x118;	// 0 is a human player
	constexpr uint32_t AUDIO_SETTINGS_VOICE		= 0x68;	// The player's own setting

	static std::vector<Option> AllOptions;
	static int Choice = GAME;
	static const char* AutoItem = nullptr; // The last fight's pick for AUTO
	static void* LastAudioSettings = nullptr;

	const std::vector<Option>& Options()
	{
		if (AllOptions.empty())
		{
			AllOptions.push_back({ L"GAME", "Game" });
			AllOptions.push_back({ L"AUTO", "Auto" });
			AllOptions.push_back({ L"DEFAULT", DEFAULT_ITEM });
			for (const Voice& voice : VOICES)
				AllOptions.push_back({ voice.Label, voice.ItemId });

			CIniReader ini((char*)StateFile::FILE_NAME);
			char* Saved = ini.ReadString((char*)StateFile::LIVE, (char*)LIVE_KEY, "Game");
			for (int i = 0; i < (int)AllOptions.size(); i++)
			{
				if (!_stricmp(Saved, AllOptions[i].Value))
					Choice = i;
			}
			delete[] Saved;
		}
		return AllOptions;
	}

	bool IsReady()
	{
		return MK12::ApplyAnnouncer && MK12::EngineNameConstruct;
	}

	int Picked()
	{
		Options();
		return Choice;
	}

	// The item to apply, null to leave the player's own setting
	static const char* Target()
	{
		if (Choice == GAME)
			return nullptr;
		if (Choice == AUTO)
			return AutoItem;
		return Options()[Choice].Value;
	}

	static void RestorePlayerSetting()
	{
		if (LastAudioSettings && MK12::AnnouncerSetter)
			MK12::AnnouncerSetter(LastAudioSettings, (MK12::FString*)((char*)LastAudioSettings + AUDIO_SETTINGS_VOICE));
	}

	static void Apply()
	{
		if (!IsReady() || GameState::IsOnline())
			return;
		const char* Item = Target();
		if (!Item)
		{
			RestorePlayerSetting();
			return;
		}
		std::wstring Wide(Item, Item + strlen(Item));
		uint64_t Name = 0;
		MK12::EngineNameConstruct(&Name, Wide.c_str(), 1);
		MK12::ApplyAnnouncer(0, &Name, false);
	}

	void Pick(int Index)
	{
		if (Index < 0 || Index >= (int)Options().size())
			return;
		Choice = Index;
		CIniReader ini((char*)StateFile::FILE_NAME);
		ini.WriteString((char*)StateFile::LIVE, (char*)LIVE_KEY, (char*)Options()[Index].Value);
		Apply();
	}

	// The first human player's fighter name, empty without one. Reads the fight's own structures, a bad read
	// gives up instead of crashing
	static bool ReadFirstHumanName(MK12::FNameInfoStruct* Out)
	{
		if (!MK12::GetCurrentMission || !MK12::GetTeam || !MK12::GetTeamDefinitions)
			return false;
		__try
		{
			char* Mission = (char*)MK12::GetCurrentMission(nullptr);
			if (!Mission || !*(char**)(Mission + MISSION_GAME_INFO))
				return false;
			void* GameInfo = *(void**)(*(char**)(Mission + MISSION_GAME_INFO) + 0x10);
			for (int Team = 0; Team < 2; Team++)
			{
				char* TeamInfo = (char*)MK12::GetTeam(GameInfo, Team);
				char* Definitions = TeamInfo ? (char*)MK12::GetTeamDefinitions(TeamInfo) : nullptr;
				if (!Definitions)
					continue;
				char* Fighter = nullptr;
				if (Definitions == TeamInfo + TEAM_SINGLE_DEFINITION || Definitions == TeamInfo + TEAM_KAMEO_DEFINITION)
					Fighter = Definitions + DEFINITION_FIGHTER;
				else if (*(int32_t*)(Definitions + DEFINITION_FIGHTER + 8) > 0)
					Fighter = *(char**)(Definitions + DEFINITION_FIGHTER);
				if (Fighter && *(uint8_t*)(Fighter + FIGHTER_AI) == 0)
				{
					*Out = *(MK12::FNameInfoStruct*)(Fighter + FIGHTER_NAME);
					return true;
				}
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
		return false;
	}

	static const char* AutoPick()
	{
		MK12::FNameInfoStruct NameInfo;
		if (!MK12::UNameTable || !ReadFirstHumanName(&NameInfo))
			return nullptr;
		MK12::FName* Name = MK12::NameTableIndexToFName(&NameInfo);
		if (!Name)
			return nullptr;
		std::string Fighter = Name->ToStr();
		size_t Dot = Fighter.find_last_of('.');
		if (Dot != std::string::npos)
			Fighter = Fighter.substr(Dot + 1);
		if (SettingsMgr->ShouldLog(Log::Verbose))
			printf("Announcer: first human fighter is %s\n", Fighter.c_str());
		for (const Voice& voice : VOICES)
		{
			if (!_stricmp(voice.Fighter, Fighter.c_str()))
				return voice.ItemId;
		}
		return nullptr;
	}

	void OnFightStartup()
	{
		Options();
		if (Choice == GAME)
			return;
		if (Choice == AUTO)
			AutoItem = AutoPick();
		Apply();
	}

	bool OnLadderReroll()
	{
		Options();
		if (!Target())
			return false;
		Apply();
		return true;
	}

	void OnMenuApplied(void* AudioSettings)
	{
		LastAudioSettings = AudioSettings;
		Options();
		if (Target())
			Apply();
	}
}
