#pragma once

// MK12TTH_State.ini, next to the mod: what the player picked in game (pak mods on or off, live switches) and
// first run flags. MK12TTH.ini stays the advanced file: what gets hooked, patterns, debug
namespace StateFile {
	constexpr const char* FILE_NAME		= "MK12TTH_State.ini";
	constexpr const char* MODS			= "Mods";		// Pak mod switches, path under Paks without extension
	constexpr const char* LIVE			= "Live";		// Live switches and choices, by feature key
	constexpr const char* FIRST_RUN		= "FirstRun";

	// Once at startup, before anything reads it: moves the older files in (MK12TTH_Mods.ini, tt_state.ini and
	// [Mods] of MK12TTH.ini), then removes them
	void Migrate();
}
