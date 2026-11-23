#include "mkutils.h"
#include "Announcer.h"
#include "Fight.h"
#include "GameState.h"
#include "StateFile.h"
#include "mk12.h"
#include "eSettingsManager.h"
#include "../IniReader.h"
#include <cstring>
#include <cwchar>
#include <string>

namespace Announcer {
	// Fighters with an announcer voice, by the game's fighter name
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

	constexpr uint32_t AUDIO_SETTINGS_VOICE		= 0x68;	// The player's own setting

	static std::vector<Option>	AllOptions;
	static int					Choice				= GAME;
	static const char*			AutoItem			= nullptr; // The fight's fighter's announcer, null without one
	static void*				LastAudioSettings	= nullptr;
	static const char* AutoPick();

	const std::vector<Option>& Options()
	{
		if (AllOptions.empty())
		{
			AllOptions.push_back({ L"GAME", "Game" });
			AllOptions.push_back({ L"AUTO", "Auto" });
			AllOptions.push_back({ L"DEFAULT", DEFAULT_ITEM });
			for (size_t i = 0; i < _countof(VOICES); i++)
				AllOptions.push_back({ VOICES[i].Label, VOICES[i].ItemId });

			CIniReader ini((char*)StateFile::FileName());
			char* Saved = ini.ReadString((char*)StateFile::LIVE, (char*)LIVE_KEY, "Game");
			for (int i = 0; i < (int)AllOptions.size(); i++)
			{
				if (!_stricmp(Saved, AllOptions[i].Value))
					Choice = i;
			}
			if (!_stricmp(Saved, "AutoGameplay")) // AUTO's old fights only twin
				Choice = AUTO;
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

	static const char* Target(bool bMenus)
	{ // Null keeps the player's setting. AUTO leaves menus alone
		if (Choice == GAME || (Choice == AUTO && bMenus))
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

	static void Apply(bool bMenus)
	{
		if (!IsReady() || GameState::IsOnline())
			return;
		const char* Item = Target(bMenus);
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
		CIniReader ini((char*)StateFile::FileName());
		ini.WriteString((char*)StateFile::LIVE, (char*)LIVE_KEY, (char*)Options()[Index].Value);
		bool bInFight = false; // Picked in the pause menu: the fight's fighter right away
		if (Choice == AUTO)
		{
			AutoItem = AutoPick();
			bInFight = AutoItem != nullptr;
		}
		Apply(!bInFight);
	}

	struct HumanSearch {
		MK12::FNameInfoStruct*	Out;
		bool					bFound;
	};

	static bool TakeHumanName(int32_t Team, char* Fighter, void* Context)
	{ // A bad read gives up instead of crashing
		HumanSearch* Search = (HumanSearch*)Context;
		__try
		{
			if (*(uint8_t*)(Fighter + Fight::FIGHTER_AI) != 0)
				return true;
			*Search->Out = *(MK12::FNameInfoStruct*)(Fighter + Fight::FIGHTER_NAME);
			Search->bFound = true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
		return false;
	}

	// The first human player's fighter name, false without one
	static bool ReadFirstHumanName(MK12::FNameInfoStruct* Out)
	{
		HumanSearch Search = { Out, false };
		Fight::ForEachFighter(TakeHumanName, &Search);
		return Search.bFound;
	}

	static const char* ItemFor(const char* Fighter)
	{
		for (size_t i = 0; i < _countof(VOICES); i++)
		{
			if (!_stricmp(VOICES[i].Fighter, Fighter))
				return VOICES[i].ItemId;
		}
		return nullptr;
	}

	static std::string FighterName(MK12::FNameInfoStruct& NameInfo)
	{ // Short, "SubZero"
		MK12::FName* Name = MK12::UNameTable ? MK12::NameTableIndexToFName(&NameInfo) : nullptr;
		if (!Name)
			return "";
		std::string Fighter = Name->ToStr();
		size_t Dot = Fighter.find_last_of('.');
		return Dot == std::string::npos ? Fighter : Fighter.substr(Dot + 1);
	}

	static const char* AutoPick()
	{
		MK12::FNameInfoStruct NameInfo;
		if (!MK12::UNameTable || !ReadFirstHumanName(&NameInfo))
			return nullptr;
		std::string Fighter = FighterName(NameInfo);
		if (Fighter.empty())
			return nullptr;
		if (SettingsMgr->ShouldLog(Log::Verbose))
			printf("Announcer: first human fighter is %s\n", Fighter.c_str());
		return ItemFor(Fighter.c_str());
	}

	void OnFightStartup()
	{
		Options();
		if (Choice == GAME)
			return;
		if (Choice == AUTO)
			AutoItem = AutoPick();
		Apply(false);
	}

	bool OnLadderReroll()
	{
		Options();
		if (!Target(false))
			return false;
		Apply(false);
		return true;
	}

	void OnMenuApplied(void* AudioSettings)
	{
		LastAudioSettings = AudioSettings;
		Options();
		if (Target(true))
			Apply(true);
	}
}
