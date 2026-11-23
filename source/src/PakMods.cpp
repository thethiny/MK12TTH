#include "mkutils.h"
#include "PakMods.h"
#include "PakContents.h"
#include "StateFile.h"
#include "../IniReader.h"
#include <Windows.h>
#include <algorithm>
#include <cwctype>
#include <map>
#include <unordered_map>

namespace PakMods {
	constexpr const wchar_t*	PAKS_MARKER				= L"/content/paks/";	// Mount paths compared lowercase with forward slashes
	constexpr const wchar_t*	MODS_KEY_PREFIX			= L"Content/Mods/";		// State file keys, Paks keys stay as they were
	constexpr uint32_t			CONTENT_PAK_ORDER		= 3;					// Every pak under Content: the game's and Paks
	constexpr uint32_t			FIRST_MOD_ORDER			= CONTENT_PAK_ORDER + 1; // Content\Mods, over the game's files

	static std::vector<Mod>				Mods;
	static std::vector<std::wstring>	DisabledKeys; // Paks, lowercase
	static std::wstring					PaksRoot;
	static std::wstring					ModsRoot;
	static bool							bMounted = false;

	static std::wstring Lower(std::wstring Text)
	{
		for (size_t i = 0; i < Text.size(); i++)
			Text[i] = (wchar_t)towlower(Text[i]);
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

	static bool StartsWith(const std::wstring& Text, const wchar_t* Prefix)
	{
		return Text.rfind(Prefix, 0) == 0;
	}

	static bool EndsWith(const std::wstring& Text, const wchar_t* Suffix)
	{
		size_t Length = wcslen(Suffix);
		return Text.size() >= Length && Text.compare(Text.size() - Length, Length, Suffix) == 0;
	}

	std::wstring PathAppend(const std::wstring& Path, const std::wstring& Part)
	{
		std::wstring Out = Path;
		if (!Out.empty() && Out[Out.size() - 1] != L'\\' && Out[Out.size() - 1] != L'/')
			Out += L'\\';
		return Out + Part;
	}

	static std::string Narrow(const std::wstring& Text)
	{
		int Size = WideCharToMultiByte(CP_UTF8, 0, Text.c_str(), (int)Text.size(), nullptr, 0, nullptr, nullptr);
		std::string Out(Size, '\0');
		if (Size > 0)
			WideCharToMultiByte(CP_UTF8, 0, Text.c_str(), (int)Text.size(), &Out[0], Size, nullptr, nullptr);
		return Out;
	}

	static bool IsGamePak(const std::wstring& Name, const std::wstring& Folder)
	{ // pakchunkN-WindowsNoEditor, pakpcN-WindowsNoEditor and global, right in Paks
		if (!Folder.empty())
			return false;
		std::wstring Lowered = Lower(Name);
		return Lowered == L"global" || ((StartsWith(Lowered, L"pakchunk") || StartsWith(Lowered, L"pakpc")) && EndsWith(Lowered, L"-windowsnoeditor"));
	}

	static int PriorityFromName(const std::wstring& Name)
	{ // "Name_P12" -> 12. A plain "_P" is the engine's patch naming, no priority
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

	// What the game sorts Paks by: the path under Paks without case
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
		for (size_t i = 0; i < mod.Paks.size(); i++)
			Top = (std::max)(Top, mod.Paks[i].Order);
		return Top;
	}

	static bool ComparePaks(const Container& A, const Container& B)
	{
		return Lower(Join(A.Folder, A.Name)) < Lower(Join(B.Folder, B.Name));
	}

	static bool WeakerMod(const Mod* A, const Mod* B)
	{ // Mods with .utoc/.ucas, then .pak only mods, then _P<n> mods by n. Name inside each, the last one wins like in Paks
		if ((A->Priority >= 0) != (B->Priority >= 0))
			return A->Priority < 0;
		if (A->Priority != B->Priority)
			return A->Priority < B->Priority;
		bool bPakA = A->Type == Kind::Pak, bPakB = B->Type == Kind::Pak;
		if (bPakA != bPakB)
			return !bPakA;
		return Lower(A->Name) < Lower(B->Name);
	}

	// Content\Mods priorities, weakest first. A mod's own paks follow each other, by name
	static void OrderModsFolder()
	{
		std::vector<Mod*> Ours;
		for (size_t i = 0; i < Mods.size(); i++)
		{
			if (Mods[i].From == Source::ModsFolder)
				Ours.push_back(&Mods[i]);
		}
		std::sort(Ours.begin(), Ours.end(), WeakerMod);
		uint32_t Next = FIRST_MOD_ORDER;
		for (size_t i = 0; i < Ours.size(); i++)
		{
			Mod* mod = Ours[i];
			std::sort(mod->Paks.begin(), mod->Paks.end(), ComparePaks);
			for (size_t p = 0; p < mod->Paks.size(); p++)
				mod->Paks[p].Order = Next++;
		}
	}

	static bool StrongerMount(const Mod* A, const Mod* B)
	{
		if (TopOrder(*A) != TopOrder(*B))
			return TopOrder(*A) > TopOrder(*B);
		bool bOursA = A->Type == Kind::IoStoreOnly, bOursB = B->Type == Kind::IoStoreOnly;
		if (bOursA != bOursB)
			return bOursA;
		return MountKey(*A) > MountKey(*B);
	}

	// Strongest first: priority, then the Paks mount order (our IoStore only mods first, then last path first)
	static void RankMounts()
	{
		std::vector<Mod*> Order;
		for (size_t i = 0; i < Mods.size(); i++)
			Order.push_back(&Mods[i]);
		std::stable_sort(Order.begin(), Order.end(), StrongerMount);
		for (size_t i = 0; i < Order.size(); i++)
			Order[i]->MountRank = (int)i;
	}

	static bool IsGameAsset(const std::wstring& File)
	{
		return EndsWith(File, L".uasset") || EndsWith(File, L".umap") || EndsWith(File, L".uexp") || EndsWith(File, L".ubulk");
	}


	// A set of file names, listed in the order they came
	struct FileSet {
		std::vector<std::wstring>				List;
		std::unordered_map<std::wstring, bool>	Has;

		void Add(const std::vector<std::wstring>& Files)
		{
			for (size_t i = 0; i < Files.size(); i++)
			{
				if (!Has.count(Files[i]))
				{
					Has[Files[i]] = true;
					List.push_back(Files[i]);
				}
			}
		}
	};

	struct Contents {
		FileSet Pak;
		FileSet IoStore;
	};

	// Shared files of Mine that Theirs has too, the ones Mine loses go to Lost
	static size_t CountShared(const FileSet& Mine, const FileSet& Theirs, bool bOtherWins, std::unordered_map<std::wstring, bool>& Lost)
	{
		size_t Shared = 0;
		for (size_t i = 0; i < Mine.List.size(); i++)
		{
			if (!Theirs.Has.count(Mine.List[i]))
				continue;
			Shared++;
			if (bOtherWins)
				Lost[Mine.List[i]] = true;
		}
		return Shared;
	}

	static bool StrongerOverride(const Override& A, const Override& B)
	{
		return Find(A.Key)->MountRank < Find(B.Key)->MountRank;
	}

	// Which files of each mod the game really uses, against the mods that are on
	static void AnalyzeLoading(const std::wstring& LastGamePak)
	{
		std::vector<Contents> All(Mods.size());
		for (size_t i = 0; i < Mods.size(); i++)
		{
			Mod& mod = Mods[i];
			bool bRead = true;
			for (size_t p = 0; p < mod.Paks.size(); p++)
			{
				const Container& Pak = mod.Paks[p];
				if (Pak.Type != Kind::IoStoreOnly)
				{
					PakContents::Listing Files = PakContents::ReadPak(FileOf(mod, Pak, L".pak"));
					bRead &= Files.bRead;
					All[i].Pak.Add(Files.Files);
				}
				if (Pak.Type != Kind::Pak)
				{
					PakContents::Listing Files = PakContents::ReadUtoc(FileOf(mod, Pak, L".utoc"));
					bRead &= Files.bRead;
					All[i].IoStore.Add(Files.Files);
				}
			}
			mod.Load.bContentsRead = bRead;
			mod.Load.Files = All[i].Pak.List.size() + All[i].IoStore.List.size();
			bool bAssets = false;
			for (size_t f = 0; f < All[i].Pak.List.size() && !bAssets; f++)
				bAssets = IsGameAsset(All[i].Pak.List[f]);
			mod.Load.bAssetsInPakOnly = mod.Type == Kind::Pak && bAssets;
			mod.Load.bAfterGameFiles = mod.From == Source::Paks && mod.Type != Kind::IoStoreOnly && !LastGamePak.empty() && MountKey(mod) < LastGamePak;
			mod.Load.LastGamePak = LastGamePak;
		}

		for (size_t i = 0; i < Mods.size(); i++)
		{
			Mod& mod = Mods[i];
			std::unordered_map<std::wstring, bool> Lost;
			for (size_t j = 0; j < Mods.size(); j++)
			{
				const Mod& Other = Mods[j];
				if (j == i || !Other.bEnabled)
					continue;
				bool bOtherWins = Other.MountRank < mod.MountRank;
				size_t Shared = CountShared(All[i].Pak, All[j].Pak, bOtherWins, Lost);
				Shared += CountShared(All[i].IoStore, All[j].IoStore, bOtherWins, Lost);
				if (Shared)
				{
					Override With = { Other.Key, Shared };
					(bOtherWins ? mod.Load.LostTo : mod.Load.WinsOver).push_back(With);
				}
			}
			mod.Load.Lost = Lost.size();
			std::sort(mod.Load.LostTo.begin(), mod.Load.LostTo.end(), StrongerOverride);
			if (mod.Load.Lost)
				printfWarning("Mods: %s gets %zu of its %zu files from other mods", mod.Key.c_str(), mod.Load.Lost, mod.Load.Files);
		}
	}

	static std::wstring ParentFolder(const std::wstring& Path)
	{
		size_t Slash = Path.find_last_of(L"\\/");
		return Slash == std::wstring::npos ? L"" : Path.substr(0, Slash);
	}

	static std::wstring ContentFolder()
	{ // MK12\Binaries\Win64\MK12.exe
		wchar_t ExePath[MAX_PATH] = {};
		GetModuleFileNameW(nullptr, ExePath, MAX_PATH);
		return PathAppend(ParentFolder(ParentFolder(ParentFolder(ExePath))), L"Content");
	}

	// A pak of a mod found by Scan
	struct FoundPak {
		std::wstring	ModFolder;
		std::wstring	ModName;
		std::wstring	PakFolder;	// Under the mod
		std::wstring	PakName;
		bool			bPak;
		bool			bIoStore;
	};

	struct ScanState {
		std::wstring					Root;
		Source							From;
		std::wstring*					LastGamePak;
		std::vector<FoundPak>			Found;
		std::map<std::wstring, size_t>	Index; // Mod and pak -> its place in Found
		bool							bStop;	// A folder couldn't be listed, like the game's directory iterator we stop there
	};

	static void AddFound(ScanState& Scan, const std::wstring& Folder, const std::wstring& Name, const std::wstring& Extension)
	{
		if (Scan.From == Source::Paks && IsGamePak(Name, Folder))
		{
			std::wstring Key = Lower(Name) + Extension;
			if (Scan.LastGamePak && Extension == L".pak" && StartsWith(Key, L"pakchunk") && Key > *Scan.LastGamePak)
				*Scan.LastGamePak = Key;
			return;
		}

		FoundPak Pak = { Folder, Name, L"", Name, false, false }; // A pak of its own
		if (Scan.From == Source::ModsFolder && !Folder.empty())
		{
			size_t Slash = Folder.find(L'/');
			std::wstring Top = Folder.substr(0, Slash);
			Pak.ModFolder = Top;
			Pak.ModName = Top;
			Pak.PakFolder = Slash == std::wstring::npos ? L"" : Folder.substr(Slash + 1);
		}
		std::wstring Key = Pak.ModFolder + L"\n" + Pak.ModName + L"\n" + Pak.PakFolder + L"\n" + Pak.PakName;
		if (!Scan.Index.count(Key))
		{
			Scan.Index[Key] = Scan.Found.size();
			Scan.Found.push_back(Pak);
		}
		FoundPak& Entry = Scan.Found[Scan.Index[Key]];
		if (Extension == L".pak")
			Entry.bPak = true;
		else
			Entry.bIoStore = true;
	}

	static void ScanFolder(ScanState& Scan, const std::wstring& Folder)
	{ // Folder: under the root, forward slashes
		if (Scan.bStop)
			return;
		std::wstring Dir = Folder.empty() ? Scan.Root : PathAppend(Scan.Root, Folder);
		WIN32_FIND_DATAW Data;
		HANDLE Find = FindFirstFileW(PathAppend(Dir, L"*").c_str(), &Data);
		if (Find == INVALID_HANDLE_VALUE)
		{
			Scan.bStop = true;
			return;
		}
		do
		{
			std::wstring Name = Data.cFileName;
			if (Name == L"." || Name == L"..")
				continue;
			if (Data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				// Directory symlinks aren't followed, junctions are
				if (!(Data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) || Data.dwReserved0 != IO_REPARSE_TAG_SYMLINK)
					ScanFolder(Scan, Join(Folder, Name));
				if (Scan.bStop)
					break;
				continue;
			}
			size_t Dot = Name.rfind(L'.');
			if (Dot == std::wstring::npos || Dot == 0)
				continue;
			std::wstring Extension = Lower(Name.substr(Dot));
			if (Extension == L".pak" || Extension == L".utoc")
				AddFound(Scan, Folder, Name.substr(0, Dot), Extension);
		} while (FindNextFileW(Find, &Data));
		FindClose(Find);
	}

	static bool CompareFound(const FoundPak& A, const FoundPak& B)
	{ // Mod (folder, name), then pak (folder, name)
		if (A.ModFolder != B.ModFolder)
			return A.ModFolder < B.ModFolder;
		if (A.ModName != B.ModName)
			return A.ModName < B.ModName;
		if (A.PakFolder != B.PakFolder)
			return A.PakFolder < B.PakFolder;
		return A.PakName < B.PakName;
	}

	// Paks: every pak is a mod. Content\Mods: a folder right under it is one mod with all the paks inside, a pak right in
	// it is a mod of its own. Paks also gives the game pak that mounts first
	static std::vector<FoundPak> Scan(const std::wstring& Root, Source From, std::wstring* LastGamePak)
	{
		ScanState State;
		State.Root = Root;
		State.From = From;
		State.LastGamePak = LastGamePak;
		State.bStop = false;
		ScanFolder(State, L"");
		std::sort(State.Found.begin(), State.Found.end(), CompareFound);
		return State.Found;
	}

	static void AddMods(const std::vector<FoundPak>& Found, Source From, CIniReader& StateIni)
	{
		for (size_t First = 0; First < Found.size();)
		{
			Mod mod;
			mod.From = From;
			mod.Folder = Found[First].ModFolder; // Paks: the pak's folder. Content\Mods: the mod's folder, empty for a pak of its own
			mod.Name = Found[First].ModName;
			bool bAllPak = true;
			size_t Next = First;
			for (; Next < Found.size() && Found[Next].ModFolder == mod.Folder && Found[Next].ModName == mod.Name; Next++)
			{
				Container Pak;
				Pak.Folder = Found[Next].PakFolder;
				Pak.Name = Found[Next].PakName;
				Pak.Type = !Found[Next].bIoStore ? Kind::Pak : Found[Next].bPak ? Kind::IoStore : Kind::IoStoreOnly;
				Pak.Order = CONTENT_PAK_ORDER;
				bAllPak &= Pak.Type == Kind::Pak;
				mod.Paks.push_back(Pak);
			}
			First = Next;
			mod.Type = bAllPak ? Kind::Pak : mod.Paks.size() == 1 ? mod.Paks.front().Type : Kind::IoStore;
			if (From == Source::ModsFolder)
				mod.Priority = PriorityFromName(mod.Name);
			std::wstring Path = From == Source::Paks ? Join(mod.Folder, mod.Name) : mod.Folder.empty() ? mod.Name : mod.Folder;
			mod.Key = Narrow(From == Source::ModsFolder ? MODS_KEY_PREFIX + Path : Path);
			mod.bEnabled = StateIni.ReadBoolean((char*)StateFile::MODS, (char*)mod.Key.c_str(), true);
			if (!mod.bEnabled && From == Source::Paks)
				DisabledKeys.push_back(Lower(Path));
			Mods.push_back(mod);
		}
	}


	static bool CompareMods(const Mod& A, const Mod& B)
	{ // Content\Mods first, then Paks, each by path
		if (A.From != B.From)
			return A.From == Source::ModsFolder;
		return Lower(Join(A.Folder, A.Name)) < Lower(Join(B.Folder, B.Name));
	}

	void Init()
	{
		Mods.clear();
		DisabledKeys.clear();
		std::wstring Content = ContentFolder();
		PaksRoot = PathAppend(Content, L"Paks");
		ModsRoot = PathAppend(Content, L"Mods");

		std::wstring LastGamePak; // The game's pakchunk that mounts first: Paks mods sorting below it mount after some game files
		CIniReader StateIni((char*)StateFile::FILE_NAME);
		AddMods(Scan(ModsRoot, Source::ModsFolder, nullptr), Source::ModsFolder, StateIni);
		AddMods(Scan(PaksRoot, Source::Paks, &LastGamePak), Source::Paks, StateIni);

		std::sort(Mods.begin(), Mods.end(), CompareMods);
		size_t Off = 0, InModsFolder = 0;
		for (size_t i = 0; i < Mods.size(); i++)
		{
			if (!Mods[i].bEnabled)
				Off++;
			if (Mods[i].From == Source::ModsFolder)
				InModsFolder++;
		}
		printfInfo("Mods: %zu found (%zu in Content\\Mods), %zu turned off", Mods.size(), InModsFolder, Off);
		OrderModsFolder();
		RankMounts();
		AnalyzeLoading(LastGamePak);
	}

	const Mod* Find(const std::string& Key)
	{
		for (size_t i = 0; i < Mods.size(); i++)
		{
			if (Mods[i].Key == Key)
				return &Mods[i];
		}
		return nullptr;
	}

	const std::vector<Mod>& List()
	{
		return Mods;
	}

	const std::wstring& Root(Source From)
	{
		return From == Source::ModsFolder ? ModsRoot : PaksRoot;
	}

	std::wstring FileOf(const Mod& mod, const Container& Pak, const wchar_t* Extension)
	{
		return PathAppend(PathAppend(PathAppend(Root(mod.From), mod.Folder), Pak.Folder), Pak.Name + Extension);
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

	// The game's mount sets up the IoStore container next to the .pak path, even without a .pak
	static void MountPak(void* PakPlatformFile, MountType* Mount, const Mod& mod, const Container& Pak)
	{
		std::wstring PakPath = FileOf(mod, Pak, L".pak");
		__int64 Result = Mount(PakPlatformFile, PakPath.c_str(), Pak.Order, nullptr, true);
		std::string Name = Narrow(Join(Pak.Folder, Pak.Name));
		if (Pak.Type == Kind::IoStoreOnly)
			printfInfo("Mods: Mounted %s: %s without a .pak, priority %u (%s)", mod.Key.c_str(), Name.c_str(), Pak.Order, Result ? "pak layer too" : "IoStore only");
		else
			printfInfo("Mods: Mounted %s: %s, priority %u%s", mod.Key.c_str(), Name.c_str(), Pak.Order, Result ? "" : " (the game refused it)");
	}

	static bool CompareRank(const Mod* A, const Mod* B)
	{
		return A->MountRank < B->MountRank;
	}

	void MountOurs(void* PakPlatformFile, MountType* Mount)
	{
		if (bMounted || !PakPlatformFile || !Mount)
			return;
		bMounted = true;

		std::vector<const Mod*> InOrder;
		for (size_t i = 0; i < Mods.size(); i++)
		{
			if (Mods[i].bEnabled)
				InOrder.push_back(&Mods[i]);
		}
		std::sort(InOrder.begin(), InOrder.end(), CompareRank);
		for (size_t i = 0; i < InOrder.size(); i++)
		{
			const Mod* mod = InOrder[i];
			for (size_t p = 0; p < mod->Paks.size(); p++)
			{
				const Container& Pak = mod->Paks[p];
				if (mod->From == Source::ModsFolder || Pak.Type == Kind::IoStoreOnly)
					MountPak(PakPlatformFile, Mount, *mod, Pak);
			}
		}
	}
}
