#include "mkutils.h"
#include "EngineConfig.h"
#include "PakMods.h"
#include "StateFile.h"
#include "mk12.h"
#include "../IniReader.h"
#include <Windows.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace EngineConfig {
	constexpr const wchar_t* LOOSE_NAME			= L"engine.ini"; // Compared lowercase
	constexpr const wchar_t* IN_PAK_FOLDER		= L"../../../MK12/Config/Mods/"; // Inside a mod's pak, as the engine sees it
	constexpr uint32_t CONFIG_FILE_DIRTY		= 0x50; // A dirty config file gets saved to disk, ours never should

	typedef void			(__fastcall SetStringType)	(void* Config, const wchar_t* Section, const wchar_t* Key, const wchar_t* Value, void* FileName);
	typedef MK12::FString*	(__fastcall GetStringType)	(void* Config, MK12::FString* Out, const wchar_t* Section, const wchar_t* Key, void* FileName);
	typedef char*			(__fastcall FindFileType)	(void* Config, void* FileName, bool bCreate);
	typedef bool			(__fastcall LoadFileType)	(MK12::FString* Out, const wchar_t* FileName, uint8_t VerifyFlags, uint32_t ReadFlags);

	static std::vector<LooseFile> Loose;
	static GameFunctions Game = {};

	static std::wstring Lower(std::wstring Text)
	{
		for (wchar_t& c : Text)
			c = (wchar_t)towlower(c);
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
		std::map<std::pair<std::wstring, std::wstring>, int> Seen;
		std::wstring Section;
		std::wistringstream Lines(Text);
		std::wstring Line;
		while (std::getline(Lines, Line))
		{
			Line = Trim(Line);
			if (Line.empty() || Line[0] == L';' || Line[0] == L'#')
				continue;
			if (Line.front() == L'[' && Line.back() == L']')
			{
				Section = Line.substr(1, Line.size() - 2);
				continue;
			}
			size_t Equals = Line.find(L'=');
			if (Section.empty() || Equals == std::wstring::npos || wcschr(L"+-.!", Line[0]))
				continue;
			Value value = { Section, Trim(Line.substr(0, Equals)), Trim(Line.substr(Equals + 1)) };
			if (Seen[{ Lower(Section), Lower(value.Key) }]++ == 0)
				Values.push_back(value);
		}

		std::vector<Value> Single;
		for (const Value& value : Values)
		{
			if (Seen[{ Lower(value.Section), Lower(value.Key) }] == 1)
				Single.push_back(value);
		}
		return Single;
	}

	static std::wstring ReadText(const std::filesystem::path& Path)
	{
		std::ifstream File(Path, std::ios::binary);
		std::string Bytes((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
		if (Bytes.size() >= 2 && (uint8_t)Bytes[0] == 0xFF && (uint8_t)Bytes[1] == 0xFE)
			return std::wstring((const wchar_t*)(Bytes.data() + 2), (Bytes.size() - 2) / sizeof(wchar_t));
		size_t Skip = Bytes.size() >= 3 && (uint8_t)Bytes[0] == 0xEF && (uint8_t)Bytes[1] == 0xBB && (uint8_t)Bytes[2] == 0xBF ? 3 : 0;
		int Size = MultiByteToWideChar(CP_UTF8, 0, Bytes.data() + Skip, (int)(Bytes.size() - Skip), nullptr, 0);
		std::wstring Text(Size, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, Bytes.data() + Skip, (int)(Bytes.size() - Skip), Text.data(), Size);
		return Text;
	}

	static const PakMods::Mod* OwnerOf(const std::wstring& ModFolder)
	{
		for (const PakMods::Mod& mod : PakMods::List())
		{
			if (mod.From == PakMods::Source::ModsFolder && !mod.Folder.empty() && !_wcsicmp(mod.Folder.c_str(), ModFolder.c_str()))
				return &mod;
		}
		return nullptr;
	}

	static void WarnOutside(PakMods::Source From)
	{
		std::filesystem::path Root = PakMods::Root(From);
		std::error_code Error;
		for (auto It = std::filesystem::recursive_directory_iterator(Root, Error); !Error && It != std::filesystem::recursive_directory_iterator(); It.increment(Error))
		{
			if (!It->is_regular_file() || Lower(It->path().filename().wstring()) != LOOSE_NAME)
				continue;
			std::filesystem::path Relative = It->path().parent_path().lexically_relative(Root);
			if (From == PakMods::Source::Paks || Relative == L".")
				printfWarning("Engine Config: %ls isn't used, put it in its mod's folder in Content\\Mods", It->path().c_str());
		}
	}

	// Only in a mod's folder in Content\Mods, anywhere inside it
	void Scan()
	{
		Loose.clear();
		std::filesystem::path Root = PakMods::Root(PakMods::Source::ModsFolder);
		std::error_code Error;
		for (auto It = std::filesystem::recursive_directory_iterator(Root, Error); !Error && It != std::filesystem::recursive_directory_iterator(); It.increment(Error))
		{
			if (!It->is_regular_file() || Lower(It->path().filename().wstring()) != LOOSE_NAME)
				continue;
			std::wstring Relative = It->path().parent_path().lexically_relative(Root).wstring();
			std::replace(Relative.begin(), Relative.end(), L'\\', L'/');
			std::wstring ModFolder = Relative.substr(0, Relative.find(L'/'));
			if (Relative == L"." || !OwnerOf(ModFolder))
				continue; // Reported by WarnOutside, or a folder without paks
			LooseFile File;
			File.ModFolder = ModFolder;
			File.Path = It->path();
			File.Values = Parse(ReadText(It->path()));
			printfInfo("Engine Config: %ls has %zu values", It->path().c_str(), File.Values.size());
			Loose.push_back(std::move(File));
		}
		WarnOutside(PakMods::Source::ModsFolder);
		WarnOutside(PakMods::Source::Paks);
	}

	std::vector<const LooseFile*> FilesFor(const PakMods::Mod& mod)
	{
		std::vector<const LooseFile*> Files;
		for (const LooseFile& File : Loose)
		{
			if (mod.From == PakMods::Source::ModsFolder && !mod.Folder.empty() && !_wcsicmp(File.ModFolder.c_str(), mod.Folder.c_str()))
				Files.push_back(&File);
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
		for (const Value& value : Values)
		{
			std::wstring Id = Lower(value.Section + L"|" + value.Key);
			auto Previous = SetBy.find(Id);
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

	void Inject()
	{
		void* Config = Game.Config ? *Game.Config : nullptr;
		if (!Config || !Game.EngineIniName || !Game.SetString || !Game.GetString)
			return;

		char* File = Game.FindFile ? ((FindFileType*)Game.FindFile)(Config, Game.EngineIniName, false) : nullptr;
		bool bWasDirty = File && File[CONFIG_FILE_DIRTY];

		std::map<std::wstring, std::wstring> SetBy;
		int Applied = 0;
		// Weakest mod first, so the one that wins the load order also wins a key two mods set
		std::vector<const LooseFile*> InOrder;
		for (const LooseFile& Entry : Loose)
		{
			if (IsLooseFileOn(Entry))
				InOrder.push_back(&Entry);
		}
		std::stable_sort(InOrder.begin(), InOrder.end(), [](const LooseFile* A, const LooseFile* B) { return OwnerOf(A->ModFolder)->MountRank > OwnerOf(B->ModFolder)->MountRank; });
		for (const LooseFile* Entry : InOrder)
			Applied += Apply(Config, Entry->Values, L"Mods/" + Entry->ModFolder, SetBy);
		if (MK12::LoadFileToString)
		{
			for (const PakMods::Mod& mod : PakMods::List())
			{
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
			File[CONFIG_FILE_DIRTY] = 0; // Keeps the engine from saving our values to the player's own Engine.ini
		if (Applied)
			printfSuccess("Engine Config: %d values from mods", Applied);
	}
}
