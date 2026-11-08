#include "Features.h"
#include "mk12hook.h"

// Every feature, in install order: a feature comes after everything it needs
namespace Features {
	void DefineAll()
	{
		using namespace MK12Hook::Hooks;

		// Infrastructure, other features build on it
		Define({ Id::VersionInfo, "bEnableVersionInfo", true, Kind::Hidden, {}, ResolveVersionFunctions,
			L"VERSION INFO", L"Reads the game's build version, build date and changelist." });
		Define({ Id::GameState, "bEnableGameState", true, Kind::Hidden, {}, GameStateHooks,
			L"GAME STATE", L"Knows when the game is loading or online, and runs MK12TTH every frame." });
		Define({ Id::Popups, "bEnableGameMessageBox", true, Kind::Hidden, { Id::GameState }, GameMessageBoxHooks,
			L"GAME MESSAGE BOX", L"The game's own popups and notifications, for MK12TTH's messages." });
		Define({ Id::ModSettings, "bEnableModSettings", true, Kind::Hidden, { Id::Popups }, ModSettingsHooks,
			L"MK12TTH MENUS", L"The MK12TTH tab in Settings and the MODS menu." });

		// Mods
		Define({ Id::ModLoader, "bEnableModLoader", true, Kind::Startup, {}, DisablePakSigning,
			L"MOD LOADER", L"Lets the game load pak mods from Content\\Paks." });
		Define({ Id::PakMods, "bEnablePakMods", true, Kind::Startup, {}, PakModsHooks,
			L"PAK MOD SWITCHES", L"Lets you turn each pak mod on and off from MODS, PAK MODS." });
		Define({ Id::EngineConfig, "bModEngineConfig", true, Kind::Startup, { Id::PakMods }, EngineConfigHooks,
			L"MOD ENGINE SETTINGS", L"Applies the Engine.ini values pak mods need: an Engine.ini in the mod's folder in Content\\Mods, or one inside its pak. Only while the mod is on, nothing is saved." });
		Define({ Id::NameTable, "bUNameGetter", true, Kind::Startup, {}, NameTableHooks,
			L"NAME TABLE", L"MK12TTH's own copy of how the game turns names into text. STRING SWAP builds on it." });
		Define({ Id::StringSwap, "bEnableStringSwap", true, Kind::Live, { Id::NameTable }, StringSwapHooks,
			L"STRING SWAP", L"Swaps names in the game using string_swaps.txt, next to the mod." });

		Define({ Id::Announcer, "bAnnouncerOverride", true, Kind::Startup, { Id::GameState }, AnnouncerHooks,
			L"ANNOUNCER OVERRIDE", L"Lets you pick the announcer in MODS, LIVE FEATURES: your own setting, the announcer of the fighter you play, or any announcer." });

		// Floyd
		Define({ Id::FloydTracking, "bEnableFloydTracking", true, Kind::Live, {}, FloydTrackingHooks,
			L"FLOYD TRACKING", L"Reads your Floyd challenges, encounters and the Floyd Tower schedule." });
		Define({ Id::FloydTowerAlert, "bFloydTowerAlert", true, Kind::Live, { Id::FloydTracking, Id::Popups }, FloydTowerAlertHooks,
			L"FLOYD TOWER ALERT", L"Says when the Floyd Tower shows up as you open Living Towers, once you've beaten Floyd. Also reads your Floyd stats for PROFILE." });
		Define({ Id::FightMetadata, "bGetFightMetadata", true, Kind::Startup, {}, ExtractFightMetadataFromSecretFightSetupStage,
			L"FIGHT METADATA", L"Reads the conditions of secret fights, like Floyd's, as they're set up." });
		Define({ Id::FightMetadataLog, "bSerializeSecretFights", true, Kind::Live, { Id::FightMetadata, Id::NameTable }, nullptr,
			L"SECRET FIGHT LOG", L"Writes the conditions of secret fights out in full in the console.", false, nullptr });

		// Online
		Define({ Id::ProfileGetter, "bEnableProfileGetter", true, Kind::Live, {}, ProfileGetterHooks,
			L"PROFILE GETTER", L"Reads your platform, platform ID and save key." });
		Define({ Id::Curl, "bPatchCurl", true, Kind::Live, {}, PatchCurl,
			L"WEB CAPTURE", L"Reads your sign in ticket and access token from the game's web requests.", false });
		Define({ Id::PrivateServer, "bEnableServerProxy", false, Kind::Startup, {}, PrivateServerHooks,
			L"PRIVATE SERVER", L"Sends the game's server traffic to the server set in MK12TTH.ini (szServerUrl). WB Network traffic isn't affected.",
			true, "Server" });

		// Quality of life
		Define({ Id::AttractMode, "bAttractModeSwitch", true, Kind::Startup, {}, AttractModeHooks,
			L"ATTRACT MODE SWITCH", L"Lets you change when attract mode, the demo after idling on the main menu, starts." });
		Definition KeepRunning = { Id::KeepRunning, "bKeepRunning", true, Kind::Live, {}, KeepRunningHooks,
			L"KEEP RUNNING IN BACKGROUND", L"Keeps the game running when its window isn't in focus, instead of pausing.", false };
		KeepRunning.OnSwitch = SetKeepRunning;
		Define(KeepRunning);
		Define({ Id::FastBoot, "bFastBoot", true, Kind::Startup, {}, FastBootHooks,
			L"FAST BOOT", L"Skips the wait on the logo and legal screens at startup. The license agreement still shows." });
	}
}
