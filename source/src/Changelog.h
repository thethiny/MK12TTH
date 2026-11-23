#pragma once

// What's new in this version, shown from the startup toast (ACCEPT) and MODS, ABOUT, WHAT'S NEW. Plain text in the
// game's message box: only what players notice, one line per change
namespace Changelog {
	constexpr const wchar_t* Current =
		L"Attract mode delay: now on the title screen too, not only the main menu.\n"
		L"Announcer: AUTO works without MK1Hook, and finds your fighter in every kind of team.\n"
		L"Pause menu: MOD SETTINGS has a panel of its own, and shows in pause menus without KONTROLS too.\n"
		L"Mods: a mod folder's description.txt shows for a folder with a single pak too.\n"
		L"Settings and choices are saved safely: a crash while saving can't break the ini files.\n"
		L"Faster startup after a mod update: found patterns are kept per game build.\n"
		L"Everything the mod prints is also written to mk12tth_log.txt next to the game.\n"
		L"The startup notice offers what's new only after an update.";
}
