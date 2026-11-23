#include "eSettingsManager.h"
#include "StateFile.h"
#include <Windows.h>

eSettingsManager*		SettingsMgr			= new eSettingsManager();
eFirstRunManager*		FirstRunMgr			= new eFirstRunManager;
eCachedPatternsManager*	CachedPatternsMgr	= new eCachedPatternsManager;
__int64 eCachedPatternsManager::GameAddr = reinterpret_cast<__int64>(GetModuleHandle(nullptr));

// Patterns added after 1.0.0: an ini without the key (an older MK12TTH.ini) gets the one this version shipped with.
// A key in the ini always wins, an empty one turns its feature off
namespace NewPatterns {
	constexpr const char* FindScript				= "69 C9 93 01 00 01 33 C8 48 83 EA 01 75 ? E8 ? ? ? ? 4C 8B F0 48 85 C0";
	constexpr const char* FightStartupCall			= "48 8B F9 E8 ? ? ? ? BA 01 00 00 00 B9 20 01 00 00 E8 ? ? ? ? 33 DB 48 85 C0 74";
	constexpr const char* FightStartupCallCpu		= "48 85 C9 74 05 E8 ? ? ? ? 48 8B CE E8 ? ? ? ? 48 8B 5C";
	constexpr const char* TransientPackageCall		= "E8 ? ? ? ? 4C 8B 44 24 78 48 8B C8 48 89 5C 24 38 45 33 C9 48 89 5C 24 30 48 8B D7 88 5C 24 28";
	constexpr const char* StaticFindObject			= "48 89 5C 24 08 48 89 74 24 18 55 57 41 54 41 56 41 57 48 8B EC 48 83 EC 60 80 3D ? ? ? ? 00 45 0F B6 F1 49 8B F8 48 8B DA 4C 8B F9";
	constexpr const char* StaticLoadObject			= "4C 89 4C 24 20 48 89 54 24 10 48 89 4C 24 08 55 53 56 57 41 54 41 55 41 56 41 57 48 8B EC 48 83 EC 68 45 33 F6";
	constexpr const char* StaticConstructObject		= "48 89 5C 24 10 48 89 74 24 18 55 57 41 54 41 56 41 57 48 8D AC 24 50 FF FF FF 48 81 EC B0 01 00 00 48 8B 05 ? ? ? ? 48 33 C4 48 89 85 A8 00 00 00 48 8B 39 4C 8D 25 ? ? ? ? 4C 8B 79 08 48 8B D9 8B 71 18 4C 8B 71 28 F7 87 CC 00 00 00 80 00 00 10";
	constexpr const char* ObjectArray				= "0F BA EB 19 83 E0 FB 89 47 08 45 33 C0 48 8D 0D ? ? ? ? 48 8B D7 E8";
}

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

// The cache file: "TPCF", version, build count, then per build: hash, entry count, entries (key length, key, offset).
// Older caches were ini files: [<build hash, 16 hex>] or [<upper half of the hash, 8 hex>.<mod version>] sections of
// "<pattern>=<offset, hex>" lines; they are read once and written again as the binary file
static const char		CACHE_MAGIC[4]		= { 'T', 'P', 'C', 'F' }; // thethiny pattern cache file
static const uint32_t	CACHE_VERSION		= 1;
static const DWORD		WRITE_DELAY_MS		= 1000; // Finds made close together go out in one write

static void PutBytes(std::string& Out, const void* Value, size_t Size)
{
	Out.append((const char*)Value, Size);
}

static bool TakeBytes(const std::string& In, size_t& At, void* Value, size_t Size)
{
	if (At + Size > In.size())
		return false;
	memcpy(Value, In.data() + At, Size);
	At += Size;
	return true;
}

static DWORD WINAPI PatternCacheWriter(LPVOID Self)
{
	Sleep(WRITE_DELAY_MS);
	((eCachedPatternsManager*)Self)->WriteQueued();
	return 0;
}

uint64_t* eCachedPatternsManager::Find(Build& In, const std::string& key)
{
	if (!In.Index.count(key))
		return nullptr;
	return &In.Offsets[In.Index[key]];
}

void eCachedPatternsManager::Put(Build& In, const std::string& key, uint64_t offset)
{
	uint64_t* Stored = Find(In, key);
	if (Stored)
	{
		*Stored = offset;
		return;
	}
	In.Index[key] = In.Keys.size();
	In.Keys.push_back(key);
	In.Offsets.push_back(offset);
}

void eCachedPatternsManager::Init(uint64_t Hash, const char* name)
{
	char ModulePath[MAX_PATH * 4] = {};
	GetModuleFileNameA((HINSTANCE)&__ImageBase, ModulePath, sizeof(ModulePath));
	std::string Folder = ModulePath;
	Folder = Folder.substr(0, Folder.find_last_of("/\\") + 1);

	AcquireSRWLockExclusive(&Lock);
	Path = Folder + name + ".cache";
	Builds.clear();
	Load(Hash);
	bEnabled = Hash != 0; // Unknown build: nothing is read or written for it
	if (bEnabled)
	{
		// The running build goes first, past MAX_BUILDS the oldest ones are dropped
		size_t Running = Builds.size();
		for (size_t i = 0; i < Builds.size(); i++)
		{
			if (Builds[i].Hash == Hash)
				Running = i;
		}
		if (Running == Builds.size())
		{
			Build New;
			New.Hash = Hash;
			Builds.insert(Builds.begin(), New);
			bDirty = true;
		}
		else if (Running != 0)
		{
			Build Moved = Builds[Running];
			Builds.erase(Builds.begin() + Running);
			Builds.insert(Builds.begin(), Moved);
			bDirty = true;
		}
		if (Builds.size() > MAX_BUILDS)
		{
			Builds.resize((size_t)MAX_BUILDS);
			bDirty = true;
		}
		if (bDirty)
			QueueWrite();
	}
	ReleaseSRWLockExclusive(&Lock);
}

void eCachedPatternsManager::Load(uint64_t Hash)
{
	HANDLE File = CreateFileA(Path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
	if (File == INVALID_HANDLE_VALUE)
		return;
	std::string Bytes;
	LARGE_INTEGER Size = {};
	if (GetFileSizeEx(File, &Size) && Size.QuadPart > 0 && Size.QuadPart < (1ll << 30))
	{
		Bytes.resize((size_t)Size.QuadPart);
		DWORD Read = 0;
		if (!ReadFile(File, &Bytes[0], (DWORD)Bytes.size(), &Read, nullptr))
			Read = 0;
		Bytes.resize(Read);
	}
	CloseHandle(File);

	if (Bytes.size() >= sizeof(CACHE_MAGIC) && memcmp(Bytes.data(), CACHE_MAGIC, sizeof(CACHE_MAGIC)) == 0)
	{
		if (!LoadBinary(Bytes))
		{
			Builds.clear(); // Unreadable: scanned again and rewritten
			bDirty = true;
		}
	}
	else
	{
		LoadIni(Bytes, Hash);
		bDirty = !Builds.empty(); // An older ini cache: written again as binary
	}
}

bool eCachedPatternsManager::LoadBinary(const std::string& Bytes)
{
	size_t At = sizeof(CACHE_MAGIC);
	uint32_t Version = 0, BuildCount = 0;
	if (!TakeBytes(Bytes, At, &Version, sizeof(Version)) || Version != CACHE_VERSION || !TakeBytes(Bytes, At, &BuildCount, sizeof(BuildCount)))
		return false;
	for (uint32_t b = 0; b < BuildCount; b++)
	{
		Build B;
		uint32_t Count = 0;
		if (!TakeBytes(Bytes, At, &B.Hash, sizeof(B.Hash)) || !TakeBytes(Bytes, At, &Count, sizeof(Count)))
			return false;
		for (uint32_t e = 0; e < Count; e++)
		{
			uint32_t KeyLength = 0;
			uint64_t Offset = 0;
			if (!TakeBytes(Bytes, At, &KeyLength, sizeof(KeyLength)) || At + KeyLength > Bytes.size())
				return false;
			std::string Entry = Bytes.substr(At, KeyLength);
			At += KeyLength;
			if (!TakeBytes(Bytes, At, &Offset, sizeof(Offset)))
				return false;
			Put(B, Entry, Offset);
		}
		Builds.push_back(B);
	}
	return true;
}

// Sections in file order. A section of the older per mod version form ("<upper half>.<version>") goes into the running
// build when its half matches (same exe, the offsets don't depend on the mod version), other ones are dropped
void eCachedPatternsManager::LoadIni(const std::string& Text, uint64_t Hash)
{
	int Current = -1;
	size_t At = 0;
	while (At < Text.size())
	{
		size_t End = Text.find('\n', At);
		if (End == std::string::npos)
			End = Text.size();
		std::string Line = Text.substr(At, End - At);
		At = End + 1;
		while (!Line.empty() && (Line[Line.size() - 1] == '\r' || Line[Line.size() - 1] == ' ' || Line[Line.size() - 1] == '\t'))
			Line.erase(Line.size() - 1);
		if (Line.empty() || Line[0] == ';')
			continue;
		if (Line[0] == '[')
		{
			size_t Close = Line.find(']');
			std::string Name = Line.substr(1, Close == std::string::npos ? std::string::npos : Close - 1);
			uint64_t SectionHash = strtoull(Name.c_str(), nullptr, 16);
			size_t Dot = Name.find('.');
			if (Dot == 8) // Older form: only the upper half of the hash
				SectionHash = Hash && (SectionHash << 32) == (Hash & 0xFFFFFFFF00000000ull) ? Hash : 0;
			Current = -1;
			for (size_t i = 0; i < Builds.size(); i++)
			{
				if (Builds[i].Hash == SectionHash)
					Current = (int)i;
			}
			if (Current == -1 && SectionHash)
			{
				Build New;
				New.Hash = SectionHash;
				Builds.push_back(New);
				Current = (int)Builds.size() - 1;
			}
			continue;
		}
		size_t Equals = Line.rfind('=');
		if (Current == -1 || Equals == std::string::npos)
			continue;
		uint64_t Offset = strtoull(Line.c_str() + Equals + 1, nullptr, 16);
		std::string Entry = Key(Line.substr(0, Equals));
		if (Offset && !Find(Builds[Current], Entry)) // The first section of a build wins
			Put(Builds[Current], Entry, Offset);
	}
}

// The whole cache, written to a temporary file that then replaces it
void eCachedPatternsManager::Write()
{
	std::string Out(CACHE_MAGIC, sizeof(CACHE_MAGIC));
	PutBytes(Out, &CACHE_VERSION, sizeof(CACHE_VERSION));
	uint32_t BuildCount = (uint32_t)Builds.size();
	PutBytes(Out, &BuildCount, sizeof(BuildCount));
	for (size_t b = 0; b < Builds.size(); b++)
	{
		const Build& B = Builds[b];
		uint32_t Count = (uint32_t)B.Keys.size();
		PutBytes(Out, &B.Hash, sizeof(B.Hash));
		PutBytes(Out, &Count, sizeof(Count));
		for (size_t e = 0; e < B.Keys.size(); e++)
		{
			uint32_t KeyLength = (uint32_t)B.Keys[e].size();
			PutBytes(Out, &KeyLength, sizeof(KeyLength));
			Out += B.Keys[e];
			PutBytes(Out, &B.Offsets[e], sizeof(B.Offsets[e]));
		}
	}

	std::string Temp = Path + ".tmp";
	HANDLE File = CreateFileA(Temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (File == INVALID_HANDLE_VALUE)
		return;
	DWORD Written = 0;
	bool bOk = WriteFile(File, Out.data(), (DWORD)Out.size(), &Written, nullptr) && Written == Out.size();
	CloseHandle(File);
	if (bOk && MoveFileExA(Temp.c_str(), Path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		bDirty = false;
	else
		DeleteFileA(Temp.c_str());
}

void eCachedPatternsManager::QueueWrite()
{
	if (bWriteQueued || !bAutoFlush)
		return;
	bWriteQueued = true;
	HANDLE Writer = CreateThread(nullptr, 0, PatternCacheWriter, this, 0, nullptr);
	if (Writer)
		CloseHandle(Writer); // The thread runs on, its handle isn't needed
	else
		bWriteQueued = false; // No writer: the next change queues one again
}

void eCachedPatternsManager::WriteQueued()
{
	AcquireSRWLockExclusive(&Lock);
	bWriteQueued = false;
	if (bDirty)
		Write();
	ReleaseSRWLockExclusive(&Lock);
}

void eCachedPatternsManager::Flush()
{
	AcquireSRWLockExclusive(&Lock);
	if (bDirty && !Path.empty())
		Write();
	ReleaseSRWLockExclusive(&Lock);
}

std::string eCachedPatternsManager::Key(const std::string& pattern)
{
	std::string key;
	size_t i = 0;
	while (i < pattern.size())
	{
		while (i < pattern.size() && isspace((unsigned char)pattern[i]))
			i++;
		size_t start = i;
		while (i < pattern.size() && !isspace((unsigned char)pattern[i]))
			i++;
		if (start == i)
			break;
		std::string token = pattern.substr(start, i - start);
		if (token.find('?') != std::string::npos)
			token = "?";
		for (size_t c = 0; c < token.size(); c++)
			token[c] = (char)toupper((unsigned char)token[c]);
		key += (key.empty() ? "" : " ") + token;
	}
	return key;
}

void eCachedPatternsManager::Set(const std::string& pattern, uint64_t value)
{
	AcquireSRWLockExclusive(&Lock);
	if (bEnabled)
	{
		uint64_t Offset = value > (uint64_t)GameAddr ? value - GameAddr : value;
		std::string Entry = Key(pattern);
		uint64_t* Stored = Find(Builds[0], Entry);
		if (!Stored || *Stored != Offset)
		{
			Put(Builds[0], Entry, Offset);
			bDirty = true;
			QueueWrite();
		}
	}
	ReleaseSRWLockExclusive(&Lock);
}

uint64_t eCachedPatternsManager::Get(const std::string& pattern)
{
	uint64_t Result = 0;
	AcquireSRWLockExclusive(&Lock);
	if (bEnabled)
	{
		uint64_t* Stored = Find(Builds[0], Key(pattern));
		if (Stored && *Stored)
			Result = *Stored + GameAddr;
	}
	ReleaseSRWLockExclusive(&Lock);
	return Result;
}

bool eCachedPatternsManager::Has(const std::string& pattern)
{
	return Get(pattern) != 0;
}

static DWORD WINAPI PaidModWarningThread(LPVOID)
{
	MessageBoxA(0, "Please note that MK12TTH is a free modding tool that is meant to be used with free content.\nIf you have paid for anything, ask for a refund.", "MK12TTH Installed", MB_ICONEXCLAMATION);
	return 0;
}

void eFirstRunManager::Init()
{
	ini = new CIniReader((char*)StateFile::FileName());

	bPaidModWarned				= ini->ReadBoolean	("FirstRun",			"bPaidModWarned",			false);


	if (!bPaidModWarned)
	{
		Save();
		CreateThread(NULL, 0, PaidModWarningThread, NULL, 0, NULL);
	}
}

void eFirstRunManager::Save()
{
	ini->WriteBoolean("FirstRun", "bPaidModWarned", true);
}

void eSettingsManager::Init()
{
	CIniReader ini("");

	// Debug Settings
	bEnableConsoleWindow		= ini.ReadBoolean	("Settings.Debug",		"bEnableConsoleWindow",		false);
	bPauseOnStart				= ini.ReadBoolean	("Settings.Debug",		"bPauseOnStart",			false);
	bDebug						= ini.ReadBoolean	("Settings.Debug",		"bDebug",					false);
	bAllowNonMK					= ini.ReadBoolean	("Settings.Debug",		"bAllowNonMK",				false);
	bFloydDebug					= ini.ReadBoolean	("Settings.Debug",		"bFloydDebug",				false);
	bProfileDebug				= ini.ReadBoolean	("Settings.Debug",		"bProfileDebug",			false);
	bDevLogs					= ini.ReadBoolean	("Settings.Debug",		"bDevLogs",					false);

	// Flood Toggles
	Floods.bFName				= ini.ReadBoolean	("Settings.Debug.FloodToggles",	"bFName",			false);
	Floods.bEndpoint			= ini.ReadBoolean	("Settings.Debug.FloodToggles",	"bEndpoint",		false);
	Floods.bCurl				= ini.ReadBoolean	("Settings.Debug.FloodToggles",	"bCurl",			false);

	// Settings
	iLogSize					= ini.ReadInteger	("Settings",			"iLogSize",					50);
	iLogLevel					= ini.ReadInteger	("Settings",			"iLogLevel",				Log::None);
	szGameVer					= ini.ReadString	("Settings",			"szGameVer",				"0.103");
	szModLoader					= ini.ReadString	("Settings",			"szModLoader",				"Kernel32.CreateFileW");
	szAntiCheatEngine			= ini.ReadString	("Settings",			"szAntiCheatEngine",		"User32.EnumChildWindows");
	szCurlSetOpt				= ini.ReadString	("Settings",			"szCurlSetOpt",				"libcurl.curl_easy_setopt");
	szCurlPerform				= ini.ReadString	("Settings",			"szCurlPerform",			"libcurl.curl_easy_perform");
	bEnableKeyboardHotkeys		= ini.ReadBoolean	("Settings",			"bEnableKeyboardHotkeys",	true);
	// Patches
	szFloydTowerMode			= ini.ReadString	("Patches",				"szFloydTowerMode",			"Game");
	bFloydTowerFromGame			= _stricmp(szFloydTowerMode.c_str(), "Calculated") != 0;
	szAttractModeDelay			= ini.ReadString	("Patches",				"szAttractModeDelay",		"Game");

	// Patterns
	pPakTocCheck				= ini.ReadString	("Patterns",			"pPakTocCheck",				"");
	pUNameObjGetPat				= ini.ReadString	("Patterns",			"pUNameObjGetPat",			"");
	pFPathLoadPat				= ini.ReadString	("Patterns",			"pFPathLoadPat",			"");
	pFPath2LoadPat				= ini.ReadString	("Patterns",			"pFPath2LoadPat",			"");
	pFPathCLoadPat				= ini.ReadString	("Patterns",			"pFPathCLoadPat",			"");
	pEndpointLoader				= ini.ReadString	("Patterns",			"pEndpointLoader",			"");
	pProfileGetter				= ini.ReadString	("Patterns",			"pProfileGetter",			"");
	pOnlineSubsystem			= ini.ReadString	("Patterns",			"pOnlineSubsystem",			"");
	pOnlineCheck				= ini.ReadString	("Patterns",			"pOnlineCheck",				"");
	pAuthToken					= ini.ReadString	("Patterns",			"pAuthToken",				"");
	// Curl
	pCurlSetOpt					= ini.ReadString	("Patterns.Curl",		"pCurlSetOpt",				"");
	pCurlMultiAddHandle			= ini.ReadString	("Patterns.Curl",		"pCurlMultiAddHandle",		"");
	pCurlMultiInfoRead			= ini.ReadString	("Patterns.Curl",		"pCurlMultiInfoRead",		"");
	// Version
	pNRSClientVersion			= ini.ReadString	("Patterns.Version",	"pNRSClientVersion",		"");
	pBuildVersion				= ini.ReadString	("Patterns.Version",	"pBuildVersion",			"");
	pBuildDate					= ini.ReadString	("Patterns.Version",	"pBuildDate",				"");
	pChangelist					= ini.ReadString	("Patterns.Version",	"pChangelist",				"");
	// Floyd
	pSecretFightCondPat			= ini.ReadString	("Patterns.Floyd",		"pSecretFightCondPat",		"");
	pGetChallengesFromHash		= ini.ReadString	("Patterns.Floyd",		"pGetChallengesFromHash",	"");
	pGetFloydHashInputString	= ini.ReadString	("Patterns.Floyd",		"pGetFloydHashInputString",	"");
	pGetFloydHashInputString2	= ini.ReadString	("Patterns.Floyd",		"pGetFloydHashInputString2","");
	pGetTacoHashInputString		= ini.ReadString	("Patterns.Floyd",		"pGetTacoHashInputString",	"");
	pGetTacoSchedule			= ini.ReadString	("Patterns.Floyd",		"pGetTacoSchedule",			"");
	pTowersSetup				= ini.ReadString	("Patterns.Floyd",		"pTowersSetup",				"");
	pGetFloydVictories			= ini.ReadString	("Patterns.Floyd",		"pGetFloydVictories",		"");
	pAttractModeIdle			= ini.ReadString	("Patterns.MainMenu",	"pAttractModeIdle",			"");
	pAttractModeLimit			= ini.ReadString	("Patterns.MainMenu",	"pAttractModeLimit",			"");
	pFindScript					= ini.ReadString	("Patterns.MainMenu",	"pFindScript",				NewPatterns::FindScript);
	pKeepRunning				= ini.ReadString	("Patterns.MainMenu",	"pKeepRunning",				"");
	pFocusLost					= ini.ReadString	("Patterns.MainMenu",	"pFocusLost",				"");
	pFocusLostPause				= ini.ReadString	("Patterns.MainMenu",	"pFocusLostPause",			"");
	pBootWaitFirst				= ini.ReadString	("Patterns.MainMenu",	"pBootWaitFirst",			"");
	pBootWaitSecond				= ini.ReadString	("Patterns.MainMenu",	"pBootWaitSecond",			"");
	pBootWaitThird				= ini.ReadString	("Patterns.MainMenu",	"pBootWaitThird",			"");
	pBootStudioLogos			= ini.ReadString	("Patterns.MainMenu",	"pBootStudioLogos",			"");
	pBootMovieSkip				= ini.ReadString	("Patterns.MainMenu",	"pBootMovieSkip",			"");
	pGetStatsContainer			= ini.ReadString	("Patterns.Floyd",		"pGetStatsContainer",		"");
	pGetProfileStat				= ini.ReadString	("Patterns.Floyd",		"pGetProfileStat",			"");
	pGetProfileStatBits			= ini.ReadString	("Patterns.Floyd",		"pGetProfileStatBits",		"");
	pFloydSeed					= ini.ReadString	("Patterns.Floyd",		"pFloydSeed",				"");
	pFloydLevel					= ini.ReadString	("Patterns.Floyd",		"pFloydLevel",				"");
	pFloydLevelSession			= ini.ReadString	("Patterns.Floyd",		"pFloydLevelSession",		"");
	pFloydRetries				= ini.ReadString	("Patterns.Floyd",		"pFloydRetries",			"");
	pAnnouncerReroll			= ini.ReadString	("Patterns.Announcer",	"pAnnouncerReroll",			"");
	pAnnouncerMenuApply			= ini.ReadString	("Patterns.Announcer",	"pAnnouncerMenuApply",		"");
	pApplyAnnouncer				= ini.ReadString	("Patterns.Announcer",	"pApplyAnnouncer",			"");
	pEngineNameConstruct		= ini.ReadString	("Patterns.Announcer",	"pEngineNameConstruct",		"");
	pGetCurrentMission			= ini.ReadString	("Patterns.Announcer",	"pGetCurrentMission",		"");
	pGetTeam					= ini.ReadString	("Patterns.Announcer",	"pGetTeam",					"");
	pGetTeamDefinitions			= ini.ReadString	("Patterns.Announcer",	"pGetTeamDefinitions",		"");
	pFightStartupCall			= ini.ReadString	("Patterns.Announcer",	"pFightStartupCall",		NewPatterns::FightStartupCall);
	pFightStartupCallCpu		= ini.ReadString	("Patterns.Announcer",	"pFightStartupCallCpu",		NewPatterns::FightStartupCallCpu);
	// Message Box
	pTextFromString				= ini.ReadString	("Patterns.MessageBox",	"pTextFromString",			"");
	pFindText					= ini.ReadString	("Patterns.MessageBox",	"pFindText",				"");
	pMessageBoxConstruct		= ini.ReadString	("Patterns.MessageBox",	"pMessageBoxConstruct",		"");
	pMessageBoxSetTitle			= ini.ReadString	("Patterns.MessageBox",	"pMessageBoxSetTitle",		"");
	pMessageBoxAddButton		= ini.ReadString	("Patterns.MessageBox",	"pMessageBoxAddButton",		"");
	pMessageBoxShow				= ini.ReadString	("Patterns.MessageBox",	"pMessageBoxShow",			"");
	pMessageBoxClose			= ini.ReadString	("Patterns.MessageBox",	"pMessageBoxClose",			"");
	pMessageBoxDestruct			= ini.ReadString	("Patterns.MessageBox",	"pMessageBoxDestruct",		"");
	pPlayerInputPressed			= ini.ReadString	("Patterns.MessageBox",	"pPlayerInputPressed",		"");
	pPlayerInputUpdate			= ini.ReadString	("Patterns.MessageBox",	"pPlayerInputUpdate",		"");
	pToastDriverCall			= ini.ReadString	("Patterns.MessageBox",	"pToastDriverCall",			"");
	pToastEnqueue				= ini.ReadString	("Patterns.MessageBox",	"pToastEnqueue",			"");
	pRealloc					= ini.ReadString	("Patterns.MessageBox",	"pRealloc",					"");
	pLocalPlayerInfo			= ini.ReadString	("Patterns.MessageBox",	"pLocalPlayerInfo",			"");
	pMessageBoxCallback			= ini.ReadString	("Patterns.MessageBox",	"pMessageBoxCallback",		"");
	// Game State
	pMainLoop					= ini.ReadString	("Patterns.GameState",	"pMainLoop",				"");
	pFrameTick					= ini.ReadString	("Patterns.GameState",	"pFrameTick",				"");
	pOnlineModeCheck			= ini.ReadString	("Patterns.GameState",	"pOnlineModeCheck",			"");
	pModeManagerGetter			= ini.ReadString	("Patterns.GameState",	"pModeManagerGetter",		"");
	// Settings Screen
	pSettingsBuildTabs			= ini.ReadString	("Patterns.SettingsScreen",	"pSettingsBuildTabs",		"");
	pSettingsCopyTabs			= ini.ReadString	("Patterns.SettingsScreen",	"pSettingsCopyTabs",		"");
	pMenuEntryConstruct			= ini.ReadString	("Patterns.SettingsScreen",	"pMenuEntryConstruct",		"");
	pSettingsTabConstruct		= ini.ReadString	("Patterns.SettingsScreen",	"pSettingsTabConstruct",	"");
	pSettingsTabDestruct		= ini.ReadString	("Patterns.SettingsScreen",	"pSettingsTabDestruct",		"");
	pGrowArray					= ini.ReadString	("Patterns.SettingsScreen",	"pGrowArray",				"");
	pStringReserve				= ini.ReadString	("Patterns.SettingsScreen",	"pStringReserve",			"");
	pFindTextPlatform			= ini.ReadString	("Patterns.SettingsScreen",	"pFindTextPlatform",		"");
	pMemoryFree					= ini.ReadString	("Patterns.SettingsScreen",	"pMemoryFree",				"");
	pMainMenuCopyTabs			= ini.ReadString	("Patterns.SettingsScreen",	"pMainMenuCopyTabs",		"");
	pSettingsVersionText		= ini.ReadString	("Patterns.SettingsScreen",	"pSettingsVersionText",	"");
	pSettingsRun				= ini.ReadString	("Patterns.SettingsScreen",	"pSettingsRun",				"");
	pSettingsPageSwitch			= ini.ReadString	("Patterns.SettingsScreen",	"pSettingsPageSwitch",			"");
	pFooterBuild				= ini.ReadString	("Patterns.SettingsScreen",	"pFooterBuild",				"");
	pPromptText					= ini.ReadString	("Patterns.SettingsScreen",	"pPromptText",				"");
	pPauseCollectEntries		= ini.ReadString	("Patterns.SettingsScreen",	"pPauseCollectEntries",		"");
	pPausePanelPageSetup		= ini.ReadString	("Patterns.SettingsScreen",	"pPausePanelPageSetup",		"");
	pGrowPauseEntries			= ini.ReadString	("Patterns.SettingsScreen",	"pGrowPauseEntries",			"");
	pPanelHolder				= ini.ReadString	("Patterns.SettingsScreen",	"pPanelHolder",				"");
	pTransientPackageCall		= ini.ReadString	("Patterns.SettingsScreen",	"pTransientPackageCall",	NewPatterns::TransientPackageCall);
	pPauseSelectedIndex			= ini.ReadString	("Patterns.SettingsScreen",	"pPauseSelectedIndex",		"");
	pPauseClose					= ini.ReadString	("Patterns.SettingsScreen",	"pPauseClose",				"");
	// Paks
	pPakMount					= ini.ReadString	("Patterns.Paks",		"pPakMount",				"");
	pConfigInit					= ini.ReadString	("Patterns.Paks",		"pConfigInit",				"");
	pConfigGlobals				= ini.ReadString	("Patterns.Paks",		"pConfigGlobals",			"");
	pConfigSetString			= ini.ReadString	("Patterns.Paks",		"pConfigSetString",			"");
	pConfigGetString			= ini.ReadString	("Patterns.Paks",		"pConfigGetString",			"");
	pConfigFindFile				= ini.ReadString	("Patterns.Paks",		"pConfigFindFile",			"");
	pLoadFileToString			= ini.ReadString	("Patterns.Paks",		"pLoadFileToString",		"");
	// Engine
	pStaticFindObject			= ini.ReadString	("Patterns.Engine",		"pStaticFindObject",		NewPatterns::StaticFindObject);
	pStaticLoadObject			= ini.ReadString	("Patterns.Engine",		"pStaticLoadObject",		NewPatterns::StaticLoadObject);
	pStaticConstructObject		= ini.ReadString	("Patterns.Engine",		"pStaticConstructObject",	NewPatterns::StaticConstructObject);
	pObjectArray				= ini.ReadString	("Patterns.Engine",		"pObjectArray",				NewPatterns::ObjectArray);


	// Keybinds
	hkMenu						= ini.ReadString	("Keybinds",			"hkMenu",					"F1");
	hkInfo						= ini.ReadString	("Keybinds",			"hkInfo",					"TAB");
	hkCheats					= ini.ReadString	("Keybinds",			"hkCheats",					"F12");

	// Private Server
	szServerUrl					= ini.ReadString	("Server",				"szServerUrl",				"");

}
