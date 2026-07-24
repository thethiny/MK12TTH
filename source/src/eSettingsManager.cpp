#include "eSettingsManager.h"
#include <Windows.h>

eSettingsManager*		SettingsMgr			= new eSettingsManager();
eFirstRunManager*		FirstRunMgr			= new eFirstRunManager;
eCachedPatternsManager*	CachedPatternsMgr	= new eCachedPatternsManager;
__int64 eCachedPatternsManager::GameAddr = reinterpret_cast<__int64>(GetModuleHandle(nullptr));

void eCachedPatternsManager::Init(uint64_t Hash, const char* version)
{
	ini = new CIniReader("PatternsCache.cache");
	if (Hash)
	{
		size_t totalLen = 8 + 1 + strlen(version) + 1;
		char* hashStr = new char[totalLen];
		sprintf_s(hashStr, totalLen, "%08X.%s", (uint32_t)(Hash >> 32), version);
		eCachedPatternsManager::Hash = hashStr;
	}
	else
		eCachedPatternsManager::Hash = nullptr;
}

void eCachedPatternsManager::Save(char* key, uint64_t offset)
{
	if (Hash)
	{
		if (offset > GameAddr)
		{
			offset -= GameAddr;
		}
		ini->WriteInteger(Hash, key, offset);
	}
}

uint64_t eCachedPatternsManager::Load(char* key)
{
	if (Hash)
	{
		uint64_t result = ini->ReadInteger(Hash, key, 0);
		if (result)
			return result + GameAddr;
	}
	return 0;
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
	bAllowNonMK					= ini.ReadBoolean	("Settings.Deubg",		"bAllowNonMK",				false);
	
	// Settings
	iLogSize					= ini.ReadInteger	("Settings",			"iLogSize",					50);
	iLogLevel					= ini.ReadInteger	("Settings",			"iLogLevel",				0);
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
