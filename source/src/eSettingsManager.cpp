#include "eSettingsManager.h"
#include "..\IniReader.h"
#include <Windows.h>

eSettingsManager* SettingsMgr = new eSettingsManager();

void eSettingsManager::Init()
{
	CIniReader ini("");

	// Debug Settings
	bEnableConsoleWindow		= ini.ReadBoolean	("Settings.Debug",		"bEnableConsoleWindow",		false);
	bPauseOnStart				= ini.ReadBoolean	("Settings.Debug",		"bPauseOnStart",			false);
	bDebug						= ini.ReadBoolean	("Settings.Debug",		"bDebug",					false);
	
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

	// Patches.AntiCheat
	bDisableSignatureCheck		= ini.ReadBoolean	("Patches.AntiCheat",	"bDisableSignatureCheck",	true);
	bDisableChunkSigCheck		= ini.ReadBoolean	("Patches.AntiCheat",	"bDisableChunkSigCheck",	true);
	//bDisableSignatureWarn		= ini.ReadBoolean	("Patches.AntiCheat",	"bDisableSignatureWarn",	false);
	bDisableSignatureWarn		= false;
	bDisableTOCSigCheck			= ini.ReadBoolean	("Patches.AntiCheat",	"bDisableTOCSigCheck",		true);

	// Patterns
	pSigCheck					= ini.ReadString	("Patterns",			"pSigCheck",				"");
	pChunkSigCheck				= ini.ReadString	("Patterns",			"pChunkSigCheck",			"");
	pChunkSigCheckFunc			= ini.ReadString	("Patterns",			"pChunkSigCheckFunc",		"");
	pSigWarn					= ini.ReadString	("Patterns",			"pSigWarn",					"");
	pTocCheck					= ini.ReadString	("Patterns",			"pTocCheck",				"");


	// Keybinds
	hkMenu						= ini.ReadString	("Keybinds",			"hkMenu",					"F1");
	hkInfo						= ini.ReadString	("Keybinds",			"hkInfo",					"TAB");
	hkCheats					= ini.ReadString	("Keybinds",			"hkCheats",					"F12");

	// Private Server
	szMITMUrl					= ini.ReadString	("Server",				"szMITMUrl",				"127.0.0.1");
	bMITM						= ini.ReadBoolean	("Server",				"bMITM",					false);

}
