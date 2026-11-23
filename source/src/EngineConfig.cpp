#include "mkutils.h"
#include "EngineConfig.h"
#include "PakMods.h"
#include "StateFile.h"
#include "mk12.h"
#include "../IniReader.h"
#include <Windows.h>
#include <algorithm>
#include <fstream>
#include <map>

namespace EngineConfig {
	constexpr const wchar_t*	LOOSE_NAME			= L"engine.ini"; // Compared lowercase
	constexpr const wchar_t*	IN_PAK_FOLDER		= L"../../../MK12/Config/Mods/"; // Inside a mod's pak, as the engine sees it
	constexpr uint32_t			CONFIG_FILE_DIRTY	= 0x50; // A dirty config file is saved to disk, ours never should

	typedef void			(__fastcall SetStringType)	(void* Config, const wchar_t* Section, const wchar_t* Key, const wchar_t* Value, void* FileName);
	typedef MK12::FString*	(__fastcall GetStringType)	(void* Config, MK12::FString* Out, const wchar_t* Section, const wchar_t* Key, void* FileName);
	typedef char*			(__fastcall FindFileType)	(void* Config, void* FileName, bool bCreate);
	typedef bool			(__fastcall LoadFileType)	(MK12::FString* Out, const wchar_t* FileName, uint8_t VerifyFlags, uint32_t ReadFlags);

	static std::vector<LooseFile>	Loose;
	static GameFunctions			Game = {};

	static std::wstring Lower(std::wstring Text)
	{
		for (size_t i = 0; i < Text.size(); i++)
			Text[i] = (wchar_t)towlower(Text[i]);
		return Text;
	}

	static std::wstring Trim(const std::wstring& Text)
	{
		size_t Start = Text.find_first_not_of(L" \t\r\n");
		size_t End = Text.find_last_not_of(L" \t\r\n");
		return Start == std::wstring::npos ? L"" : Text.substr(Start, End - Start + 1);
	}

	// Single values only: keys that show up twice in a section are lists, and so are the +, -, ., ! ones
	static std::vector<Value> Parse(const std::wstring& Text)
	{
		std::vector<Value> Values;
		std::map<std::wstring, int> Seen; // Lowercase section \n key
		std::wstring Section;
		for (size_t Start = 0; Start < Text.size();)
		{
			size_t End = Text.find(L'\n', Start);
			if (End == std::wstring::npos)
				End = Text.size();
			std::wstring Line = Trim(Text.substr(Start, End - Start));
			Start = End + 1;
			if (Line.empty() || Line[0] == L';' || Line[0] == L'#')
				continue;
			if (Line[0] == L'[' && Line[Line.size() - 1] == L']')
			{
				Section = Line.substr(1, Line.size() - 2);
				continue;
			}
			size_t Equals = Line.find(L'=');
			if (Section.empty() || Equals == std::wstring::npos || wcschr(L"+-.!", Line[0]))
				continue;
			Value value = { Section, Trim(Line.substr(0, Equals)), Trim(Line.substr(Equals + 1)) };
			if (Seen[Lower(Section) + L"\n" + Lower(value.Key)]++ == 0)
				Values.push_back(value);
		}

		std::vector<Value> Single;
		for (size_t i = 0; i < Values.size(); i++)
		{
			if (Seen[Lower(Values[i].Section) + L"\n" + Lower(Values[i].Key)] == 1)
				Single.push_back(Values[i]);
		}
		return Single;
	}

	static std::wstring ReadText(const std::wstring& Path)
	{
		std::string Bytes;
		std::ifstream File(Path.c_str(), std::ios::binary);
		if (File)
		{
			File.seekg(0, std::ios::end);
			Bytes.resize((size_t)File.tellg());
			File.seekg(0, std::ios::beg);
			if (!Bytes.empty())
				File.read(&Bytes[0], Bytes.size());
		}
		if (Bytes.size() >= 2 && (uint8_t)Bytes[0] == 0xFF && (uint8_t)Bytes[1] == 0xFE)
			return std::wstring((const wchar_t*)(Bytes.data() + 2), (Bytes.size() - 2) / sizeof(wchar_t));
		size_t Skip = Bytes.size() >= 3 && (uint8_t)Bytes[0] == 0xEF && (uint8_t)Bytes[1] == 0xBB && (uint8_t)Bytes[2] == 0xBF ? 3 : 0;
		int Size = MultiByteToWideChar(CP_UTF8, 0, Bytes.data() + Skip, (int)(Bytes.size() - Skip), nullptr, 0);
		std::wstring Text(Size, L'\0');
		if (Size > 0)
			MultiByteToWideChar(CP_UTF8, 0, Bytes.data() + Skip, (int)(Bytes.size() - Skip), &Text[0], Size);
		return Text;
	}

	static const PakMods::Mod* OwnerOf(const std::wstring& ModFolder)
	{
		const std::vector<PakMods::Mod>& Mods = PakMods::List();
		for (size_t i = 0; i < Mods.size(); i++)
		{
			const PakMods::Mod& mod = Mods[i];
			if (mod.From == PakMods::Source::ModsFolder && !mod.Folder.empty() && !_wcsicmp(mod.Folder.c_str(), ModFolder.c_str()))
				return &mod;
		}
		return nullptr;
	}

	struct FoundIni {
		std::wstring Path;
		std::wstring Folder; // Under the root, forward slashes, empty for the root itself
	};

	// Every Engine.ini under Folder, folders in the order Windows lists them. A folder that can't be listed stops the
	// whole search (bStop)
	static void FindInis(const std::wstring& Dir, const std::wstring& Folder, std::vector<FoundIni>& Found, bool& bStop)
	{
		WIN32_FIND_DATAW Data;
		HANDLE Find = FindFirstFileW(PakMods::PathAppend(Dir, L"*").c_str(), &Data);
		if (Find == INVALID_HANDLE_VALUE)
		{
			bStop = true;
			return;
		}
		do
		{
			std::wstring Name = Data.cFileName;
			if (Name == L"." || Name == L"..")
				continue;
			std::wstring Path = PakMods::PathAppend(Dir, Name);
			if (Data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (!(Data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) || Data.dwReserved0 != IO_REPARSE_TAG_SYMLINK)
					FindInis(Path, Folder.empty() ? Name : Folder + L"/" + Name, Found, bStop);
				if (bStop)
					break;
				continue;
			}
			if (Lower(Name) != LOOSE_NAME)
				continue;
			FoundIni Ini = { Path, Folder };
			Found.push_back(Ini);
		} while (FindNextFileW(Find, &Data));
		FindClose(Find);
	}

	static std::vector<FoundIni> FindInis(const std::wstring& Root)
	{
		std::vector<FoundIni> Found;
		bool bStop = false;
		FindInis(Root, L"", Found, bStop);
		return Found;
	}

	static void WarnOutside(PakMods::Source From)
	{
		std::vector<FoundIni> Found = FindInis(PakMods::Root(From));
		for (size_t i = 0; i < Found.size(); i++)
		{
			if (From == PakMods::Source::Paks || Found[i].Folder.empty())
				printfWarning("Engine Config: %ls isn't used, put it in its mod's folder in Content\\Mods", Found[i].Path.c_str());
		}
	}

	void Scan()
	{ // Only in a mod's folder in Content\Mods, anywhere inside it
		Loose.clear();
		std::vector<FoundIni> Found = FindInis(PakMods::Root(PakMods::Source::ModsFolder));
		for (size_t i = 0; i < Found.size(); i++)
		{
			const std::wstring& Relative = Found[i].Folder;
			std::wstring ModFolder = Relative.substr(0, Relative.find(L'/'));
			if (Relative.empty() || !OwnerOf(ModFolder))
				continue; // Reported by WarnOutside, or a folder without paks
			LooseFile File;
			File.ModFolder = ModFolder;
			File.Path = Found[i].Path;
			File.Values = Parse(ReadText(File.Path));
			printfInfo("Engine Config: %ls has %zu values", File.Path.c_str(), File.Values.size());
			Loose.push_back(File);
		}
		WarnOutside(PakMods::Source::ModsFolder);
		WarnOutside(PakMods::Source::Paks);
	}

	std::vector<const LooseFile*> FilesFor(const PakMods::Mod& mod)
	{
		std::vector<const LooseFile*> Files;
		for (size_t i = 0; i < Loose.size(); i++)
		{
			if (mod.From == PakMods::Source::ModsFolder && !mod.Folder.empty() && !_wcsicmp(Loose[i].ModFolder.c_str(), mod.Folder.c_str()))
				Files.push_back(&Loose[i]);
		}
		return Files;
	}

	void SetGameFunctions(const GameFunctions& Functions)
	{
		Game = Functions;
	}

	static bool IsLooseFileOn(const LooseFile& File)
	{
		const PakMods::Mod* Owner = OwnerOf(File.ModFolder);
		return Owner && Owner->bEnabled;
	}

	static std::wstring CurrentValue(void* Config, const Value& value)
	{
		MK12::FString Current = { nullptr, 0, 0 };
		((GetStringType*)Game.GetString)(Config, &Current, value.Section.c_str(), value.Key.c_str(), Game.EngineIniName);
		std::wstring Text = Current.Data ? Current.Data : L"";
		if (Current.Data && MK12::MemoryFree)
			MK12::MemoryFree(Current.Data);
		return Text;
	}

	static int Apply(void* Config, const std::vector<Value>& Values, const std::wstring& From, std::map<std::wstring, std::wstring>& SetBy)
	{
		int Applied = 0;
		for (size_t i = 0; i < Values.size(); i++)
		{
			const Value& value = Values[i];
			std::wstring Id = Lower(value.Section + L"|" + value.Key);
			std::map<std::wstring, std::wstring>::iterator Previous = SetBy.find(Id);
			if (Previous != SetBy.end())
				printfWarning("Engine Config: %ls sets [%ls] %ls again, after %ls", From.c_str(), value.Section.c_str(), value.Key.c_str(), Previous->second.c_str());
			else if (CurrentValue(Config, value) == value.Text)
				continue; // The game's own value
			((SetStringType*)Game.SetString)(Config, value.Section.c_str(), value.Key.c_str(), value.Text.c_str(), Game.EngineIniName);
			SetBy[Id] = From;
			Applied++;
			printfInfo("Engine Config: [%ls] %ls=%ls (%ls)", value.Section.c_str(), value.Key.c_str(), value.Text.c_str(), From.c_str());
		}
		return Applied;
	}

	static bool WeakerFirst(const LooseFile* A, const LooseFile* B)
	{
		return OwnerOf(A->ModFolder)->MountRank > OwnerOf(B->ModFolder)->MountRank;
	}

	void Inject()
	{
		void* Config = Game.Config ? *Game.Config : nullptr;
		if (!Config || !Game.EngineIniName || !Game.SetString || !Game.GetString)
			return;

		char* File = Game.FindFile ? ((FindFileType*)Game.FindFile)(Config, Game.EngineIniName, false) : nullptr;
		bool bWasDirty = File && File[CONFIG_FILE_DIRTY];

		std::map<std::wstring, std::wstring> SetBy;
		int Applied = 0;
		// Weakest mod first, so the one winning the load order also wins a key two mods set
		std::vector<const LooseFile*> InOrder;
		for (size_t i = 0; i < Loose.size(); i++)
		{
			if (IsLooseFileOn(Loose[i]))
				InOrder.push_back(&Loose[i]);
		}
		std::stable_sort(InOrder.begin(), InOrder.end(), WeakerFirst);
		for (size_t i = 0; i < InOrder.size(); i++)
			Applied += Apply(Config, InOrder[i]->Values, L"Mods/" + InOrder[i]->ModFolder, SetBy);
		if (MK12::LoadFileToString)
		{
			const std::vector<PakMods::Mod>& Mods = PakMods::List();
			for (size_t i = 0; i < Mods.size(); i++)
			{
				const PakMods::Mod& mod = Mods[i];
				if (!mod.bEnabled)
					continue;
				MK12::FString Text = { nullptr, 0, 0 };
				std::wstring Path = std::wstring(IN_PAK_FOLDER) + mod.Name + L".ini";
				if (MK12::LoadFileToString(&Text, Path.c_str(), 0, 0) && Text.Data)
					Applied += Apply(Config, Parse(Text.Data), mod.Name, SetBy);
				if (Text.Data && MK12::MemoryFree)
					MK12::MemoryFree(Text.Data);
			}
		}

		if (File && !bWasDirty)
			File[CONFIG_FILE_DIRTY] = 0; // Keeps the engine from saving our values to the player's Engine.ini
		if (Applied)
			printfSuccess("Engine Config: %d values from mods", Applied);
	}
}
