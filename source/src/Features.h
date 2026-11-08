#pragma once
#include <functional>
#include <utility>
#include <string>
#include <vector>

// Every MK12TTH feature: what it needs, how far it got at startup and whether it runs right now. One place owns
// all of it, everyone else asks.
// - Startup: a feature is hooked unless MK12TTH.ini turns it off ([Patches], the Settings tab, needs a restart).
//   It's installed after everything it needs, and when one of those isn't installed it's blocked by it.
// - Live: an installed feature with a switch can be turned off and on while the game runs (MODS). Its hooks stay
//   in and skip their own work. The switch is saved to the state file
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
		Pending,	// Not looked at yet
		Disabled,	// Off in MK12TTH.ini, never hooked
		Installed,	// Hooked and working
		Failed,		// Its own hooks couldn't go in
		Blocked,	// Something it needs isn't installed
	};

	enum class Kind {
		Hidden,		// Infrastructure, ini only
		Startup,	// Settings tab row, a restart applies it
		Live,		// Settings tab row, plus a live switch in MODS
	};

	struct Definition {
		Id						Which;
		const char*				IniKey;				// MK12TTH.ini (IniSection), and the live switch in the state file
		bool					bIniDefault;		// When the key is missing
		Kind					Type;
		std::vector<Id>			Needs;				// Defined before it
		std::function<bool()>	Install;			// Null: nothing to hook, it only needs what it needs
		const wchar_t*			Title;				// Settings tab and MODS
		const wchar_t*			Description;
		bool					bLiveDefault = true;	// Live switch when the state file has none
		const char*				IniSection = "Patches"; // Null: nothing to hook, so no startup switch, only the live one
		std::function<void(bool)> OnSwitch;			// Live: runs when it's installed and on each change, with the new state
		std::vector<std::pair<const wchar_t*, const char*>> Modes; // Startup choices (label, ini value) instead of OFF / ON.
													// The first is off, bIniDefault picks the second or the first
	};

	void DefineAll(); // The feature list, FeatureList.cpp

	void Define(Definition Feature); // In order: everything it needs comes first
	void InstallAll(); // Once at startup, in the order defined

	const Definition& Get(Id Which);
	State GetState(Id Which);
	Id BlockedBy(Id Which); // The first need that isn't installed, when Blocked
	bool IsInstalled(Id Which);
	bool IsActive(Id Which); // Installed and switched on (always on without a live switch). Any thread
	void SetActive(Id Which, bool bActive); // Ignored unless it's installed with a live switch. Saved

	bool HasStartupSwitch(Id Which); // Has a row in Settings, MK12TTH
	int StartupMode(Id Which); // Index into its Modes, from MK12TTH.ini (0 is off)

	// Why it isn't running, for the player: empty when it's installed
	std::wstring Explain(Id Which);
	void PrintSummary();
}
