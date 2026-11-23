#pragma once

// <mod>_State.ini next to the mod, named after it like its ini (MK12TTH_State.ini): what the player picked in game
// (pak mods on or off, live switches) and first run flags. <mod>.ini stays the advanced file: what gets hooked,
// patterns, debug
namespace StateFile {
	const char* FileName(); // The file's name, without its folder
	constexpr const char* MODS			= "Mods";		// Pak mod switches, path under Paks without extension
	constexpr const char* LIVE			= "Live";		// Live switches and choices, by feature key
	constexpr const char* FIRST_RUN		= "FirstRun";

	// Once at startup, before anything reads it: moves the older files in (MK12TTH_Mods.ini, tt_state.ini and
	// [Mods] of MK12TTH.ini), then removes them. A renamed mod with no state file of its own yet starts from a copy of
	// MK12TTH_State.ini
	void Migrate();
}
