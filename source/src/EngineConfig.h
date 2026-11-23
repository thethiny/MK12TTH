#pragma once
#include "PakMods.h"
#include <string>
#include <vector>

// Engine.ini values pak mods need, set in the engine's config in memory right after it loads its own. Nothing is saved,
// a mod that's off isn't applied
// - An Engine.ini anywhere in a mod's folder in Content\Mods, while the mod is on. Two mods setting one key: the one
//   that wins the load order wins. Engine.ini files anywhere else are reported and left out
// - MK12/Config/Mods/<ModName>.ini inside a mod's pak
// Only keys that differ from the game's values, list keys (Paths=, +Key=) are left alone
namespace EngineConfig {
	struct Value {
		std::wstring	Section;
		std::wstring	Key;
		std::wstring	Text;
	};

	struct LooseFile {
		std::wstring		ModFolder;	// Under Content\Mods
		std::wstring		Path;
		std::vector<Value>	Values;
	};

	void Scan(); // After PakMods::Init
	std::vector<const LooseFile*> FilesFor(const PakMods::Mod& mod);

	// Found by the hooks
	struct GameFunctions {
		void**	Config;			// The engine's config, set once it starts
		void*	EngineIniName;	// FString the engine files Engine.ini values under
		void*	SetString;
		void*	GetString;
		void*	FindFile;
	};
	void SetGameFunctions(const GameFunctions& Functions);
	void Inject(); // Right after the engine loaded its config
}
