#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Pak mods: lists them, orders them and keeps the ones turned off from mounting. On/off is saved per mod in the state
// file (missing is on), a change needs a restart since the game can't unmount
// Content\Mods: one folder per mod (any number of .pak and .utoc/.ucas, its Engine.ini), a pak right in it is a mod of
// its own. We mount them with the game's own mount, all at one priority over the game's files: the last name wins like
// in Paks. _P<number> at the end of a mod's name raises it, higher wins. A mod with several paks has a switch per pak
// and per folder holding more than one pak, a pak mounts only when it, its folders and its mod are on. A folder's
// description.txt is shown with it
// Content\Paks: the old place, mounted by the game itself at the priority of its own files
namespace PakMods {
	enum class Kind {
		Pak,			// .pak only
		IoStore,		// .utoc/.ucas with a .pak
		IoStoreOnly,	// .utoc/.ucas without a .pak, the game only looks for .pak files so we mount it
	};

	enum class Source {
		ModsFolder,		// Mounted by us
		Paks,			// Mounted by the game
	};

	// A .pak, a .utoc/.ucas or both under one name
	struct Container {
		std::wstring	Folder;				// Under the mod's root
		std::wstring	Name;				// Without extension
		Kind			Type = Kind::Pak;
		uint32_t		Order = 0;			// Mount priority, the game's own files have 3
		bool			bLoads = true;		// As the game started: it, its folders and its mod are on
		std::wstring	DescriptionInPak;	// Its description.txt in the game's file system
		std::wstring	Description;		// Read once it's mounted (Content\Mods only)
	};

	// Content\Mods, mod with several paks: one per pak and per folder with more than one pak
	struct Switch {
		std::string		Key;				// The mod's key + the path under it
		std::wstring	Path;				// Under the mod's root, paks without extension
		std::wstring	Name;
		bool			bFolder = false;
		int				Depth = 1;			// Switches above it, the mod included
		size_t			Pak = 0;			// Index in the mod's Paks
		bool			bEnabled = true;	// As the game started
		std::wstring	Description;		// Folders: their description.txt
	};

	// What the game really uses from a mod. Higher priority wins, at the same priority the pak mounted first wins (Paks
	// mounts by path, last first). .pak and .utoc files are looked up apart
	struct Override {
		std::string	Key;		// The other mod
		size_t		Files = 0;	// Files both have
	};
	struct LoadInfo {
		bool					bContentsRead = false;
		size_t					Files = 0;
		size_t					Lost = 0;					// Files a mod that's on gives instead
		std::vector<Override>	LostTo;						// Strongest first
		std::vector<Override>	WinsOver;
		bool					bAssetsInPakOnly = false;	// The game loads assets from .utoc/.ucas only
		bool					bAfterGameFiles = false;	// Paks: mounts after some of the game's paks
		std::wstring			LastGamePak;				// The game's pak that mounts first
	};

	struct Mod {
		std::wstring			Name;				// Folder name, or file name for a single pak
		std::wstring			Folder;				// Under the root, empty for a pak right in it
		std::string				Key;				// Paks: path under Paks without extension. Content\Mods: "Content/Mods/" + its name
		Source					From = Source::Paks;
		std::vector<Container>	Paks;				// Paks: always one
		Kind					Type = Kind::Pak;	// Pak when all its paks are .pak only
		bool					bEnabled = true;	// As the game started
		int						Priority = -1;		// _P<number>
		std::vector<Switch>		Switches;			// Parents first
		std::wstring			Description;		// Content\Mods folder with several paks
		int						MountRank = 0;		// 0 wins over everything
		LoadInfo				Load;				// Mods that are off: as if they were on
	};
	const Mod* Find(const std::string& Key);
	bool AnyLoads(const Mod& mod);

	void Init(); // Before the game mounts paks
	const std::vector<Mod>& List();
	const std::wstring& Root(Source From); // Content\Mods or Content\Paks
	std::wstring FileOf(const Mod& mod, const Container& Pak, const wchar_t* Extension);
	std::wstring PathAppend(const std::wstring& Path, const std::wstring& Part); // Path / Part, a separator only when needed

	bool IsDisabled(const wchar_t* PakPath); // Each pak the game mounts, true skips it

	typedef __int64(__fastcall MountType)(void* PakPlatformFile, const wchar_t* PakFilename, uint32_t PakOrder, const wchar_t* MountPath, bool bLoadIndex);
	// Mounts Content\Mods and the Paks mods without a .pak, once with the first pak the game mounts (its global container
	// is up then). Only the priority matters, not the time
	void MountOurs(void* PakPlatformFile, MountType* Mount);
}
