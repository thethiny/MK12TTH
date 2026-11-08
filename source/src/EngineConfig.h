#pragma once
#include "PakMods.h"
#include <string>
#include <vector>

// Engine.ini values that pak mods need, put into the engine's config in memory right after it loads its own,
// before anything reads them. Nothing is written to disk: a mod that's off simply isn't applied.
// Where they come from:
// - An Engine.ini in a mod's folder in Content\Mods (anywhere inside it), applied while that mod is on. When two mods
//   set the same key, the one that wins the load order wins. Engine.ini files anywhere else are reported and left out
// - MK12/Config/Mods/<ModName>.ini inside a mod's own pak
// Only keys that differ from the game's own values are applied, list keys (Paths=, +Key=) are left alone
namespace EngineConfig {
	struct Value {
		std::wstring Section;
		std::wstring Key;
		std::wstring Text;
	};

	struct LooseFile {
		std::wstring			ModFolder;	// The mod's folder under Content\Mods
		std::filesystem::path	Path;
		std::vector<Value>		Values;
	};

	void Scan(); // After the pak mods are listed: finds the mods' Engine.ini files
	std::vector<const LooseFile*> FilesFor(const PakMods::Mod& mod); // The Engine.ini files a mod's folder brings

	// The game's config functions, found by the hooks
	struct GameFunctions {
		void**	Config;			// The engine's config, set once it starts
		void*	EngineIniName;	// FString, the Engine.ini name the engine files its values under
		void*	SetString;
		void*	GetString;
		void*	FindFile;
	};
	void SetGameFunctions(const GameFunctions& Functions);
	void Inject(); // Right after the engine loaded its config
}
