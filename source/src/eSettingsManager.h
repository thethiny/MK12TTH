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

public:
	// Settings

	bool bEnableKeyboardHotkeys;

	// Debug
	bool bEnableConsoleWindow;
	bool bPauseOnStart;
	int	iLogLevel;
	bool bDebug;
	bool bAllowNonMK;
	struct {
		bool bFName = false;
		bool bEndpoint = false;
		bool bCurl = false;
	} Floods;


	// Toggles
	bool bDisableSignatureCheck;
	bool bDisableSignatureWarn;
	bool bDisableTOCSigCheck;
	bool bDisableChunkSigCheck;
	bool bDisablePakTOCCheck;
	bool bPatchCurl;
	bool bFNameToStrHook;
	bool bUNameGetter;
	bool bGetFightMetadata;
	bool bSerializeSecretFights;
	bool bEnableStringSwap;
	bool bEnableFloydTracking;
	bool bEnableProfileGetter;
	bool bEnableVersionInfo;

	// Addresses

	// Patterns
	std::string pSigCheck;
	std::string pSigWarn;
	std::string pTocCheck;
	std::string pPakTocCheck;
	std::string pChunkSigCheck;
	std::string pChunkSigCheckFunc;
	std::string pUNameObjGetPat;
	std::string pFPathLoadPat;
	std::string pFPath2LoadPat;
	std::string pFPathCLoadPat;
	std::string pEndpointLoader;
	std::string pProfileGetter;
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
	bool bEnableServerProxy;

	// Announcer Mod
	struct {
		bool bEnable;
		std::string szDefault;
		std::string szLiuKang;
		std::string szGeras;
		std::string szJohnnyCage;
		std::string szShangTsung;
		std::string szSindel;
		std::string szSubZero;
		std::string szOmniMan;
	} AnnouncerSwap;

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

	void Set(const std::string& key, uint64_t value);
	uint64_t Get(const std::string& key);
	bool Has(const std::string& key);

	void EnableAutoFlush() { bAutoFlush = true; }
	void DisableAutoFlush() { bAutoFlush = false; }

	~eCachedPatternsManager() { Flush(); if (HashKey) delete[] HashKey; }
};

extern eSettingsManager* SettingsMgr;
extern eFirstRunManager* FirstRunMgr;
extern eCachedPatternsManager* CachedPatternsMgr;