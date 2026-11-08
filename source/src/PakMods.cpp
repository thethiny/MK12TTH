#include "mkutils.h"
#include "PakMods.h"
#include "PakContents.h"
#include "StateFile.h"
#include "mk12.h"
#include "../IniReader.h"
#include <Windows.h>
#include <algorithm>
#include <cwctype>
#include <fstream>
#include <map>
#include <set>
#include <unordered_set>

namespace PakMods {
	constexpr const wchar_t* PAKS_MARKER = L"/content/paks/"; // Mount paths are relative to the game, compared lowercase with forward slashes
	constexpr const wchar_t* MODS_KEY_PREFIX = L"Content/Mods/"; // State file keys of Content\Mods, Paks keys stay as they were
	constexpr const wchar_t* DESCRIPTION_FILE = L"description.txt";
	constexpr size_t DESCRIPTION_MAX_BYTES = 4096;
	constexpr size_t DESCRIPTION_MAX_CHARS = 1500;
	constexpr const wchar_t* GAME_ROOT = L"../../../"; // The game's paths are relative to its exe, the listings start at the game folder
	constexpr uint32_t CONTENT_PAK_ORDER = 3; // What the game gives every pak under Content: its own and the ones in Paks
	constexpr uint32_t MODS_ORDER = CONTENT_PAK_ORDER + 1; // Content\Mods, over the game's files. _P<n> mods get one more plus n

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

	// Content\Mods: one priority for all, a _P<n> mod's own above it. The mount order (RankMounts, last name first)
	// decides between mods with the same one
	static void OrderModsFolder()
	{
		for (Mod& mod : Mods)
		{
			if (mod.From != Source::ModsFolder)
				continue;
			std::sort(mod.Paks.begin(), mod.Paks.end(), [](const Container& A, const Container& B) { return Lower(Join(A.Folder, A.Name)) < Lower(Join(B.Folder, B.Name)); });
			for (Container& Pak : mod.Paks)
				Pak.Order = mod.Priority >= 0 ? MODS_ORDER + 1 + (uint32_t)mod.Priority : MODS_ORDER;
		}
	}

	// Strongest first: higher priority, then the mount order. Paks: our IoStore only mods go first, then last path first.
	// Content\Mods: last path first
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
			if (A->From == Source::Paks && bOursA != bOursB)
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

	// The pak's own description.txt, the one closest to its root. Loose files sit in the .pak, never in the .utoc
	static std::wstring DescriptionIn(const PakContents::Listing& Files)
	{
		std::wstring Found;
		std::wstring Name = std::wstring(L"/") + DESCRIPTION_FILE;
		for (const std::wstring& File : Files.Files)
		{
			bool bMatch = File == DESCRIPTION_FILE || (File.size() > Name.size() && File.compare(File.size() - Name.size(), Name.size(), Name) == 0);
			if (bMatch && (Found.empty() || File.size() < Found.size()))
				Found = File;
		}
		return Found.empty() ? L"" : GAME_ROOT + Found;
	}

	// Which files of each mod the game will really use, against the paks that are on. A mod is checked whole, as if
	// all of it were on
	static void AnalyzeLoading(const std::wstring& LastGamePak)
	{
		struct Contents { std::unordered_set<std::wstring> Pak, IoStore; };
		std::vector<Contents> All(Mods.size()), Loading(Mods.size());
		for (size_t i = 0; i < Mods.size(); i++)
		{
			Mod& mod = Mods[i];
			bool bRead = true;
			for (Container& Pak : mod.Paks)
			{
				if (Pak.Type != Kind::IoStoreOnly)
				{
					PakContents::Listing Files = PakContents::ReadPak(FileOf(mod, Pak, L".pak"));
					bRead &= Files.bRead;
					All[i].Pak.insert(Files.Files.begin(), Files.Files.end());
					Pak.DescriptionInPak = DescriptionIn(Files);
					if (Pak.bLoads)
						Loading[i].Pak.insert(Files.Files.begin(), Files.Files.end());
				}
				if (Pak.Type != Kind::Pak)
				{
					PakContents::Listing Files = PakContents::ReadUtoc(FileOf(mod, Pak, L".utoc"));
					bRead &= Files.bRead;
					All[i].IoStore.insert(Files.Files.begin(), Files.Files.end());
					if (Pak.bLoads)
						Loading[i].IoStore.insert(Files.Files.begin(), Files.Files.end());
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
				if (j == i || !AnyLoads(Other))
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
				Compare(All[i].Pak, Loading[j].Pak);
				Compare(All[i].IoStore, Loading[j].IoStore);
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
			std::wstring Path = From == Source::Paks ? Join(mod.Folder, mod.Name) : mod.Folder.empty() ? mod.Name : mod.Folder;
			mod.Key = Narrow(From == Source::ModsFolder ? MODS_KEY_PREFIX + Path : Path);
			if (From == Source::ModsFolder)
				mod.Priority = PriorityFromName(mod.Name);
			mod.bEnabled = StateIni.ReadBoolean((char*)StateFile::MODS, (char*)mod.Key.c_str(), true); // Missing is on
			if (!mod.bEnabled && From == Source::Paks)
				DisabledKeys.push_back(Lower(Path));
			Mods.push_back(std::move(mod));
		}
	}

	// Without carriage returns and the blank around it, the start of it only
	static std::wstring Trimmed(std::wstring Text)
	{
		Text.erase(std::remove(Text.begin(), Text.end(), L'\r'), Text.end());
		size_t First = Text.find_first_not_of(L" \t\n");
		if (First == std::wstring::npos)
			return L"";
		Text = Text.substr(First, Text.find_last_not_of(L" \t\n") - First + 1);
		return Text.size() > DESCRIPTION_MAX_CHARS ? Text.substr(0, DESCRIPTION_MAX_CHARS) + L"..." : Text;
	}

	// A folder's description.txt: UTF-8 or UTF-16, the start of it only
	static std::wstring ReadDescription(const std::filesystem::path& Path)
	{
		std::ifstream File(Path, std::ios::binary);
		if (!File)
			return L"";
		std::string Bytes(DESCRIPTION_MAX_BYTES, '\0');
		File.read(Bytes.data(), Bytes.size());
		Bytes.resize((size_t)File.gcount());

		std::wstring Text;
		if (Bytes.size() >= 2 && (uint8_t)Bytes[0] == 0xFF && (uint8_t)Bytes[1] == 0xFE)
			Text.assign((const wchar_t*)(Bytes.data() + 2), (Bytes.size() - 2) / sizeof(wchar_t));
		else
		{
			size_t Skip = Bytes.rfind("\xEF\xBB\xBF", 0) == 0 ? 3 : 0;
			int Size = MultiByteToWideChar(CP_UTF8, 0, Bytes.data() + Skip, (int)(Bytes.size() - Skip), nullptr, 0);
			Text.resize(Size);
			MultiByteToWideChar(CP_UTF8, 0, Bytes.data() + Skip, (int)(Bytes.size() - Skip), Text.data(), Size);
		}
		return Trimmed(Text);
	}

	// Folders and their parts compared one level at a time, so a folder's paks and subfolders follow it
	static std::wstring TreeOrder(const std::wstring& Path)
	{
		std::wstring Key = Lower(Path);
		std::replace(Key.begin(), Key.end(), L'/', L'\x01');
		return Key;
	}

	// "a/b/c" -> "a", "a/b", "a/b/c"
	static std::vector<std::wstring> FolderChain(const std::wstring& Folder)
	{
		std::vector<std::wstring> Chain;
		for (size_t Slash = 0; !Folder.empty(); Slash++)
		{
			Slash = Folder.find(L'/', Slash);
			Chain.push_back(Folder.substr(0, Slash));
			if (Slash == std::wstring::npos)
				break;
		}
		return Chain;
	}

	static bool IsInside(const std::wstring& Path, const std::wstring& Folder)
	{
		std::wstring Lowered = Lower(Path), Parent = Lower(Folder);
		return Lowered.size() > Parent.size() && Lowered.compare(0, Parent.size(), Parent) == 0 && Lowered[Parent.size()] == L'/';
	}

	// Content\Mods with several paks: a switch per pak and per folder holding more than one, and what really loads
	static void BuildSwitches(Mod& mod, CIniReader& StateIni)
	{
		for (Container& Pak : mod.Paks)
			Pak.bLoads = mod.bEnabled;
		if (mod.From != Source::ModsFolder || mod.Paks.size() < 2)
			return;
		mod.Description = ReadDescription(ModsRoot / mod.Folder / DESCRIPTION_FILE);

		std::map<std::wstring, size_t> PaksUnder; // Lowercase folder -> paks in it and below
		for (const Container& Pak : mod.Paks)
		{
			for (const std::wstring& Folder : FolderChain(Pak.Folder))
				PaksUnder[Lower(Folder)]++;
		}

		std::vector<size_t> Sorted(mod.Paks.size());
		for (size_t i = 0; i < Sorted.size(); i++)
			Sorted[i] = i;
		std::sort(Sorted.begin(), Sorted.end(), [&](size_t A, size_t B)
		{
			const Container& PakA = mod.Paks[A], &PakB = mod.Paks[B];
			if (Lower(PakA.Folder) != Lower(PakB.Folder))
				return TreeOrder(PakA.Folder) < TreeOrder(PakB.Folder);
			return Lower(PakA.Name) < Lower(PakB.Name);
		});

		std::set<std::wstring> Added;
		for (size_t Index : Sorted)
		{
			const Container& Pak = mod.Paks[Index];
			int Depth = 1;
			for (const std::wstring& Folder : FolderChain(Pak.Folder))
			{
				if (PaksUnder[Lower(Folder)] < 2)
					continue;
				if (Added.insert(Lower(Folder)).second)
				{
					size_t Slash = Folder.rfind(L'/');
					Switch Entry;
					Entry.Path = Folder;
					Entry.Name = Slash == std::wstring::npos ? Folder : Folder.substr(Slash + 1);
					Entry.bFolder = true;
					Entry.Depth = Depth;
					Entry.Description = ReadDescription(ModsRoot / mod.Folder / Folder / DESCRIPTION_FILE);
					mod.Switches.push_back(std::move(Entry));
				}
				Depth++;
			}
			Switch Entry;
			Entry.Path = Join(Pak.Folder, Pak.Name);
			Entry.Name = Pak.Name;
			Entry.Depth = Depth;
			Entry.Pak = Index;
			mod.Switches.push_back(std::move(Entry));
		}

		for (Switch& Entry : mod.Switches)
		{
			Entry.Key = mod.Key + "/" + Narrow(Entry.Path);
			Entry.bEnabled = StateIni.ReadBoolean((char*)StateFile::MODS, (char*)Entry.Key.c_str(), true);
		}
		for (const Switch& Entry : mod.Switches)
		{
			if (Entry.bFolder)
				continue;
			Container& Pak = mod.Paks[Entry.Pak];
			Pak.bLoads = mod.bEnabled && Entry.bEnabled;
			for (const Switch& Folder : mod.Switches)
			{
				if (Folder.bFolder && !Folder.bEnabled && (IsInside(Entry.Path, Folder.Path)))
					Pak.bLoads = false;
			}
		}
	}

	bool AnyLoads(const Mod& mod)
	{
		return std::any_of(mod.Paks.begin(), mod.Paks.end(), [](const Container& Pak) { return Pak.bLoads; });
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
		OrderModsFolder();
		for (Mod& mod : Mods)
			BuildSwitches(mod, StateIni);
		size_t Off = std::count_if(Mods.begin(), Mods.end(), [](const Mod& mod) { return !AnyLoads(mod); });
		size_t InModsFolder = std::count_if(Mods.begin(), Mods.end(), [](const Mod& mod) { return mod.From == Source::ModsFolder; });
		printfInfo("Mods: %zu found (%zu in Content\\Mods), %zu turned off", Mods.size(), InModsFolder, Off);
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
	static void MountPak(void* PakPlatformFile, MountType* Mount, const Mod& mod, Container& Pak)
	{
		std::filesystem::path PakPath = FileOf(mod, Pak, L".pak");
		__int64 Result = Mount(PakPlatformFile, PakPath.c_str(), Pak.Order, nullptr, true);
		std::string Name = Narrow(Join(Pak.Folder, Pak.Name));
		if (Pak.Type == Kind::IoStoreOnly)
			printfInfo("Mods: Mounted %s: %s without a .pak, priority %u (%s)", mod.Key.c_str(), Name.c_str(), Pak.Order, Result ? "pak layer too" : "IoStore only");
		else
			printfInfo("Mods: Mounted %s: %s, priority %u", mod.Key.c_str(), Name.c_str(), Pak.Order); // The result says nothing: mods that load return 0 too
		if (!Pak.DescriptionInPak.empty() && MK12::LoadFileToString)
		{
			MK12::FString Text = { nullptr, 0, 0 };
			if (MK12::LoadFileToString(&Text, Pak.DescriptionInPak.c_str(), 0, 0) && Text.Data)
				Pak.Description = Trimmed(Text.Data);
			if (Text.Data && MK12::MemoryFree)
				MK12::MemoryFree(Text.Data);
		}
	}

	void MountOurs(void* PakPlatformFile, MountType* Mount)
	{
		if (bMounted || !PakPlatformFile || !Mount)
			return;
		bMounted = true;

		std::vector<Mod*> InOrder;
		for (Mod& mod : Mods)
		{
			if (AnyLoads(mod))
				InOrder.push_back(&mod);
		}
		std::sort(InOrder.begin(), InOrder.end(), [](const Mod* A, const Mod* B) { return A->MountRank < B->MountRank; });
		for (Mod* mod : InOrder)
		{
			for (Container& Pak : mod->Paks)
			{
				if (!Pak.bLoads)
					printfInfo("Mods: Skipped %s: %s (turned off)", mod->Key.c_str(), Narrow(Join(Pak.Folder, Pak.Name)).c_str());
				else if (mod->From == Source::ModsFolder || Pak.Type == Kind::IoStoreOnly)
					MountPak(PakPlatformFile, Mount, *mod, Pak);
			}
		}
	}
}
