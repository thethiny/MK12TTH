#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// Pak mods: lists them, decides their load order and keeps the disabled ones from mounting. On or off is saved per mod
// in the state file (missing means on). A change applies after a restart: the game can't unmount a mod. Two places:
// - Content\Mods: one folder per mod, with its paks (.pak, .utoc/.ucas, any number) and its Engine.ini. A pak right in
//   Content\Mods is a mod of its own. MK12TTH mounts them through the game's own mount, all with one priority over the
//   game's files: between two mods giving the same file, the last name wins, like in Paks. A creator can raise a mod by
//   ending its folder (or file) name with _P<number>: higher wins, over everything without one. Inside a mod with several paks each pak has its own switch, and so has each folder holding more than one pak: a pak
//   mounts only when it, its folders and its mod are on. A folder with a single pak is that pak. A folder's
//   description.txt is shown with it
// - Content\Paks: the old place, the game mounts them itself with the same priority as its own files
namespace PakMods {
	enum class Kind {
		Pak,			// .pak only
		IoStore,		// .utoc/.ucas with a .pak beside it
		IoStoreOnly,	// .utoc/.ucas without a .pak, the game only looks for .pak files so we mount it
	};

	enum class Source {
		ModsFolder,	// Content\Mods, mounted by us
		Paks,		// Content\Paks, mounted by the game
	};

	// One pak of a mod: a .pak, a .utoc/.ucas or both under one name
	struct Container {
		std::wstring	Folder;		// Under the mod's root, empty for the root itself
		std::wstring	Name;		// File name without extension
		Kind			Type = Kind::Pak;
		uint32_t		Order = 0;	// The priority it's mounted with, the game's own files have 3
		bool			bLoads = true; // As the game was started: it, its folders and its mod are on
		std::wstring	DescriptionInPak; // Path of its own description.txt in the game's file system, empty without one
		std::wstring	Description; // Its own description.txt, read once it's mounted (Content\Mods only)
	};

	// Content\Mods, inside a mod with several paks: a switch for each pak and each folder with more than one pak
	struct Switch {
		std::string		Key;		// State file key: the mod's key and the path under it
		std::wstring	Path;		// Under the mod's root, the pak's without extension
		std::wstring	Name;		// The folder's or the pak's own name
		bool			bFolder = false;
		int				Depth = 1;	// Folders with a switch above it, the mod included
		size_t			Pak = 0;	// Paks: index in the mod's Paks
		bool			bEnabled = true; // Its own switch, as the game was started
		std::wstring	Description; // Folders: their description.txt
	};

	// What the game will really use from a mod. A higher mount priority wins; with the same priority the pak mounted
	// first wins, and the game mounts Paks by path, last name first. Files in a .pak and in a .utoc are looked up apart
	struct Override {
		std::string	Key;		// The other mod
		size_t		Files = 0;	// Files the two share
	};
	struct LoadInfo {
		bool					bContentsRead = false;	// False when its files couldn't be listed
		size_t					Files = 0;				// Everything it has
		size_t					Lost = 0;				// Files another mod that's on gives instead
		std::vector<Override>	LostTo;					// The mods that win over it, strongest first
		std::vector<Override>	WinsOver;				// The mods it wins over
		bool					bAssetsInPakOnly = false; // Game assets in a .pak only, the game loads them from .utoc/.ucas
		bool					bAfterGameFiles = false;	// Paks only: mounts after some of the game's own paks
		std::wstring			LastGamePak;			// The game's own pak that mounts first, for the advice
	};

	struct Mod {
		std::wstring Name; // Its folder's name, or the file name without extension for a single pak
		std::wstring Folder; // Its folder under the root, empty for a single pak right in the root
		std::string Key; // Ini key: Paks keeps the path under Paks without extension ("pakchunk93-modPAK/MK3SelectMusic"),
		                 // Content\Mods is "Content/Mods/" and its folder or file name
		Source From = Source::Paks;
		std::vector<Container> Paks; // Paks: always one
		Kind Type = Kind::Pak; // Pak when all its paks are .pak only, IoStore otherwise
		bool bEnabled = true; // As the game was started
		int Priority = -1; // From _P<number> in its name, -1 without one
		std::vector<Switch> Switches; // Content\Mods with several paks: folders and paks inside, parents first
		std::wstring Description; // Content\Mods folder with several paks: its description.txt
		int MountRank = 0; // 0 wins over everything
		LoadInfo Load; // For mods that are off: what would happen if they were on
	};
	const Mod* Find(const std::string& Key);
	bool AnyLoads(const Mod& mod); // Some of its paks mount

	void Init(); // Lists the mods and reads their switches, call before the game mounts paks
	const std::vector<Mod>& List();
	const std::filesystem::path& Root(Source From); // Content\Mods or Content\Paks
	std::filesystem::path FileOf(const Mod& mod, const Container& Pak, const wchar_t* Extension); // ".pak", ".utoc"

	// Called with each pak the game mounts (path as the game passes it). True when it must not mount
	bool IsDisabled(const wchar_t* PakPath);

	typedef __int64(__fastcall MountType)(void* PakPlatformFile, const wchar_t* PakFilename, uint32_t PakOrder, const wchar_t* MountPath, bool bLoadIndex);
	// Mounts what the game wouldn't: Content\Mods and the Paks mods without a .pak. Once, with the first pak the game
	// mounts (its global container is up by then). Mount time doesn't matter, the priority each pak gets does
	void MountOurs(void* PakPlatformFile, MountType* Mount);
}
