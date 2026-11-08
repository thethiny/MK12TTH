#include "mkutils.h"
#include "FloydFight.h"
#include "StateFile.h"
#include "eSettingsManager.h"
#include "../IniReader.h"
#include <string>

namespace FloydFight {
	constexpr int		MAX_LEVEL		= 19; // The game's highest AI level, higher is harder
	constexpr int		MAX_RETRIES		= 10;

	static std::vector<Option> Levels;
	static std::vector<Option> Retries;
	static std::vector<std::wstring> Labels; // Backing text for the options
	static std::vector<std::string> Values;
	static int Level = GAME;
	static int RetryChoice = GAME;

	static bool bTowerPending = false; // SPAWN FLOYD TOWER NOW pressed, for the next time Living Towers opens. Never saved

	static uint32_t* GameRetries = nullptr;
	static uint32_t GameRetriesOwn = 0; // The game's value before we wrote ours
	static bool bRetriesWritten = false; // GAME never writes: it only gives the game's value back after ours

	static int ReadChoice(const char* Key, const std::vector<Option>& Options)
	{
		CIniReader ini((char*)StateFile::FILE_NAME);
		char* Saved = ini.ReadString((char*)StateFile::LIVE, (char*)Key, (char*)Options[GAME].Value);
		int Picked = GAME;
		for (int i = 0; i < (int)Options.size(); i++)
		{
			if (!_stricmp(Saved, Options[i].Value))
				Picked = i;
		}
		delete[] Saved;
		return Picked;
	}

	static void Build()
	{
		if (!Levels.empty())
			return;
		// Text first, the options point into it: 1 to 19 serve both lists
		int Most = MAX_LEVEL > MAX_RETRIES ? MAX_LEVEL : MAX_RETRIES;
		Labels.reserve(Most);
		Values.reserve(Most);
		for (int i = 1; i <= Most; i++)
		{
			Labels.push_back(std::to_wstring(i));
			Values.push_back(std::to_string(i));
		}

		Levels.push_back({ L"GAME", "Game" });
		for (int i = 0; i < MAX_LEVEL; i++)
			Levels.push_back({ Labels[i].c_str(), Values[i].c_str() });
		Retries.push_back({ L"GAME", "Game" });
		Retries.push_back({ L"NONE", "0" });
		for (int i = 0; i < MAX_RETRIES; i++)
			Retries.push_back({ Labels[i].c_str(), Values[i].c_str() });
		Retries.push_back({ L"99", "99" });

		Level = ReadChoice(LEVEL_KEY, Levels);
		RetryChoice = ReadChoice(RETRIES_KEY, Retries);
	}

	const std::vector<Option>& LevelOptions()
	{
		Build();
		return Levels;
	}

	const std::vector<Option>& RetryOptions()
	{
		Build();
		return Retries;
	}

	int PickedLevel()
	{
		Build();
		return Level;
	}

	int PickedRetries()
	{
		Build();
		return RetryChoice;
	}

	static void Save(const char* Key, const char* Value)
	{
		CIniReader ini((char*)StateFile::FILE_NAME);
		ini.WriteString((char*)StateFile::LIVE, (char*)Key, (char*)Value);
	}

	// The game reads it as a match ends and in the pause menu. GAME leaves it alone, or gives back the game's own
	// value once after ours
	static void ApplyRetries()
	{
		if (!GameRetries)
			return;
		if (RetryChoice == GAME)
		{
			if (bRetriesWritten)
				*GameRetries = GameRetriesOwn;
			bRetriesWritten = false;
			return;
		}
		if (!bRetriesWritten)
			GameRetriesOwn = *GameRetries;
		bRetriesWritten = true;
		*GameRetries = (uint32_t)atoi(Retries[RetryChoice].Value);
	}

	void PickLevel(int Index)
	{
		Build();
		if (Index < 0 || Index >= (int)Levels.size())
			return;
		Level = Index;
		Save(LEVEL_KEY, Levels[Index].Value);
	}

	void PickRetries(int Index)
	{
		Build();
		if (Index < 0 || Index >= (int)Retries.size())
			return;
		RetryChoice = Index;
		Save(RETRIES_KEY, Retries[Index].Value);
		ApplyRetries();
	}

	bool IsReady()
	{
		return GameRetries != nullptr;
	}

	void SetRetryLimit(uint32_t* Limit)
	{
		Build();
		GameRetries = Limit;
		ApplyRetries();
	}

	void SpawnTower()
	{
		bTowerPending = true;
	}

	bool MoveTowerSlot(int64_t NowTicks, int64_t GameStart, int64_t GameEnd, int64_t& Start, int64_t& End)
	{
		if (!bTowerPending || GameEnd <= GameStart)
			return false;
		// Once: this visit to Living Towers only, the next one gets the game's own schedule back
		bTowerPending = false;
		Start = NowTicks;
		End = NowTicks + (GameEnd - GameStart);
		if (SettingsMgr->ShouldLogFloyd())
			printf("Floyd Fight: the Floyd Tower slot moved to now\n");
		return true;
	}

	bool OverrideLevel(uint8_t GameLevel, uint8_t& Out)
	{
		Build();
		if (Level == GAME)
			return false; // Passthrough: the game's own call, untouched
		ApplyRetries(); // In case something reloaded the game's own
		Out = (uint8_t)atoi(Levels[Level].Value);
		if (SettingsMgr->ShouldLogFloyd())
			printf("Floyd Fight: the game picked level %d, Floyd fights at level %d\n", GameLevel, Out);
		return true;
	}
}
