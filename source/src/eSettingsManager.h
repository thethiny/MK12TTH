#pragma once
#include<string>

class eSettingsManager {
public:
	void Init();

public:
	// Debug
	bool bEnableConsoleWindow;
	bool bPauseOnStart;
	int	iVerbose;
	bool bDebug;

	// Toggles
	bool bDisableSignatureCheck;
	//bool bDisableSignatureWarn;
	bool bDisableTOCSigCheck;
	bool bDisableChunkSigCheck;
	bool bPatchCurl;

	// Addresses

	// Patterns
	std::string pSigCheck;
	//std::string pSigWarn;
	std::string pTocCheck;
	std::string pChunkSigCheck;
	std::string pChunkSigCheckFunc;


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
	std::string szMITMUrl;
	bool bMITM;

};

extern eSettingsManager* SettingsMgr;