#include "eSettingsManager.h"
#include "StateFile.h"
#include <Windows.h>

eSettingsManager*		SettingsMgr			= new eSettingsManager();
eFirstRunManager*		FirstRunMgr			= new eFirstRunManager;
eCachedPatternsManager*	CachedPatternsMgr	= new eCachedPatternsManager;
__int64 eCachedPatternsManager::GameAddr = reinterpret_cast<__int64>(GetModuleHandle(nullptr));

void eCachedPatternsManager::Init(uint64_t Hash, const char* version, const char* name)
{
	std::string cacheFile = std::string(name) + ".cache";
	ini = new CIniReader((char*)cacheFile.c_str());
	if (Hash)
	{
		size_t totalLen = 8 + 1 + strlen(version) + 1;
		char* hashStr = new char[totalLen];
		sprintf_s(hashStr, totalLen, "%08X.%s", (uint32_t)(Hash >> 32), version);
		eCachedPatternsManager::HashKey = hashStr;

		// Load all cached entries into memory, no more per-call file I/O
		// INI reader doesn't support enumerating keys, so entries are loaded on first access via Get()
	}
	else
		eCachedPatternsManager::HashKey = nullptr;
}

void eCachedPatternsManager::FlushEntry(const std::string& key, uint64_t value)
{
	if (!HashKey || !ini)
		return;

	uint64_t offset = value;
	if (offset > (uint64_t)GameAddr)
		offset -= GameAddr;
	char buf[20];
	sprintf_s(buf, sizeof(buf), "%llX", offset);
	ini->WriteString(HashKey, (char*)key.c_str(), buf);
}

void eCachedPatternsManager::Flush()
{
	if (!HashKey || !ini)
		return;

	for (auto& pair : Cache)
		FlushEntry(pair.first, pair.second);

	Cache.clear();
}

std::string eCachedPatternsManager::Key(const std::string& pattern)
{
	std::string key;
	size_t i = 0;
	while (i < pattern.size())
	{
		while (i < pattern.size() && isspace((unsigned char)pattern[i]))
			i++;
		size_t start = i;
		while (i < pattern.size() && !isspace((unsigned char)pattern[i]))
			i++;
		if (start == i)
			break;
		std::string token = pattern.substr(start, i - start);
		if (token.find('?') != std::string::npos)
			token = "?";
		for (char& c : token)
			c = (char)toupper((unsigned char)c);
		key += (key.empty() ? "" : " ") + token;
	}
	return key;
}

void eCachedPatternsManager::Set(const std::string& pattern, uint64_t value)
{
	std::string key = Key(pattern);
	Cache[key] = value;
	if (bAutoFlush)
		FlushEntry(key, value);
}

uint64_t eCachedPatternsManager::Get(const std::string& pattern)
{
	std::string key = Key(pattern);
	auto it = Cache.find(key);
	if (it != Cache.end())
		return it->second;

	if (HashKey)
	{
		char* result = ini->ReadString(HashKey, (char*)key.c_str(), "0");
		uint64_t val = strtoull(result, nullptr, 16);
		if (val)
		{
			val += GameAddr;
			Cache[key] = val;
			return val;
		}
	}
	return 0;
}

bool eCachedPatternsManager::Has(const std::string& pattern)
{
	return Cache.find(Key(pattern)) != Cache.end();
}

static DWORD WINAPI PaidModWarningThread(LPVOID)
{
	MessageBoxA(0, "Please note that MK12TTH is a free modding tool that is meant to be used with free content.\nIf you have paid for anything, ask for a refund.", "MK12TTH Installed", MB_ICONEXCLAMATION);
	return 0;
}

void eFirstRunManager::Init()
{
	ini = new CIniReader((char*)StateFile::FILE_NAME);

	bPaidModWarned				= ini->ReadBoolean	("FirstRun",			"bPaidModWarned",			false);


	if (!bPaidModWarned)
	{
		Save();
		CreateThread(NULL, 0, PaidModWarningThread, NULL, 0, NULL);
	}
}

void eFirstRunManager::Save()
{
	ini->WriteBoolean("FirstRun", "bPaidModWarned", true);
}

void eSettingsManager::Init()
{
	CIniReader ini("");

	// Debug Settings
	bEnableConsoleWindow		= ini.ReadBoolean	("Settings.Debug",		"bEnableConsoleWindow",		false);
	bPauseOnStart				= ini.ReadBoolean	("Settings.Debug",		"bPauseOnStart",			false);
	bDebug						= ini.ReadBoolean	("Settings.Debug",		"bDebug",					false);
	bAllowNonMK					= ini.ReadBoolean	("Settings.Debug",		"bAllowNonMK",				false);
	bFloydDebug					= ini.ReadBoolean	("Settings.Debug",		"bFloydDebug",				false);
	bProfileDebug				= ini.ReadBoolean	("Settings.Debug",		"bProfileDebug",			false);

	// Flood Toggles
	Floods.bFName				= ini.ReadBoolean	("Settings.Debug.FloodToggles",	"bFName",			false);
	Floods.bEndpoint			= ini.ReadBoolean	("Settings.Debug.FloodToggles",	"bEndpoint",		false);
	Floods.bCurl				= ini.ReadBoolean	("Settings.Debug.FloodToggles",	"bCurl",			false);

	// Settings
	iLogSize					= ini.ReadInteger	("Settings",			"iLogSize",					50);
	iLogLevel					= ini.ReadInteger	("Settings",			"iLogLevel",				Log::None);
	szGameVer					= ini.ReadString	("Settings",			"szGameVer",				"0.103");
	szModLoader					= ini.ReadString	("Settings",			"szModLoader",				"Kernel32.CreateFileW");
	szAntiCheatEngine			= ini.ReadString	("Settings",			"szAntiCheatEngine",		"User32.EnumChildWindows");
	szCurlSetOpt				= ini.ReadString	("Settings",			"szCurlSetOpt",				"libcurl.curl_easy_setopt");
	szCurlPerform				= ini.ReadString	("Settings",			"szCurlPerform",			"libcurl.curl_easy_perform");
	bEnableKeyboardHotkeys		= ini.ReadBoolean	("Settings",			"bEnableKeyboardHotkeys",	true);
	// Patches
	szFloydTowerMode			= ini.ReadString	("Patches",				"szFloydTowerMode",			"Game");
	bFloydTowerFromGame			= _stricmp(szFloydTowerMode.c_str(), "Calculated") != 0;
	szAttractModeDelay			= ini.ReadString	("Patches",				"szAttractModeDelay",		"Game");

	// Patterns
	pPakTocCheck				= ini.ReadString	("Patterns",			"pPakTocCheck",				"");
	pUNameObjGetPat				= ini.ReadString	("Patterns",			"pUNameObjGetPat",			"");
	pFPathLoadPat				= ini.ReadString	("Patterns",			"pFPathLoadPat",			"");
	pFPath2LoadPat				= ini.ReadString	("Patterns",			"pFPath2LoadPat",			"");
	pFPathCLoadPat				= ini.ReadString	("Patterns",			"pFPathCLoadPat",			"");
	pEndpointLoader				= ini.ReadString	("Patterns",			"pEndpointLoader",			"");
	pProfileGetter				= ini.ReadString	("Patterns",			"pProfileGetter",			"");
	pOnlineSubsystem			= ini.ReadString	("Patterns",			"pOnlineSubsystem",			"");
	pOnlineCheck				= ini.ReadString	("Patterns",			"pOnlineCheck",				"");
	pAuthToken					= ini.ReadString	("Patterns",			"pAuthToken",				"");
	// Curl
	pCurlSetOpt					= ini.ReadString	("Patterns.Curl",		"pCurlSetOpt",				"");
	pCurlMultiAddHandle			= ini.ReadString	("Patterns.Curl",		"pCurlMultiAddHandle",		"");
	pCurlMultiInfoRead			= ini.ReadString	("Patterns.Curl",		"pCurlMultiInfoRead",		"");
	// Version
	pNRSClientVersion			= ini.ReadString	("Patterns.Version",	"pNRSClientVersion",		"");
	pBuildVersion				= ini.ReadString	("Patterns.Version",	"pBuildVersion",			"");
	pBuildDate					= ini.ReadString	("Patterns.Version",	"pBuildDate",				"");
	pChangelist					= ini.ReadString	("Patterns.Version",	"pChangelist",				"");
	// Floyd
	pSecretFightCondPat			= ini.ReadString	("Patterns.Floyd",		"pSecretFightCondPat",		"");
	pGetChallengesFromHash		= ini.ReadString	("Patterns.Floyd",		"pGetChallengesFromHash",	"");
	pGetFloydHashInputString	= ini.ReadString	("Patterns.Floyd",		"pGetFloydHashInputString",	"");
	pGetFloydHashInputString2	= ini.ReadString	("Patterns.Floyd",		"pGetFloydHashInputString2","");
	pGetTacoHashInputString		= ini.ReadString	("Patterns.Floyd",		"pGetTacoHashInputString",	"");
	pGetTacoSchedule			= ini.ReadString	("Patterns.Floyd",		"pGetTacoSchedule",			"");
	pTowersSetup				= ini.ReadString	("Patterns.Floyd",		"pTowersSetup",				"");
	pGetFloydVictories			= ini.ReadString	("Patterns.Floyd",		"pGetFloydVictories",		"");
	pAttractModeIdle			= ini.ReadString	("Patterns.MainMenu",	"pAttractModeIdle",			"");
	pAttractModeLimit			= ini.ReadString	("Patterns.MainMenu",	"pAttractModeLimit",			"");
	pKeepRunning				= ini.ReadString	("Patterns.MainMenu",	"pKeepRunning",				"");
	pFocusLost					= ini.ReadString	("Patterns.MainMenu",	"pFocusLost",				"");
	pFocusLostPause				= ini.ReadString	("Patterns.MainMenu",	"pFocusLostPause",			"");
	pBootWaitFirst				= ini.ReadString	("Patterns.MainMenu",	"pBootWaitFirst",			"");
	pBootWaitSecond				= ini.ReadString	("Patterns.MainMenu",	"pBootWaitSecond",			"");
	pBootWaitThird				= ini.ReadString	("Patterns.MainMenu",	"pBootWaitThird",			"");
	pBootStudioLogos			= ini.ReadString	("Patterns.MainMenu",	"pBootStudioLogos",			"");
	pBootMovieSkip				= ini.ReadString	("Patterns.MainMenu",	"pBootMovieSkip",			"");
	pGetStatsContainer			= ini.ReadString	("Patterns.Floyd",		"pGetStatsContainer",		"");
	pGetProfileStat				= ini.ReadString	("Patterns.Floyd",		"pGetProfileStat",			"");
	pGetProfileStatBits			= ini.ReadString	("Patterns.Floyd",		"pGetProfileStatBits",		"");
	pFloydSeed					= ini.ReadString	("Patterns.Floyd",		"pFloydSeed",				"");
	pFloydLevel					= ini.ReadString	("Patterns.Floyd",		"pFloydLevel",				"");
	pFloydLevelSession			= ini.ReadString	("Patterns.Floyd",		"pFloydLevelSession",		"");
	pFloydRetries				= ini.ReadString	("Patterns.Floyd",		"pFloydRetries",			"");
	pAnnouncerReroll			= ini.ReadString	("Patterns.Announcer",	"pAnnouncerReroll",			"");
	pAnnouncerMenuApply			= ini.ReadString	("Patterns.Announcer",	"pAnnouncerMenuApply",		"");
	pApplyAnnouncer				= ini.ReadString	("Patterns.Announcer",	"pApplyAnnouncer",			"");
	pEngineNameConstruct		= ini.ReadString	("Patterns.Announcer",	"pEngineNameConstruct",		"");
	pGetCurrentMission			= ini.ReadString	("Patterns.Announcer",	"pGetCurrentMission",		"");
	pGetTeam					= ini.ReadString	("Patterns.Announcer",	"pGetTeam",					"");
	pGetTeamDefinitions			= ini.ReadString	("Patterns.Announcer",	"pGetTeamDefinitions",		"");
	// Message Box
	pTextFromString				= ini.ReadString	("Patterns.MessageBox",	"pTextFromString",			"");
	pFindText					= ini.ReadString	("Patterns.MessageBox",	"pFindText",				"");
	pMessageBoxConstruct		= ini.ReadString	("Patterns.MessageBox",	"pMessageBoxConstruct",		"");
	pMessageBoxSetTitle			= ini.ReadString	("Patterns.MessageBox",	"pMessageBoxSetTitle",		"");
	pMessageBoxAddButton		= ini.ReadString	("Patterns.MessageBox",	"pMessageBoxAddButton",		"");
	pMessageBoxShow				= ini.ReadString	("Patterns.MessageBox",	"pMessageBoxShow",			"");
	pMessageBoxClose			= ini.ReadString	("Patterns.MessageBox",	"pMessageBoxClose",			"");
	pMessageBoxDestruct			= ini.ReadString	("Patterns.MessageBox",	"pMessageBoxDestruct",		"");
	pPlayerInputPressed			= ini.ReadString	("Patterns.MessageBox",	"pPlayerInputPressed",		"");
	pPlayerInputUpdate			= ini.ReadString	("Patterns.MessageBox",	"pPlayerInputUpdate",		"");
	pToastDriverCall			= ini.ReadString	("Patterns.MessageBox",	"pToastDriverCall",			"");
	pToastEnqueue				= ini.ReadString	("Patterns.MessageBox",	"pToastEnqueue",			"");
	pRealloc					= ini.ReadString	("Patterns.MessageBox",	"pRealloc",					"");
	pLocalPlayerInfo			= ini.ReadString	("Patterns.MessageBox",	"pLocalPlayerInfo",			"");
	pMessageBoxCallback			= ini.ReadString	("Patterns.MessageBox",	"pMessageBoxCallback",		"");
	// Game State
	pMainLoop					= ini.ReadString	("Patterns.GameState",	"pMainLoop",				"");
	pFrameTick					= ini.ReadString	("Patterns.GameState",	"pFrameTick",				"");
	pOnlineModeCheck			= ini.ReadString	("Patterns.GameState",	"pOnlineModeCheck",			"");
	pModeManagerGetter			= ini.ReadString	("Patterns.GameState",	"pModeManagerGetter",		"");
	// Settings Screen
	pSettingsBuildTabs			= ini.ReadString	("Patterns.SettingsScreen",	"pSettingsBuildTabs",		"");
	pSettingsCopyTabs			= ini.ReadString	("Patterns.SettingsScreen",	"pSettingsCopyTabs",		"");
	pMenuEntryConstruct			= ini.ReadString	("Patterns.SettingsScreen",	"pMenuEntryConstruct",		"");
	pSettingsTabConstruct		= ini.ReadString	("Patterns.SettingsScreen",	"pSettingsTabConstruct",	"");
	pSettingsTabDestruct		= ini.ReadString	("Patterns.SettingsScreen",	"pSettingsTabDestruct",		"");
	pGrowArray					= ini.ReadString	("Patterns.SettingsScreen",	"pGrowArray",				"");
	pStringReserve				= ini.ReadString	("Patterns.SettingsScreen",	"pStringReserve",			"");
	pFindTextPlatform			= ini.ReadString	("Patterns.SettingsScreen",	"pFindTextPlatform",		"");
	pMemoryFree					= ini.ReadString	("Patterns.SettingsScreen",	"pMemoryFree",				"");
	pMainMenuCopyTabs			= ini.ReadString	("Patterns.SettingsScreen",	"pMainMenuCopyTabs",		"");
	pSettingsVersionText		= ini.ReadString	("Patterns.SettingsScreen",	"pSettingsVersionText",	"");
	pSettingsRun				= ini.ReadString	("Patterns.SettingsScreen",	"pSettingsRun",				"");
	pSettingsPageSwitch			= ini.ReadString	("Patterns.SettingsScreen",	"pSettingsPageSwitch",			"");
	pFooterBuild				= ini.ReadString	("Patterns.SettingsScreen",	"pFooterBuild",				"");
	pPromptText					= ini.ReadString	("Patterns.SettingsScreen",	"pPromptText",				"");
	pPauseCollectEntries		= ini.ReadString	("Patterns.SettingsScreen",	"pPauseCollectEntries",		"");
	pPausePanelPageSetup		= ini.ReadString	("Patterns.SettingsScreen",	"pPausePanelPageSetup",		"");
	pGrowPauseEntries			= ini.ReadString	("Patterns.SettingsScreen",	"pGrowPauseEntries",			"");
	pPanelHolder				= ini.ReadString	("Patterns.SettingsScreen",	"pPanelHolder",				"");
	pPauseSelectedIndex			= ini.ReadString	("Patterns.SettingsScreen",	"pPauseSelectedIndex",		"");
	pPauseClose					= ini.ReadString	("Patterns.SettingsScreen",	"pPauseClose",				"");
	// Paks
	pPakMount					= ini.ReadString	("Patterns.Paks",		"pPakMount",				"");
	pConfigInit					= ini.ReadString	("Patterns.Paks",		"pConfigInit",				"");
	pConfigGlobals				= ini.ReadString	("Patterns.Paks",		"pConfigGlobals",			"");
	pConfigSetString			= ini.ReadString	("Patterns.Paks",		"pConfigSetString",			"");
	pConfigGetString			= ini.ReadString	("Patterns.Paks",		"pConfigGetString",			"");
	pConfigFindFile				= ini.ReadString	("Patterns.Paks",		"pConfigFindFile",			"");
	pLoadFileToString			= ini.ReadString	("Patterns.Paks",		"pLoadFileToString",		"");


	// Keybinds
	hkMenu						= ini.ReadString	("Keybinds",			"hkMenu",					"F1");
	hkInfo						= ini.ReadString	("Keybinds",			"hkInfo",					"TAB");
	hkCheats					= ini.ReadString	("Keybinds",			"hkCheats",					"F12");

	// Private Server
	szServerUrl					= ini.ReadString	("Server",				"szServerUrl",				"");

}
