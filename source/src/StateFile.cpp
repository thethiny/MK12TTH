#include "StateFile.h"
#include "../IniReader.h"
#include <Windows.h>
#include <string>
#include <vector>

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

namespace StateFile {
	constexpr const char* OLD_MODS_FILE		= "MK12TTH_Mods.ini";
	constexpr const char* OLD_STATE_FILE	= "tt_state.ini";
	constexpr const char* FIRST_STATE_FILE	= "MK12TTH_State.ini"; // The state file's name before it followed the mod's

	static std::string ModFolder()
	{
		char Path[MAX_PATH] = {};
		GetModuleFileNameA((HINSTANCE)&__ImageBase, Path, MAX_PATH);
		std::string Folder = Path;
		return Folder.substr(0, Folder.find_last_of("\\/") + 1);
	}

	const char* FileName()
	{
		static std::string Name;
		if (Name.empty())
		{
			char Path[MAX_PATH] = {};
			GetModuleFileNameA((HINSTANCE)&__ImageBase, Path, MAX_PATH);
			std::string Module = Path;
			Module = Module.substr(Module.find_last_of("\\/") + 1);
			Name = Module.substr(0, Module.find_last_of('.')) + "_State.ini";
		}
		return Name.c_str();
	}

	static std::string MainIniPath()
	{
		char Path[MAX_PATH] = {};
		GetModuleFileNameA((HINSTANCE)&__ImageBase, Path, MAX_PATH);
		std::string Ini = Path;
		return Ini.substr(0, Ini.find_last_of('.')) + ".ini";
	}

	// key=value lines of a section, empty when it's missing
	static std::vector<std::string> ReadSection(const std::string& File, const char* Section)
	{
		std::vector<char> Buffer;
		DWORD Length = IniReadSection(Section, Buffer, File.c_str());
		std::vector<std::string> Lines;
		for (const char* Line = Buffer.data(); Line < Buffer.data() + Length && *Line; Line += strlen(Line) + 1)
			Lines.push_back(Line);
		return Lines;
	}

	// Keys already in the state file win, it's the newer one
	static void MoveSection(const std::string& From, const char* FromSection, const std::string& To, const char* ToSection)
	{
		std::vector<std::string> Lines = ReadSection(From, FromSection);
		for (size_t i = 0; i < Lines.size(); i++)
		{
			const std::string& Line = Lines[i];
			size_t Equals = Line.find('=');
			if (Equals == std::string::npos)
				continue;
			std::string Key = Line.substr(0, Equals);
			if (!IniHasKey(ToSection, Key.c_str(), To.c_str()))
				IniWriteSafe(ToSection, Key.c_str(), Line.substr(Equals + 1).c_str(), To.c_str());
		}
	}

	void Migrate()
	{
		std::string Folder = ModFolder();
		std::string State = Folder + FileName();

		// Copied, not moved: the file stays for the mod it was made by
		std::string FirstState = Folder + FIRST_STATE_FILE;
		if (_stricmp(FileName(), FIRST_STATE_FILE) != 0 && GetFileAttributesA(State.c_str()) == INVALID_FILE_ATTRIBUTES &&
			GetFileAttributesA(FirstState.c_str()) != INVALID_FILE_ATTRIBUTES)
			CopyFileA(FirstState.c_str(), State.c_str(), TRUE);

		std::string OldMods = Folder + OLD_MODS_FILE;
		if (GetFileAttributesA(OldMods.c_str()) != INVALID_FILE_ATTRIBUTES)
		{
			MoveSection(OldMods, "Mods", State, MODS);
			DeleteFileA(OldMods.c_str());
		}

		std::string OldState = Folder + OLD_STATE_FILE;
		if (GetFileAttributesA(OldState.c_str()) != INVALID_FILE_ATTRIBUTES)
		{
			MoveSection(OldState, "FirstRun", State, FIRST_RUN);
			DeleteFileA(OldState.c_str());
		}

		// The first pak mod switches lived in the main ini
		std::string MainIni = MainIniPath();
		if (!ReadSection(MainIni, "Mods").empty())
		{
			MoveSection(MainIni, "Mods", State, MODS);
			IniWriteSafe("Mods", nullptr, nullptr, MainIni.c_str()); // Removes the section
		}
	}
}
