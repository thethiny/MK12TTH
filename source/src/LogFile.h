#pragma once

// <mod>_log.txt next to the game, named after the mod like its cache (mk12tth_log.txt): a copy of everything the mod
// prints to the console, emptied every launch. Not a debug setting: it's on whatever the log level and console window
// say, so the output can be read afterwards. The console stays as it is, the file gets the same text without the
// color codes
namespace LogFile {
	bool Start(); // Once, before anything is printed
}
