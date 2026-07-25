#include "eSettingsManager.h"
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

void eCachedPatternsManager::Set(const std::string& key, uint64_t value)
{
	Cache[key] = value;
	if (bAutoFlush)
		FlushEntry(key, value);
}

uint64_t eCachedPatternsManager::Get(const std::string& key)
{
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

bool eCachedPatternsManager::Has(const std::string& key)
{
	return Cache.find(key) != Cache.end();
}

static DWORD WINAPI PaidModWarningThread(LPVOID)
{
	MessageBoxA(0, "Please note that MK12TTH is a free modding tool that is meant to be used with free content.\nIf you have paid for anything, ask for a refund.", "MK12TTH Installed", MB_ICONEXCLAMATION);
	return 0;
}

void eFirstRunManager::Init()
{
	ini = new CIniReader("tt_state.ini");

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

	// Flood Toggles
	Floods.bFName				= ini.ReadBoolean	("Settings.Debug.FloodToggles",	"bFName",			false);
	Floods.bEndpoint			= ini.ReadBoolean	("Settings.Debug.FloodToggles",	"bEndpoint",		false);

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
	bPatchCurl					= ini.ReadBoolean	("Patches",				"bPatchCurl",				false);
	bFNameToStrHook				= ini.ReadBoolean	("Patches",				"bFPathLoader",				false);
	bUNameGetter				= ini.ReadBoolean	("Patches",				"bUNameGetter",				false);
	bGetFightMetadata			= ini.ReadBoolean	("Patches",				"bGetFightMetadata",		false);
	bSerializeSecretFights		= ini.ReadBoolean	("Patches",				"bSerializeSecretFights",	false);
	bEnableStringSwap			= ini.ReadBoolean	("Patches",				"bEnableStringSwap",		false);
	bEnableFloydTracking		= ini.ReadBoolean	("Patches",				"bEnableFloydTracking",		false);
	bEnableProfileGetter		= ini.ReadBoolean	("Patches",				"bEnableProfileGetter",		false);

	// Patches.AntiCheat
	bDisableSignatureCheck		= ini.ReadBoolean	("Patches.AntiCheat",	"bDisableSignatureCheck",	true);
	bDisableChunkSigCheck		= ini.ReadBoolean	("Patches.AntiCheat",	"bDisableChunkSigCheck",	true);
	//bDisableSignatureWarn		= ini.ReadBoolean	("Patches.AntiCheat",	"bDisableSignatureWarn",	false);
	bDisableSignatureWarn		= false;
	bDisableTOCSigCheck			= ini.ReadBoolean	("Patches.AntiCheat",	"bDisableTOCSigCheck",		true);
	bDisablePakTOCCheck			= ini.ReadBoolean	("Patches.AntiCheat",	"bDisablePakTOCCheck",		true);

	// Patterns
	pSigCheck					= ini.ReadString	("Patterns",			"pSigCheck",				"");
	pChunkSigCheck				= ini.ReadString	("Patterns",			"pChunkSigCheck",			"");
	pChunkSigCheckFunc			= ini.ReadString	("Patterns",			"pChunkSigCheckFunc",		"");
	pSigWarn					= ini.ReadString	("Patterns",			"pSigWarn",					"");
	pTocCheck					= ini.ReadString	("Patterns",			"pTocCheck",				"");
	pPakTocCheck				= ini.ReadString	("Patterns",			"pPakTocCheck",				"");
	pUNameObjGetPat				= ini.ReadString	("Patterns",			"pUNameObjGetPat",			"");
	pFPathLoadPat				= ini.ReadString	("Patterns",			"pFPathLoadPat",			"");
	pFPath2LoadPat				= ini.ReadString	("Patterns",			"pFPath2LoadPat",			"");
	pFPathCLoadPat				= ini.ReadString	("Patterns",			"pFPathCLoadPat",			"");
	pEndpointLoader				= ini.ReadString	("Patterns",			"pEndpointLoader",			"");
	pProfileGetter				= ini.ReadString	("Patterns",			"pProfileGetter",			"");
	// Floyd
	pSecretFightCondPat			= ini.ReadString	("Patterns.Floyd",		"pSecretFightCondPat",		"");
	pGetChallengesFromHash		= ini.ReadString	("Patterns.Floyd",		"pGetChallengesFromHash",	"");
	pGetFloydHashInputString	= ini.ReadString	("Patterns.Floyd",		"pGetFloydHashInputString",	"");
	pGetFloydHashInputString2	= ini.ReadString	("Patterns.Floyd",		"pGetFloydHashInputString2","");


	// Keybinds
	hkMenu						= ini.ReadString	("Keybinds",			"hkMenu",					"F1");
	hkInfo						= ini.ReadString	("Keybinds",			"hkInfo",					"TAB");
	hkCheats					= ini.ReadString	("Keybinds",			"hkCheats",					"F12");

	// Private Server
	szServerUrl					= ini.ReadString	("Server",				"szServerUrl",				"");
	bEnableServerProxy			= ini.ReadBoolean	("Server",				"bEnableServerProxy",		false);


	// Announcer Swap
	AnnouncerSwap.bEnable		= ini.ReadBoolean	("AnnouncerSwap",		"bEnable",					false);
	AnnouncerSwap.szDefault		= ini.ReadString	("AnnouncerSwap",		"Default",					"");
	AnnouncerSwap.szLiuKang		= ini.ReadString	("AnnouncerSwap",		"LiuKang",					"");
	AnnouncerSwap.szGeras		= ini.ReadString	("AnnouncerSwap",		"Geras",					"");
	AnnouncerSwap.szJohnnyCage	= ini.ReadString	("AnnouncerSwap",		"JohnnyCage",				"");
	AnnouncerSwap.szShangTsung	= ini.ReadString	("AnnouncerSwap",		"ShangTsung",				"");
	AnnouncerSwap.szSindel		= ini.ReadString	("AnnouncerSwap",		"Sindel",					"");
	AnnouncerSwap.szSubZero		= ini.ReadString	("AnnouncerSwap",		"SubZero",					"");
	AnnouncerSwap.szOmniMan		= ini.ReadString	("AnnouncerSwap",		"OmniMan",					"");
}
