#include "StateFile.h"
#include <Windows.h>
#include <string>
#include <vector>

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

namespace StateFile {
	constexpr const char* OLD_MODS_FILE		= "MK12TTH_Mods.ini";
	constexpr const char* OLD_STATE_FILE	= "tt_state.ini";

	static std::string ModFolder()
	{
		char Path[MAX_PATH] = {};
		GetModuleFileNameA((HINSTANCE)&__ImageBase, Path, MAX_PATH);
		std::string Folder = Path;
		return Folder.substr(0, Folder.find_last_of("\\/") + 1);
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
		std::vector<char> Buffer(64 * 1024);
		DWORD Length = GetPrivateProfileSectionA(Section, Buffer.data(), (DWORD)Buffer.size(), File.c_str());
		std::vector<std::string> Lines;
		for (const char* Line = Buffer.data(); Line < Buffer.data() + Length && *Line; Line += strlen(Line) + 1)
			Lines.push_back(Line);
		return Lines;
	}

	// Keys already in the state file win, it's the newer one
	static void MoveSection(const std::string& From, const char* FromSection, const std::string& To, const char* ToSection)
	{
		for (const std::string& Line : ReadSection(From, FromSection))
		{
			size_t Equals = Line.find('=');
			if (Equals == std::string::npos)
				continue;
			std::string Key = Line.substr(0, Equals);
			char Existing[8] = {};
			if (GetPrivateProfileStringA(ToSection, Key.c_str(), "", Existing, sizeof(Existing), To.c_str()) == 0)
				WritePrivateProfileStringA(ToSection, Key.c_str(), Line.substr(Equals + 1).c_str(), To.c_str());
		}
	}

	void Migrate()
	{
		std::string Folder = ModFolder();
		std::string State = Folder + FILE_NAME;

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
			WritePrivateProfileStringA("Mods", nullptr, nullptr, MainIni.c_str()); // Removes the section
		}
	}
}
