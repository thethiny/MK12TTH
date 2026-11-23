#pragma once
#include <string>
#include <vector>

// Every MK12TTH feature, what it needs and whether it runs
// Startup: hooked unless MK12TTH.ini turns it off (needs a restart). Installed after what it needs, blocked if one of those isn't installed
// Live: an installed feature with a switch can be turned off and on in game (MODS). Its hooks stay in and skip their work
namespace Features {
	enum class Id {
		VersionInfo,
		ModLoader,
		GameState,
		Popups,
		ModSettings,
		PakMods,
		EngineConfig,
		NameTable,
		StringSwap,
		Announcer,
		FightMetadata,
		FightMetadataLog,
		FloydTracking,
		FloydTowerAlert,
		ProfileGetter,
		Curl,
		PrivateServer,
		AttractMode,
		KeepRunning,
		FastBoot,
		FloydFight,
		Count
	};

	enum class State {
		Pending,
		Disabled,	// Off in the ini
		Installed,
		Failed,		// Its hooks couldn't go in
		Blocked,	// Something it needs isn't installed
	};

	enum class Kind {
		Hidden,		// Ini only
		Startup,	// Settings tab row, needs a restart
		Live,		// Settings tab row + live switch in MODS
	};

	typedef bool (InstallType)();
	typedef void (OnSwitchType)(bool bActive);

	struct Mode {
		const wchar_t*	Label;
		const char*		IniValue;
	};

	struct Definition {
		Id					Which;
		const char*			IniKey;					// Also the live switch's key in the state file
		bool				bIniDefault;
		Kind				Type;
		std::vector<Id>		Needs;					// Must be defined before it
		InstallType*		Install;				// Null: nothing to hook
		const wchar_t*		Title;
		const wchar_t*		Description;
		bool				bLiveDefault = true;
		const char*			IniSection = "Patches";	// Null: no startup switch, only the live one
		OnSwitchType*		OnSwitch = nullptr;		// Live: after install and on each change
		std::vector<Mode>	Modes;					// Startup choices instead of OFF / ON. The first is off, bIniDefault picks the second
	};

	void DefineAll(); // FeatureList.cpp

	void Define(const Definition& Feature);
	void InstallAll();

	const Definition& Get(Id Which);
	State GetState(Id Which);
	Id BlockedBy(Id Which);
	bool IsInstalled(Id Which);
	bool IsActive(Id Which); // Installed and switched on. Any thread
	void SetActive(Id Which, bool bActive); // Live switch, saved

	bool HasStartupSwitch(Id Which);
	int StartupMode(Id Which); // Index into Modes (0 is off)

	std::wstring Explain(Id Which); // Why it isn't running, empty when installed
	void PrintSummary();
}
