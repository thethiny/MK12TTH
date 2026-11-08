#include "mkutils.h"
#include "PakMods.h"
#include "PakContents.h"
#include "StateFile.h"
#include "../IniReader.h"
#include <Windows.h>
#include <algorithm>
#include <cwctype>
#include <map>
#include <unordered_set>

namespace PakMods {
	constexpr const wchar_t* PAKS_MARKER = L"/content/paks/"; // Mount paths are relative to the game, compared lowercase with forward slashes
	constexpr const wchar_t* MODS_KEY_PREFIX = L"Content/Mods/"; // State file keys of Content\Mods, Paks keys stay as they were
	constexpr uint32_t CONTENT_PAK_ORDER = 3; // What the game gives every pak under Content: its own and the ones in Paks
	constexpr uint32_t FIRST_MOD_ORDER = CONTENT_PAK_ORDER + 1; // Content\Mods, over the game's files

	static std::vector<Mod> Mods;
	static std::vector<std::wstring> DisabledKeys; // Paks, lowercase
	static std::filesystem::path PaksRoot;
	static std::filesystem::path ModsRoot;
	static bool bMounted = false;

	static std::wstring Lower(std::wstring Text)
	{
		std::transform(Text.begin(), Text.end(), Text.begin(), [](wchar_t c) { return (wchar_t)towlower(c); });
		return Text;
	}

	static std::wstring ForwardSlashes(std::wstring Text)
	{
		std::replace(Text.begin(), Text.end(), L'\\', L'/');
		return Text;
	}

	static std::wstring Join(const std::wstring& Folder, const std::wstring& Name)
	{
		return Folder.empty() ? Name : Folder + L"/" + Name;
	}

	static std::string Narrow(const std::wstring& Text)
	{
		int Size = WideCharToMultiByte(CP_UTF8, 0, Text.c_str(), (int)Text.size(), nullptr, 0, nullptr, nullptr);
		std::string Out(Size, '\0');
		WideCharToMultiByte(CP_UTF8, 0, Text.c_str(), (int)Text.size(), Out.data(), Size, nullptr, nullptr);
		return Out;
	}

	// The game's own paks: pakchunkN-WindowsNoEditor, pakpcN-WindowsNoEditor and global, all in Paks itself
	static bool IsGamePak(const std::wstring& Name, const std::wstring& Folder)
	{
		if (!Folder.empty())
			return false;
		std::wstring Lowered = Lower(Name);
		auto StartsWith = [&](const wchar_t* Prefix) { return Lowered.rfind(Prefix, 0) == 0; };
		auto EndsWith = [&](const wchar_t* Suffix) {
			size_t Length = wcslen(Suffix);
			return Lowered.size() >= Length && Lowered.compare(Lowered.size() - Length, Length, Suffix) == 0;
		};
		return Lowered == L"global" || ((StartsWith(L"pakchunk") || StartsWith(L"pakpc")) && EndsWith(L"-windowsnoeditor"));
	}

	// "Name_P12" -> 12. A plain "_P" (the engine's patch naming) has no number and no priority
	static int PriorityFromName(const std::wstring& Name)
	{
		size_t At = Lower(Name).rfind(L"_p");
		if (At == std::wstring::npos || At + 2 >= Name.size() || Name.size() - At - 2 > 6)
			return -1;
		for (size_t i = At + 2; i < Name.size(); i++)
		{
			if (!iswdigit(Name[i]))
				return -1;
		}
		return _wtoi(Name.c_str() + At + 2);
	}

	// The key the game sorts Paks by: the path under Paks, compared without case
	static std::wstring MountKey(const Mod& mod, const Container& Pak)
	{
		return Lower(Join(Join(mod.Folder, Pak.Folder), Pak.Name) + L".pak");
	}

	static std::wstring MountKey(const Mod& mod)
	{
		return mod.Paks.empty() ? Lower(Join(mod.Folder, mod.Name)) : MountKey(mod, mod.Paks.front());
	}

	static uint32_t TopOrder(const Mod& mod)
	{
		uint32_t Top = 0;
		for (const Container& Pak : mod.Paks)
			Top = (std::max)(Top, Pak.Order);
		return Top;
	}

	// Content\Mods priorities, weakest first: mods with .utoc/.ucas, then .pak only mods, then the ones with _P<n> by n.
	// Name decides inside each group, the last one wins like it does in Paks. A mod's own paks follow each other, by name
	static void OrderModsFolder()
	{
		std::vector<Mod*> Ours;
		for (Mod& mod : Mods)
		{
			if (mod.From == Source::ModsFolder)
				Ours.push_back(&mod);
		}
		std::sort(Ours.begin(), Ours.end(), [](const Mod* A, const Mod* B)
		{
			if ((A->Priority >= 0) != (B->Priority >= 0))
				return A->Priority < 0;
			if (A->Priority != B->Priority)
				return A->Priority < B->Priority;
			bool bPakA = A->Type == Kind::Pak, bPakB = B->Type == Kind::Pak;
			if (bPakA != bPakB)
				return !bPakA;
			return Lower(A->Name) < Lower(B->Name);
		});
		uint32_t Next = FIRST_MOD_ORDER;
		for (Mod* mod : Ours)
		{
			std::sort(mod->Paks.begin(), mod->Paks.end(), [](const Container& A, const Container& B) { return Lower(Join(A.Folder, A.Name)) < Lower(Join(B.Folder, B.Name)); });
			for (Container& Pak : mod->Paks)
				Pak.Order = Next++;
		}
	}

	// Strongest first: higher priority, then the Paks mount order (our IoStore only mods go first, then last path first)
	static void RankMounts()
	{
		std::vector<Mod*> Order;
		for (Mod& mod : Mods)
			Order.push_back(&mod);
		std::stable_sort(Order.begin(), Order.end(), [](const Mod* A, const Mod* B)
		{
			if (TopOrder(*A) != TopOrder(*B))
				return TopOrder(*A) > TopOrder(*B);
			bool bOursA = A->Type == Kind::IoStoreOnly, bOursB = B->Type == Kind::IoStoreOnly;
			if (bOursA != bOursB)
				return bOursA;
			return MountKey(*A) > MountKey(*B);
		});
		for (size_t i = 0; i < Order.size(); i++)
			Order[i]->MountRank = (int)i;
	}

	static bool IsGameAsset(const std::wstring& File)
	{
		for (const wchar_t* Extension : { L".uasset", L".umap", L".uexp", L".ubulk" })
		{
			size_t Length = wcslen(Extension);
			if (File.size() >= Length && File.compare(File.size() - Length, Length, Extension) == 0)
				return true;
		}
		return false;
	}

	// Which files of each mod the game will really use, against the mods that are on
	static void AnalyzeLoading(const std::wstring& LastGamePak)
	{
		struct Contents { std::unordered_set<std::wstring> Pak, IoStore; };
		std::vector<Contents> All(Mods.size());
		for (size_t i = 0; i < Mods.size(); i++)
		{
			Mod& mod = Mods[i];
			bool bRead = true;
			for (const Container& Pak : mod.Paks)
			{
				if (Pak.Type != Kind::IoStoreOnly)
				{
					PakContents::Listing Files = PakContents::ReadPak(FileOf(mod, Pak, L".pak"));
					bRead &= Files.bRead;
					All[i].Pak.insert(Files.Files.begin(), Files.Files.end());
				}
				if (Pak.Type != Kind::Pak)
				{
					PakContents::Listing Files = PakContents::ReadUtoc(FileOf(mod, Pak, L".utoc"));
					bRead &= Files.bRead;
					All[i].IoStore.insert(Files.Files.begin(), Files.Files.end());
				}
			}
			mod.Load.bContentsRead = bRead;
			mod.Load.Files = All[i].Pak.size() + All[i].IoStore.size();
			mod.Load.bAssetsInPakOnly = mod.Type == Kind::Pak && std::any_of(All[i].Pak.begin(), All[i].Pak.end(), IsGameAsset);
			mod.Load.bAfterGameFiles = mod.From == Source::Paks && mod.Type != Kind::IoStoreOnly && !LastGamePak.empty() && MountKey(mod) < LastGamePak;
			mod.Load.LastGamePak = LastGamePak;
		}

		for (size_t i = 0; i < Mods.size(); i++)
		{
			Mod& mod = Mods[i];
			std::unordered_set<std::wstring> Lost;
			for (size_t j = 0; j < Mods.size(); j++)
			{
				const Mod& Other = Mods[j];
				if (j == i || !Other.bEnabled)
					continue;
				bool bOtherWins = Other.MountRank < mod.MountRank;
				size_t Shared = 0;
				auto Compare = [&](const std::unordered_set<std::wstring>& Mine, const std::unordered_set<std::wstring>& Theirs)
				{
					for (const std::wstring& File : Mine)
					{
						if (!Theirs.count(File))
							continue;
						Shared++;
						if (bOtherWins)
							Lost.insert(File);
					}
				};
				Compare(All[i].Pak, All[j].Pak);
				Compare(All[i].IoStore, All[j].IoStore);
				if (Shared)
					(bOtherWins ? mod.Load.LostTo : mod.Load.WinsOver).push_back({ Other.Key, Shared });
			}
			mod.Load.Lost = Lost.size();
			std::sort(mod.Load.LostTo.begin(), mod.Load.LostTo.end(), [](const Override& A, const Override& B) { return Find(A.Key)->MountRank < Find(B.Key)->MountRank; });
			if (mod.Load.Lost)
				printfWarning("Mods: %s gets %zu of its %zu files from other mods", mod.Key.c_str(), mod.Load.Lost, mod.Load.Files);
		}
	}

	static std::filesystem::path ContentFolder()
	{
		wchar_t ExePath[MAX_PATH] = {};
		GetModuleFileNameW(nullptr, ExePath, MAX_PATH);
		return std::filesystem::path(ExePath).parent_path().parent_path().parent_path() / L"Content"; // MK12\Binaries\Win64\MK12.exe
	}

	struct Files { bool bPak = false; bool bIoStore = false; };
	// Mod (folder, name) -> its paks (folder under the mod, name) -> which files
	typedef std::map<std::pair<std::wstring, std::wstring>, std::map<std::pair<std::wstring, std::wstring>, Files>> FoundMods;

	// Paks: every pak is a mod. Content\Mods: a folder right under it is one mod with all the paks inside, a pak right
	// in it is a mod of its own. Paks also gives the game pak that mounts first
	static FoundMods Scan(const std::filesystem::path& Root, Source From, std::wstring* LastGamePak)
	{
		FoundMods Found;
		std::error_code Error;
		for (auto It = std::filesystem::recursive_directory_iterator(Root, Error); !Error && It != std::filesystem::recursive_directory_iterator(); It.increment(Error))
		{
			const std::filesystem::path& Path = It->path();
			std::wstring Extension = Lower(Path.extension().wstring());
			if (!It->is_regular_file() || (Extension != L".pak" && Extension != L".utoc"))
				continue;

			std::wstring Name = Path.stem().wstring();
			std::filesystem::path Relative = Path.parent_path().lexically_relative(Root);
			std::wstring Folder = Relative == L"." ? L"" : ForwardSlashes(Relative.wstring());
			if (From == Source::Paks && IsGamePak(Name, Folder))
			{
				std::wstring Key = Lower(Name) + Extension;
				if (LastGamePak && Extension == L".pak" && Key.rfind(L"pakchunk", 0) == 0 && Key > *LastGamePak)
					*LastGamePak = Key;
				continue;
			}

			std::pair<std::wstring, std::wstring> ModAt = { Folder, Name }; // A pak of its own
			std::pair<std::wstring, std::wstring> PakAt = { L"", Name };
			if (From == Source::ModsFolder && !Folder.empty())
			{
				size_t Slash = Folder.find(L'/');
				std::wstring Top = Folder.substr(0, Slash);
				ModAt = { Top, Top };
				PakAt = { Slash == std::wstring::npos ? L"" : Folder.substr(Slash + 1), Name };
			}
			Files& files = Found[ModAt][PakAt];
			(Extension == L".pak" ? files.bPak : files.bIoStore) = true;
		}
		return Found;
	}

	static void AddMods(const FoundMods& Found, Source From, CIniReader& StateIni)
	{
		for (const auto& [Where, Paks] : Found)
		{
			Mod mod;
			mod.From = From;
			mod.Folder = Where.first; // Paks: the pak's folder. Content\Mods: the mod's folder, empty for a pak of its own
			mod.Name = Where.second;
			bool bAllPak = true;
			for (const auto& [PakAt, files] : Paks)
			{
				Container Pak;
				Pak.Folder = PakAt.first;
				Pak.Name = PakAt.second;
				Pak.Type = !files.bIoStore ? Kind::Pak : files.bPak ? Kind::IoStore : Kind::IoStoreOnly;
				Pak.Order = CONTENT_PAK_ORDER;
				bAllPak &= Pak.Type == Kind::Pak;
				mod.Paks.push_back(Pak);
			}
			mod.Type = bAllPak ? Kind::Pak : mod.Paks.size() == 1 ? mod.Paks.front().Type : Kind::IoStore;
			if (From == Source::ModsFolder)
				mod.Priority = PriorityFromName(mod.Name);
			std::wstring Path = From == Source::Paks ? Join(mod.Folder, mod.Name) : mod.Folder.empty() ? mod.Name : mod.Folder;
			mod.Key = Narrow(From == Source::ModsFolder ? MODS_KEY_PREFIX + Path : Path);
			mod.bEnabled = StateIni.ReadBoolean((char*)StateFile::MODS, (char*)mod.Key.c_str(), true); // Missing is on
			if (!mod.bEnabled && From == Source::Paks)
				DisabledKeys.push_back(Lower(Path));
			Mods.push_back(std::move(mod));
		}
	}

	void Init()
	{
		Mods.clear();
		DisabledKeys.clear();
		std::filesystem::path Content = ContentFolder();
		PaksRoot = Content / L"Paks";
		ModsRoot = Content / L"Mods";

		std::wstring LastGamePak; // The game's pakchunk pak that mounts first: Paks mods sorting below it mount after some game files
		CIniReader StateIni((char*)StateFile::FILE_NAME);
		AddMods(Scan(ModsRoot, Source::ModsFolder, nullptr), Source::ModsFolder, StateIni);
		AddMods(Scan(PaksRoot, Source::Paks, &LastGamePak), Source::Paks, StateIni);

		// Content\Mods first, then Paks, each by path
		std::sort(Mods.begin(), Mods.end(), [](const Mod& A, const Mod& B)
		{
			if (A.From != B.From)
				return A.From == Source::ModsFolder;
			return Lower(Join(A.Folder, A.Name)) < Lower(Join(B.Folder, B.Name));
		});
		size_t Off = std::count_if(Mods.begin(), Mods.end(), [](const Mod& mod) { return !mod.bEnabled; });
		size_t InModsFolder = std::count_if(Mods.begin(), Mods.end(), [](const Mod& mod) { return mod.From == Source::ModsFolder; });
		printfInfo("Mods: %zu found (%zu in Content\\Mods), %zu turned off", Mods.size(), InModsFolder, Off);
		OrderModsFolder();
		RankMounts();
		AnalyzeLoading(LastGamePak);
	}

	const Mod* Find(const std::string& Key)
	{
		for (const Mod& mod : Mods)
		{
			if (mod.Key == Key)
				return &mod;
		}
		return nullptr;
	}

	const std::vector<Mod>& List()
	{
		return Mods;
	}

	const std::filesystem::path& Root(Source From)
	{
		return From == Source::ModsFolder ? ModsRoot : PaksRoot;
	}

	std::filesystem::path FileOf(const Mod& mod, const Container& Pak, const wchar_t* Extension)
	{
		return Root(mod.From) / mod.Folder / Pak.Folder / (Pak.Name + Extension);
	}

	bool IsDisabled(const wchar_t* PakPath)
	{
		if (!PakPath || DisabledKeys.empty())
			return false;

		std::wstring Path = Lower(ForwardSlashes(PakPath));
		size_t Marker = Path.find(PAKS_MARKER);
		if (Marker == std::wstring::npos)
			return false;

		std::wstring Key = Path.substr(Marker + wcslen(PAKS_MARKER));
		size_t Dot = Key.rfind(L'.');
		if (Dot != std::wstring::npos && Key.find(L'/', Dot) == std::wstring::npos)
			Key.resize(Dot);

		bool bDisabled = std::find(DisabledKeys.begin(), DisabledKeys.end(), Key) != DisabledKeys.end();
		if (bDisabled)
			printfInfo("Mods: Skipped %ls (turned off)", PakPath);
		return bDisabled;
	}

	// The game's mount sets up the IoStore container next to the .pak path even when the .pak isn't there
	static void MountPak(void* PakPlatformFile, MountType* Mount, const Mod& mod, const Container& Pak)
	{
		std::filesystem::path PakPath = FileOf(mod, Pak, L".pak");
		__int64 Result = Mount(PakPlatformFile, PakPath.c_str(), Pak.Order, nullptr, true);
		std::string Name = Narrow(Join(Pak.Folder, Pak.Name));
		if (Pak.Type == Kind::IoStoreOnly)
			printfInfo("Mods: Mounted %s: %s without a .pak, priority %u (%s)", mod.Key.c_str(), Name.c_str(), Pak.Order, Result ? "pak layer too" : "IoStore only");
		else
			printfInfo("Mods: Mounted %s: %s, priority %u%s", mod.Key.c_str(), Name.c_str(), Pak.Order, Result ? "" : " (the game refused it)");
	}

	void MountOurs(void* PakPlatformFile, MountType* Mount)
	{
		if (bMounted || !PakPlatformFile || !Mount)
			return;
		bMounted = true;

		std::vector<const Mod*> InOrder;
		for (const Mod& mod : Mods)
		{
			if (mod.bEnabled)
				InOrder.push_back(&mod);
		}
		std::sort(InOrder.begin(), InOrder.end(), [](const Mod* A, const Mod* B) { return A->MountRank < B->MountRank; });
		for (const Mod* mod : InOrder)
		{
			for (const Container& Pak : mod->Paks)
			{
				if (mod->From == Source::ModsFolder || Pak.Type == Kind::IoStoreOnly)
					MountPak(PakPlatformFile, Mount, *mod, Pak);
			}
		}
	}
}
