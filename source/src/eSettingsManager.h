#pragma once
#include<string>
#include<unordered_map>
#include "..\IniReader.h"

namespace Log {
	enum Level : int {
		None    = 0,   // Production: colored prints only
		Info    = 1,   // Reserved for future use
		Verbose = 2,   // Pattern addresses, function starts
		Debug   = 5,   // Debug breakpoint checks, DEBUGME
		Flood   = 10,  // Per-call spam, respects FloodToggles
		Drown   = 100  // Everything, ignores all toggles
	};
}

class eSettingsManager {
public:
	void Init();

	/** Check if logging should occur at the given level.
	 *  bDebug=true grants up to Log::Debug (not Flood/Drown).
	 *  @param level  Log::None through Log::Drown */
	inline bool ShouldLog(int level) { return (bDebug && level <= Log::Debug) || iLogLevel >= level; }

	/** Check if a specific flood source should print.
	 *  At Log::Drown, ignores the toggle and always prints.
	 *  At Log::Flood, respects the toggle.
	 *  @param toggle  The specific FloodToggles member */
	inline bool ShouldFlood(bool toggle) { return iLogLevel >= Log::Drown || (iLogLevel >= Log::Flood && toggle); }

	/** Check if Floyd dev logs should print (hash inputs, seeds, extracted ids).
	 *  bFloydDebug works on its own, Log::Verbose and up still show them. */
	inline bool ShouldLogFloyd() { return bFloydDebug || ShouldLog(Log::Verbose); }

	/** Check if profile dev logs should print (platform, platform id, save key).
	 *  bProfileDebug works on its own, Log::Verbose and up still show them. */
	inline bool ShouldLogProfile() { return bProfileDebug || ShouldLog(Log::Verbose); }

public:
	// Settings

	bool bEnableKeyboardHotkeys;

	// Debug
	bool bEnableConsoleWindow;
	bool bPauseOnStart;
	int	iLogLevel;
	bool bDebug;
	bool bAllowNonMK;
	bool bFloydDebug;
	bool bProfileDebug;
	struct {
		bool bFName = false;
		bool bEndpoint = false;
		bool bCurl = false;
	} Floods;


	// Toggles
	std::string szFloydTowerMode;
	bool bFloydTowerFromGame; // szFloydTowerMode is Game, otherwise Calculated
	std::string szAttractModeDelay; // Game, Never or seconds

	// Addresses

	// Patterns
	std::string pPakTocCheck;
	std::string pUNameObjGetPat;
	std::string pFPathLoadPat;
	std::string pFPath2LoadPat;
	std::string pFPathCLoadPat;
	std::string pEndpointLoader;
	std::string pProfileGetter;
	std::string pOnlineSubsystem;
	std::string pOnlineCheck;
	// Curl
	std::string pCurlSetOpt;
	std::string pCurlMultiAddHandle;
	std::string pCurlMultiInfoRead;
	// Version
	std::string pNRSClientVersion;
	std::string pBuildVersion;
	std::string pBuildDate;
	std::string pChangelist;
	//Floyd
	std::string pSecretFightCondPat;
	std::string pGetChallengesFromHash;
	std::string pGetFloydHashInputString;
	std::string pGetFloydHashInputString2;
	std::string pGetTacoHashInputString;
	std::string pGetTacoSchedule;
	// Message Box
	std::string pTextFromString;
	std::string pFindText;
	std::string pMessageBoxConstruct;
	std::string pMessageBoxSetTitle;
	std::string pMessageBoxAddButton;
	std::string pMessageBoxShow;
	std::string pMessageBoxClose;
	std::string pMessageBoxDestruct;
	std::string pPlayerInputPressed;
	std::string pPlayerInputUpdate;
	std::string pToastDriverCall;
	std::string pToastEnqueue;
	std::string pLocalPlayerInfo;
	std::string pMessageBoxCallback;
	// Game State
	std::string pMainLoop;
	std::string pFrameTick;
	std::string pOnlineModeCheck;
	std::string pModeManagerGetter;
	// Settings Screen
	std::string pSettingsBuildTabs;
	std::string pSettingsCopyTabs;
	std::string pMenuEntryConstruct;
	std::string pSettingsTabConstruct;
	std::string pSettingsTabDestruct;
	std::string pGrowArray;
	std::string pStringReserve;
	std::string pFindTextPlatform;
	std::string pMemoryFree;
	std::string pMainMenuCopyTabs;
	std::string pSettingsVersionText;
	std::string pSettingsRun;
	// Paks
	std::string pPakMount;
	std::string pConfigInit;
	std::string pConfigGlobals;
	std::string pConfigSetString;
	std::string pConfigGetString;
	std::string pConfigFindFile;
	std::string pLoadFileToString;
	std::string pAttractModeIdle;
	std::string pAttractModeLimit;
	std::string pKeepRunning;
	std::string pFocusLost;
	std::string pFocusLostPause;
	std::string pBootWaitFirst;
	std::string pBootWaitSecond;
	std::string pBootWaitThird;
	std::string pGetStatsContainer;
	std::string pGetProfileStat;
	std::string pGetProfileStatBits;
	std::string pFloydSeed;
	std::string pAnnouncerReroll;
	std::string pAnnouncerMenuApply;
	std::string pApplyAnnouncer;
	std::string pEngineNameConstruct;
	std::string pGetCurrentMission;
	std::string pGetTeam;
	std::string pGetTeamDefinitions;
	std::string pTowersSetup;
	std::string pGetFloydVictories;


	// Menu Section
	std::string hkCheats;
	std::string hkMenu;
	std::string hkInfo;
	int iVKCheats;
	int iVKMenuToggle;
	int iVKMenuInfo;

	//Other
	int iLogSize;
	bool FORCE_CHECK_VER = false;
	std::string szGameVer;
	std::string szModLoader;
	std::string szAntiCheatEngine;
	std::string szCurlSetOpt;
	std::string szCurlPerform;

	//Private Server
	std::string szServerUrl;

};

class eFirstRunManager
{
public:
	void Init();
	void Save();

public:
	bool bPaidModWarned;

private:
	CIniReader* ini;
};

class eCachedPatternsManager
{
private:
	char* HashKey = nullptr;
	CIniReader* ini;
	static __int64 GameAddr;
	std::unordered_map<std::string, uint64_t> Cache;
	bool bAutoFlush = true;

	void FlushEntry(const std::string& key, uint64_t value);

public:
	void Init(uint64_t Hash, const char* version, const char* name = "PatternsCache");
	void Flush();

	// Keyed by the pattern itself (uppercase, single spaces, every wildcard as "?"): editing a pattern scans again,
	// a cosmetic edit doesn't. Patterns found before this was keyed by hook name are scanned once more
	static std::string Key(const std::string& pattern);
	void Set(const std::string& pattern, uint64_t value);
	uint64_t Get(const std::string& pattern);
	bool Has(const std::string& pattern);

	void EnableAutoFlush() { bAutoFlush = true; }
	void DisableAutoFlush() { bAutoFlush = false; }

	~eCachedPatternsManager() { Flush(); if (HashKey) delete[] HashKey; }
};

extern eSettingsManager* SettingsMgr;
extern eFirstRunManager* FirstRunMgr;
extern eCachedPatternsManager* CachedPatternsMgr;