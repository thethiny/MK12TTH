#pragma once
#include<string>
#include "..\IniReader.h"

class eSettingsManager {
public:
	void Init();

public:
	// Settings

	bool bEnableKeyboardHotkeys;

	// Debug
	bool bEnableConsoleWindow;
	bool bPauseOnStart;
	int	iLogLevel;
	bool bDebug;
	bool bAllowNonMK;


	// Toggles
	bool bDisableSignatureCheck;
	bool bDisableSignatureWarn;
	bool bDisableTOCSigCheck;
	bool bDisableChunkSigCheck;
	bool bDisablePakTOCCheck;
	bool bPatchCurl;
	bool bFNameToStrHook;
	bool bUNameGetter;

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

extern eSettingsManager* SettingsMgr;
extern eFirstRunManager* FirstRunMgr;