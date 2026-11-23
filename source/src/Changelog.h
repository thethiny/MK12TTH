#pragma once

// What's new in this version, shown from the startup toast (ACCEPT) and MODS, ABOUT, WHAT'S NEW. Plain text in the
// game's message box: only what players notice, one line per change
namespace Changelog {
	constexpr const wchar_t* Current =
		L"Mods: put them in Content\\Mods, one folder per mod. Turn each mod, pak or folder on and off in MODS, PAK MODS.\n"
		L"Settings in game: MODS in the main menu, MK12TTH in Settings, and MOD SETTINGS in the pause menu during fights. "
		L"Nothing to edit by hand.\n"
		L"Floyd: your challenges, encounters and wins in MODS, PROFILE, and an alert when the Floyd Tower shows up.\n"
		L"Floyd fight: set his difficulty level and your retries, spawn the Floyd Tower now, or summon Floyd.\n"
		L"Announcer: your own, your fighter's (AUTO) or any announcer.";
}
