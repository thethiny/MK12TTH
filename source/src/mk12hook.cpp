#include "mk12hook.h"
#include "AGBinary.h"
#include "GamePatchManager.h"
#include "FloydTower.h"
#include "GameState.h"
#include "GameMessageBox.h"
#include "GameToast.h"
#include "ModSettings.h"
#include "ModMenu.h"
#include "PauseMenu.h"
#include "FloydFight.h"
#include "PakMods.h"
#include "Features.h"
#include "Announcer.h"
#include "EngineConfig.h"
#include "FloydTowerAlert.h"
#include "OnlineIds.h"
#include "AttractMode.h"


HookMetadata::ServerProxyMode		HookMetadata::eServerProxyMode = HookMetadata::PROXY_NONE;
HookMetadata::ActiveMods			HookMetadata::ActiveModsMap;
HookMetadata::CheatsStruct			HookMetadata::sCheatsStruct;
HookMetadata::LibMapsStruct			HookMetadata::sLFS;
HookMetadata::UserKeysStruct		HookMetadata::sUserKeys;
HookMetadata::SwapTable				HookMetadata::FSwapTable;
// Thread safety note: FSwapTable is only written at startup (AnnouncerSwap/StringSwaps) and read-only after.
// FloydCluesInfo, TacoTowerInfo and UserProfileInfo are written by NRS-to-UE proxies which are single-threaded within NRS's caller.
// If additional hooks are added that write to these from different call sites, add synchronization.
HookMetadata::FloydCluesInfo		HookMetadata::CurrentFloydInfo;
HookMetadata::TacoTowerInfo			HookMetadata::CurrentTacoInfo;
HookMetadata::ProfileInfo			HookMetadata::UserProfileInfo;

bool HookMetadata::HasResponseCapture() { auto It = ActiveModsMap.find("bCurlResponseCapture"); return It != ActiveModsMap.end() && It->second; }

namespace CurlOpt {
	constexpr __int64 URL = 10002;
	constexpr __int64 HEADERDATA = 10029;
	constexpr __int64 PRIVATE = 10103;
}

namespace MK12Hook::Proxies {

	wchar_t** __fastcall OverrideGameEndpoint(MK12::JSONEndpointValue obj, wchar_t** EndpointAddress)
	{
		static wchar_t* cachedServerUrl = nullptr;
		static int cachedServerLen = 0;

		if (Features::IsInstalled(Features::Id::PrivateServer) && !SettingsMgr->szServerUrl.empty())
		{
			if (!cachedServerUrl)
			{
				CPPython::string ServerUrl = SettingsMgr->szServerUrl;
				ServerUrl = ServerUrl.strip("/");

#pragma warning(push)
#pragma warning(disable: 4267)
				int StringLength = (ServerUrl.size() + 1) * 2;
				std::wstring wServerUrl = std::wstring(ServerUrl.begin(), ServerUrl.end());

				cachedServerUrl = new wchar_t[StringLength];
				memcpy(cachedServerUrl, wServerUrl.c_str(), StringLength);
				cachedServerLen = ServerUrl.size() + 1;
#pragma warning(pop)

				wprintf(L"Replacing endpoint with \"%s\"!\n", wServerUrl.c_str());
			}

			obj.ValuePointer = cachedServerUrl;
			obj.ValueLength = cachedServerLen;
			obj.ValueLength8BAligned = (obj.ValueLength + 7) & (~7);

			MK12::GetEndpointKeyValue(obj, EndpointAddress);
			return &obj.ValuePointer;
		}

		return MK12::GetEndpointKeyValue(obj, EndpointAddress);

	}

	MK12::TArray<uint32_t>* __fastcall GenerateFloydCluesFromHashProxy(MK12::TArray<uint32_t>* ArrayLocation, uint64_t ShuffleSeed, uint64_t unk, uint64_t unk2)
	{ // Use hash to create 10 challenges
		if (!Features::IsActive(Features::Id::FloydTracking))
			return MK12::GenerateFloydCluesFromHash(ArrayLocation, ShuffleSeed, unk, unk2);

		if (SettingsMgr->ShouldLogFloyd())
			printf("Floyd Shuffle Seed: %llX\n", (uint64_t)ShuffleSeed);
		MK12::TArray<uint32_t>* ReturnedArrayLocation = MK12::GenerateFloydCluesFromHash(ArrayLocation, ShuffleSeed, unk, unk2); // 8 bytes location and 4 bytes count

		if (ReturnedArrayLocation && ReturnedArrayLocation->Data) {

			// Store the data
			uint32_t newUserProfileHash = (uint32_t)(ShuffleSeed >> 32);
			uint32_t newFloydEncounters = (uint32_t)(ShuffleSeed & 0xFFFFFFFF);

			// The game's latest list is the one in use: it regenerates them in the Floyd fight, and keeps an offline and an online list
			{
				HookMetadata::CurrentFloydInfo.HasClues = true;
				HookMetadata::CurrentFloydInfo.Clues.clear();

				for (uint8_t i = 0; i < ReturnedArrayLocation->ElementsCount; i++)
				{
					HookMetadata::CurrentFloydInfo.Clues.push_back((uint8_t)ReturnedArrayLocation->Data[i] + 1);
				}

				HookMetadata::CurrentFloydInfo.UserProfileHash = newUserProfileHash;
				HookMetadata::CurrentFloydInfo.FloydEncounters = newFloydEncounters;
			}

			printf("Floyd Challenges: ");
			for (int32_t i = 0; i < ReturnedArrayLocation->ElementsCount; ++i) {
				printf("%i ", (uint32_t)ReturnedArrayLocation->Data[i]+1);
			}
			printf("\n");
		}
		else {
			printfError("Couldn't Read Floyd Challenges!\n");
		}

		return ReturnedArrayLocation;
	}

	uint32_t __fastcall CustomCityHashProxy(wchar_t** StringToHash)
	{ // Hash the floyd string
		if (!Features::IsActive(Features::Id::FloydTracking))
			return MK12::CustomCityHash(StringToHash);

		if (StringToHash && *StringToHash) {
			if (SettingsMgr->ShouldLogFloyd())
				printf("StringToHash: %ls\n", *StringToHash); // %ls for wchar_t*

			OnlineIds::Store(*StringToHash); // The same text the game hashes
		}
		else
		{
			printfError("StringToHash Is Empty!\n");
		}
		
		uint32_t HashResult = MK12::CustomCityHash(StringToHash);
		if (SettingsMgr->ShouldLogFloyd())
			printf("HashResult: %X\n", (uint32_t)HashResult);

		return HashResult;
	}

	uint32_t __fastcall TacoCityHashProxy(wchar_t** StringToHash)
	{ // Hash the taco tower schedule string. Runs for today and yesterday on every schedule rebuild, so only print on change
		uint32_t HashResult = MK12::CustomCityHash(StringToHash);
		if (!Features::IsActive(Features::Id::FloydTracking))
			return HashResult;

		if (HashResult != HookMetadata::CurrentTacoInfo.UserProfileHash)
		{
			HookMetadata::CurrentTacoInfo.UserProfileHash = HashResult;

			if (SettingsMgr->ShouldLogFloyd())
			{
				if (StringToHash && *StringToHash)
					printf("Taco StringToHash: %ls\n", *StringToHash);
				else
					printf("Taco StringToHash: <empty>\n");
				printf("Taco HashResult: %X\n", HashResult);
			}

			if (!SettingsMgr->bFloydTowerFromGame)
				FloydTower::LogSchedule(HashResult);
		}

		return HashResult;
	}


	__int64 __fastcall TacoScheduleProxy(MK12::UTacoTowerSchedule* Schedule, int64_t NowTicks)
	{ // Game's own Floyd Tower windows, read right after it rebuilds them. Runs often, so only log on change
		__int64 Result = MK12::UpdateTacoSchedule(Schedule, NowTicks);
		if (!Schedule || !Features::IsActive(Features::Id::FloydTracking))
			return Result;

		// FLOYD TOWER NOW: today's slot, as the game built it, moved to start now
		MK12::FTacoTowerEntry* Today = nullptr;
		for (int32_t i = 0; i < Schedule->Entries.ElementsCount; i++)
		{
			MK12::FTacoTowerEntry& Entry = Schedule->Entries.Data[i];
			if (!Today || Entry.StartTicks > Today->StartTicks)
				Today = &Entry;
		}
		int64_t Start = 0, End = 0;
		if (Today && Features::IsInstalled(Features::Id::FloydFight) && FloydFight::MoveTowerSlot(NowTicks, Today->StartTicks, Today->EndTicks, Start, End))
		{
			Today->StartTicks = Start;
			Today->EndTicks = End;
		}

		std::vector<FloydTower::Window> Windows;
		for (int32_t i = 0; i < Schedule->Entries.ElementsCount; i++)
		{
			MK12::FTacoTowerEntry& Entry = Schedule->Entries.Data[i];
			FloydTower::Window window;
			window.Start = FloydTower::TicksToUnix(Entry.StartTicks);
			window.End = FloydTower::TicksToUnix(Entry.EndTicks);
			window.Slot = FloydTower::SlotFromSplineName(Entry.SplineName.c_str());
			Windows.push_back(window);
		}

		if (FloydTower::SetGameWindows(Windows))
		{
			if (SettingsMgr->ShouldLogFloyd())
			{
				printf("Taco Schedule: %d entries\n", Schedule->Entries.ElementsCount);
				for (int32_t i = 0; i < Schedule->Entries.ElementsCount; i++)
				{
					MK12::FTacoTowerEntry& Entry = Schedule->Entries.Data[i];
					printf("  %ls @ %ls: %lld -> %lld\n", Entry.LadderName.c_str(), Entry.SplineName.c_str(), Entry.StartTicks, Entry.EndTicks);
				}
			}
			FloydTower::LogGameSchedule();
		}

		return Result;
	}

	bool __fastcall PlayerInputPressedProxy(void* Player, int32_t Input, MK12::FString* Id, uint32_t Flags)
	{ // The game pauses the screen behind its own message boxes, ours don't, so the screen would take their presses
		if (!GameMessageBox::IsTakingInput())
			return MK12::PlayerInputPressed(Player, Input, Id, Flags);
		if (!GameMessageBox::IsOwnButton(Id))
			return false;
		MK12::PlayerInputPressed(Player, Input, Id, Flags); // Keeps the button prompt, the press itself was already taken
		return GameMessageBox::TakePress(Input);
	}

	void __fastcall PlayerInputUpdateProxy(MK12::FPlayerInput* Input, void* Sample)
	{ // Right after the game samples a player's buttons, before any screen or box reads them
		MK12::PlayerInputUpdate(Input, Sample);
		if (!GameMessageBox::IsTakingInput())
			ModSettings::OnInputUpdate(Input);
		GameMessageBox::OnInputUpdate(Input);
	}

	__int64 __fastcall FrameTickProxy()
	{ // Once per frame on the game thread, in menus, fights and loading
		__int64 Result = MK12::FrameTick();
		GameState::Tick();
		GameMessageBox::Tick();
		GameToast::Tick();
		FloydTowerAlert::Tick();
		OnlineIds::Tick();
		ModSettings::Tick();
		PauseMenu::Tick();
		return Result;
	}

	__int64 __fastcall SettingsBuildTabsProxy(MK12::USettingsScreen* Screen, bool bOnline)
	{ // The main menu settings just added their pages, ours goes last so its tab lines up
		ModSettings::OnPagesBuilt(Screen);
		return MK12::SettingsBuildTabs(Screen, bOnline);
	}

	void __fastcall SettingsCopyTabsProxy(void* Out, MK12::FSettingsTab* Tabs, int32_t Count, int32_t ExtraA, int32_t ExtraB)
	{ // Only the settings tab bar comes through here
		ModSettings::CopyTabs(Out, Tabs, Count, ExtraA, ExtraB);
	}

	bool __fastcall AttractModeIdleProxy(void* ModeManager, void* ModeClass)
	{ // Asked every frame by the main menu's idle timer, false restarts it
		if (AttractMode::ShouldRestartTimer())
			return false;
		return MK12::IsModeOnTop(ModeManager, ModeClass);
	}

	bool __fastcall BootScreenWaitProxy(int32_t FirstBootFrames, int32_t Frames, int32_t Counter)
	{ // The logo and legal screens ask it whether they waited long enough (or Skip was pressed): always yes
		static int32_t LastFrames = -1;
		if (Frames != LastFrames)
		{
			LastFrames = Frames;
			if (SettingsMgr->ShouldLog(Log::Debug))
				printf("Fast Boot: A boot screen goes on right away instead of waiting %d frames\n", FirstBootFrames > Frames ? FirstBootFrames : Frames);
		}
		return true;
	}

	bool __fastcall StudioLogosProxy(const wchar_t* Name, int32_t A, int32_t B, int32_t C, int32_t D, void* Out)
	{ // The studio logos movie at boot: "didn't start", the boot screens go on as the game does when it can't play
		if (SettingsMgr->ShouldLog(Log::Debug))
			printf("Fast Boot: Skipped the studio logos\n");
		return false;
	}

	bool __fastcall SkipBootMovieProxy(const wchar_t* CommandLine, const wchar_t* Param)
	{ // The intro movie asks for -skipbootmovie: yes, the game's own skip
		if (SettingsMgr->ShouldLog(Log::Debug))
			printf("Fast Boot: Skipped the intro movie\n");
		return true;
	}

	void __fastcall ConfigInitProxy()
	{ // The engine just loaded its ini files, nothing read them yet
		MK12::InitializeConfigSystem();
		EngineConfig::Inject();
	}

	uint8_t* __fastcall FloydSetLevelProxy(uint8_t* AI, uint8_t Level)
	{ // Floyd's fight being built (a retry too): the level the game picked for this attempt, or ours
		uint8_t Ours = 0;
		if (!FloydFight::OverrideLevel(Level, Ours))
			return MK12::FloydSetLevel(AI, Level);
		return MK12::FloydSetLevel(AI, Ours);
	}

	void __fastcall PauseCloseProxy(void* Pause)
	{ // The pause menu closing, MOD SETTINGS lets go of it first
		PauseMenu::OnPauseClosed(Pause);
		MK12::PauseClose(Pause);
	}

	__int64 __fastcall AnnouncerRerollProxy(void* AudioSettings)
	{ // Ladder fights re-roll a random announcer as they're set up
		if (Announcer::OnLadderReroll())
			return 0;
		return MK12::AnnouncerReroll(AudioSettings);
	}

	__int64 __fastcall AnnouncerMenuApplyProxy(void* AudioSettings, MK12::FString* ItemId)
	{ // The main menu applies the player's own announcer setting as it loads
		__int64 Result = MK12::AnnouncerSetter(AudioSettings, ItemId);
		Announcer::OnMenuApplied(AudioSettings);
		return Result;
	}

	char __fastcall TowersSetupProxy(void* TacoSchedule)
	{ // Living Towers loaded, the towers and their schedule are set up by the game
		char Result = MK12::TowersSetup(TacoSchedule);
		FloydTowerAlert::OnTowersEntered(TacoSchedule);
		return Result;
	}

	void __fastcall SettingsRunProxy(void* Screen, void* Player, int64_t Mode)
	{ // Every settings screen, from opening until the player backs out
		ModSettings::OnScreenOpened();
		MK12::SettingsRun(Screen, Player, Mode);
		ModSettings::OnScreenClosed();
	}

	void __fastcall PauseCollectEntriesProxy(void* Source, void* Player, MK12::TArray<MK12::FPauseEntry>* Out)
	{ // The pause menu listing its entries, ours goes in after KONTROLS
		MK12::PauseCollectEntries(Source, Player, Out);
		PauseMenu::OnEntriesCollected(Out);
	}

	void __fastcall PausePanelPageSetupProxy(MK12::USettingsScreen* Screen, void* Player, int64_t Mode)
	{ // The pause menu's KONTROLS panel filling its pages, MOD SETTINGS uses the same panel
		MK12::PausePanelPageSetup(Screen, Player, Mode);
		PauseMenu::OnPanelSetUp(Screen, Player, Mode);
	}

	__int64 __fastcall FooterBuildProxy(MK12::USettingsScreen* Screen, MK12::TArray<MK12::FText>* Labels, void* FocusedRow, bool bNarrate, MK12::TArray<uint8_t>* Bits)
	{ // The settings screen's footer prompts, COPY joins them on our rows with a value
		__int64 Result = MK12::FooterBuild(Screen, Labels, FocusedRow, bNarrate, Bits);
		if (!bNarrate)
			ModSettings::OnFooterBuilt(Screen, Labels, FocusedRow, Bits);
		return Result;
	}

	MK12::FString* __fastcall SettingsVersionTextProxy()
	{ // The settings screen's bottom left version, ours in our menu
		return ModSettings::VersionText(MK12::SettingsVersionText());
	}

	void __fastcall MainMenuCopyTabsProxy(void* Out, MK12::FSettingsTab* Tabs, int32_t Count, int32_t ExtraA, int32_t ExtraB)
	{ // Only the main menu's entry list comes through here
		ModMenu::CopyMainMenuTabs(Out, Tabs, Count, ExtraA, ExtraB);
	}

	__int64 __fastcall PakMountProxy(void* PakPlatformFile, const wchar_t* PakFilename, uint32_t PakOrder, const wchar_t* MountPath, bool bLoadIndex)
	{ // Every pak and IoStore container the game mounts, except global. A mod turned off isn't mounted
		PakMods::MountOurs(PakPlatformFile, MK12::PakMount);
		if (PakMods::IsDisabled(PakFilename))
			return false;
		return MK12::PakMount(PakPlatformFile, PakFilename, PakOrder, MountPath, bLoadIndex);
	}

	MK12::FText* __fastcall FindTextProxy(MK12::FText* Out, const wchar_t* Table, const wchar_t* Key)
	{ // Text of our settings rows, everything else is the game's
		if (ModSettings::FindText(Out, Table, Key))
			return Out;
		return MK12::FindText(Out, Table, Key);
	}

	MK12::FText* __fastcall FindTextPlatformProxy(MK12::FText* Out, const wchar_t* Table, const wchar_t* Key)
	{ // Descriptions and the page description come through here
		if (ModSettings::FindText(Out, Table, Key))
			return Out;
		return MK12::FindTextPlatform(Out, Table, Key);
	}

	// Stand in for FCoreDelegates::GetPakSigningKeysDelegate(). Every caller checks the delegate's
	// allocation and size before using it, so all zeros reads as unbound: no .sig loading, no signed
	// pak readers, no chunk CRC checks and no IoStore TOC signatures, same as a build without keys
	uint64_t* __fastcall GetUnboundPakSigningKeysDelegate()
	{
		static uint64_t UnboundDelegate[4] = {};
		return UnboundDelegate;
	}

	// Hooks a specific NRS call site that always passes exactly 3 wchar_t* args (Platform, PlatformId, SaveKey)
	wchar_t* MKWScanfProxyForCrypto(wchar_t* ResultString, const wchar_t* Format, ...)
	{
		va_list args;
		va_start(args, Format);

		wchar_t* arg0 = va_arg(args, wchar_t*);
		wchar_t* arg1 = va_arg(args, wchar_t*);
		wchar_t* arg2 = va_arg(args, wchar_t*);

		va_end(args);

		if (!Features::IsActive(Features::Id::ProfileGetter))
			return MK12::MKWScanf(ResultString, Format, arg0, arg1, arg2);

		auto SafeStringCopy = [](const wchar_t* src) -> wchar_t* {
			if (!src) return nullptr;
			size_t len = wcslen(src) + 1;
			wchar_t* copy = new wchar_t[len];
			wcscpy(copy, src);
			return copy;
			};

		auto SafeWcsCmp = [](const wchar_t* a, const wchar_t* b) -> bool {
			if (!a && !b) return true;
			if (!a || !b) return false;
			return wcscmp(a, b) == 0;
			};

		if (!SafeWcsCmp(HookMetadata::UserProfileInfo.Platform, arg0) ||
			!SafeWcsCmp(HookMetadata::UserProfileInfo.PlatformId, arg1) ||
			!SafeWcsCmp(HookMetadata::UserProfileInfo.SaveKey, arg2))
		{
			delete[] HookMetadata::UserProfileInfo.Platform;
			delete[] HookMetadata::UserProfileInfo.PlatformId;
			delete[] HookMetadata::UserProfileInfo.SaveKey;

			HookMetadata::UserProfileInfo.Platform = SafeStringCopy(arg0);
			HookMetadata::UserProfileInfo.PlatformId = SafeStringCopy(arg1);
			HookMetadata::UserProfileInfo.SaveKey = SafeStringCopy(arg2);

			if (SettingsMgr->ShouldLogProfile())
				printf("Stored Profile Info:\nPlatform: %ws\nPlatformId: %ws\nSaveKey: %ws\n", arg0, arg1, arg2);
		}

		// Now call real function with full argsCopy
		return MK12::MKWScanf(ResultString, Format, arg0, arg1, arg2);
	}



	MK12::FKlassicLadderSecretFightData* __fastcall SetupSecretFightConditionsProxy(MK12::FKlassicLadderSecretFightData* mSecretFightData)
	{
 		MK12::UKlassicTowerSecretFightData* mKlassicTowerSecretFightInstance = (MK12::UKlassicTowerSecretFightData*)(((uint64_t)(mSecretFightData)) - MK12::KlassicTowerSecretFightDataOffset);
		if (SettingsMgr->ShouldLog(Log::Debug))
		{
			printf("SecretFightData @ %p\n", mSecretFightData);
			printf("KlassicTowerSecretFightData @ %p\n", mKlassicTowerSecretFightInstance);
		}

		MK12::FKlassicLadderSecretFightData* mSecretFightDataRet = MK12::SetupSecretFightConditions(mSecretFightData);

		if (!Features::IsActive(Features::Id::FightMetadataLog))
			return mSecretFightDataRet;

		if (mKlassicTowerSecretFightInstance)
		{
			MK12::FMissionBuildStep_KlassicTowerSecretFight& secretFightRules = mKlassicTowerSecretFightInstance->mSecretFightRulesBuildStep;

			printf("Floyd mDifficultyMaps: ");
			for (int i = 0; i < secretFightRules.mDifficultyMap.ElementsCount; i++) {
				printf("%02X ", secretFightRules.mDifficultyMap.Data[i]);
			}
			printf("\n");
		}


		char FightingStatCondition[]		= "LadderSecretFightCondition_FightingStatCondition";
		char MultiFightingStatCondition[]	= "LadderSecretFightCondition_MultiFightingStatCondition";
		char MoveTraceCondition[]			= "LadderSecretFightCondition_MoveTraceConditions";
		char ProfileStatCondition[]			= "LadderSecretFightCondition_ProfileStatCondition";

		// Start deserialization
		for (int i = 0; i < mSecretFightData->mConditions.ElementsCount; i++)
		{
			MK12::FKlassicLadderSecretFightCondition* CurrentCondition = MK12::TArrayHelpers::index<MK12::FKlassicLadderSecretFightCondition>(mSecretFightData->mConditions, i);
			if (!CurrentCondition || !CurrentCondition->StructPtrType || !CurrentCondition->StructPtrInstance)
				continue;
			MK12::FName* ScriptType = MK12::NameTableIndexToFName(&CurrentCondition->StructPtrType->ScriptClass);
			if (!ScriptType)
				continue;
			printf("Floyd Challenge %d - %s\n\tType %s\n", CurrentCondition->Index, MK12::EnumMaps::Find(MK12::EnumMaps::FloydChallengeMap, CurrentCondition->Index), FNAME_STR(ScriptType));
			if (!CurrentCondition->enable)
			{
				printf("Disabled!");
				continue;
			}

			if (strncmp(ScriptType->Name, FightingStatCondition, ScriptType->GetSize()) == 0)
			{
				MK12::FLadderSecretFightCondition_FightingStatCondition* RetypedCondition = static_cast<MK12::FLadderSecretFightCondition_FightingStatCondition*>(CurrentCondition->StructPtrInstance);
				MK12::Remake::SecretFight::PrintTypedConditions(RetypedCondition);
			}
			else if (strncmp(ScriptType->Name, MultiFightingStatCondition, ScriptType->GetSize()) == 0)
			{
				MK12::FLadderSecretFightCondition_MultiFightingStatCondition* RetypedCondition = static_cast<MK12::FLadderSecretFightCondition_MultiFightingStatCondition*>(CurrentCondition->StructPtrInstance);
				MK12::Remake::SecretFight::PrintTypedConditions(RetypedCondition);
			}
			else if (strncmp(ScriptType->Name, MoveTraceCondition, ScriptType->GetSize()) == 0)
			{
				MK12::FLadderSecretFightCondition_MoveTraceConditions* RetypedCondition = static_cast<MK12::FLadderSecretFightCondition_MoveTraceConditions*>(CurrentCondition->StructPtrInstance);
				MK12::Remake::SecretFight::PrintTypedConditions(RetypedCondition);
			}
			else if (strncmp(ScriptType->Name, ProfileStatCondition, ScriptType->GetSize()) == 0)
			{
				MK12::FLadderSecretFightCondition_ProfileStatCondition* RetypedCondition = static_cast<MK12::FLadderSecretFightCondition_ProfileStatCondition*>(CurrentCondition->StructPtrInstance);
				MK12::Remake::SecretFight::PrintTypedConditions(RetypedCondition);
			}

			if (!CurrentCondition->CanCheckDuringFight)
			{
				printf("* Notification pops up after the match!\n");
			}
			printf("\n");
		}


		return mSecretFightDataRet;
	}

	static std::unordered_map<__int64, std::string> CurlHandleUrls;
	static std::unordered_map<__int64, MK12::CURL::HTTPRequestWrapper*> CurlHandleStructs;

	// What the capture has seen on the wire, only for the log: the keys the mod shows come from the game itself (OnlineIds)
	static bool bCurlSawRequest = false;
	static bool bCurlSawResponse = false;

	static void CurlCleanupIfDone()
	{
		if (bCurlSawRequest && (!HookMetadata::HasResponseCapture() || bCurlSawResponse))
		{
			CurlHandleUrls.clear();
			CurlHandleStructs.clear();
		}
	}

	__int64 __fastcall CurlSetOptProxy(__int64 handle, __int64 option, __int64 value)
	{
		if (!Features::IsActive(Features::Id::Curl))
			return MK12::CurlSetOpt(handle, option, value);

		if (option == CurlOpt::URL && value)
		{
			CurlHandleUrls[handle] = (const char*)value;
			if (SettingsMgr->ShouldFlood(SettingsMgr->Floods.bCurl))
				printf("[CURL] URL: %s\n", (const char*)value);

		}
		else
		{
			if ((option == CurlOpt::HEADERDATA || option == CurlOpt::PRIVATE) && value)
				CurlHandleStructs[handle] = (MK12::CURL::HTTPRequestWrapper*)value;

			if (SettingsMgr->ShouldFlood(SettingsMgr->Floods.bCurl))
				printf("[CURL] setopt handle=%p option=%lld value=%p\n", (void*)handle, option, (void*)value);
		}

		return MK12::CurlSetOpt(handle, option, value);
	}

	static void ParseAccessRequest(MK12::CURL::HTTPRequestWrapper* wrapper)
	{
		char* data = nullptr;
		int32_t size = 0;
		if (!wrapper->GetRequestBody(&data, &size)) return;

		int32_t cursor = 0;
		if (!AGBinary::FindKey(data, size, cursor, "auth")) return;

		int32_t count = AGBinary::EnterMap(data, size, cursor);
		if (count < 0) return;

		for (int32_t i = 0; i < count; i++)
		{
			std::string key = AGBinary::ReadString(data, size, cursor);
			if (key == "fail_on_missing")
			{
				AGBinary::Skip(data, size, cursor);
				continue;
			}
			if (!key.empty())
			{
				std::string ticket = AGBinary::ReadString(data, size, cursor);
				if (!ticket.empty())
				{
					HookMetadata::sUserKeys.Platform = key;
					HookMetadata::sUserKeys.PlatformTicket = ticket;
					printfSuccess("Platform: %s, ticket captured (%zu chars)", key.c_str(), ticket.size());
					if (SettingsMgr->ShouldLog(Log::Verbose))
						printf("Stored Auth Info:\nPlatform: %s\nTicket: %s\n", key.c_str(), ticket.c_str());
					CurlCleanupIfDone();
					return;
				}
			}
			AGBinary::Skip(data, size, cursor);
		}
		printfWarning("ParseAccessRequest: failed to extract platform ticket from %d bytes", size);
	}

	static void ParseAccessResponse(MK12::CURL::HTTPRequestWrapper* wrapper)
	{
		char* data = nullptr;
		int32_t size = 0;
		if (!wrapper->GetResponseBody(&data, &size)) return;

		std::string token = AGBinary::GetString(data, size, "token");
		if (token.empty()) return;

		HookMetadata::sUserKeys.AccessToken = token;
		printfSuccess("Access token captured (%zu chars)", token.size());
		if (SettingsMgr->ShouldLog(Log::Verbose))
			printf("Stored Access Token: %s\n", token.c_str());
		CurlCleanupIfDone();
	}

	__int64 __fastcall CurlMultiAddHandleProxy(__int64 multiHandle, __int64 easyHandle)
	{
		if (!Features::IsActive(Features::Id::Curl))
			return MK12::CurlMultiAddHandle(multiHandle, easyHandle);

		if (SettingsMgr->ShouldFlood(SettingsMgr->Floods.bCurl))
			printf("[CURL] multi_add_handle multi=%p easy=%p\n", (void*)multiHandle, (void*)easyHandle);

		if (!bCurlSawRequest)
		{
			auto structIt = CurlHandleStructs.find(easyHandle);
			if (structIt != CurlHandleStructs.end())
				ParseAccessRequest(structIt->second);
		}

		return MK12::CurlMultiAddHandle(multiHandle, easyHandle);
	}

	__int64 __fastcall CurlMultiInfoReadProxy(__int64 multiHandle, __int64* msgsInQueue)
	{
		__int64 result = MK12::CurlMultiInfoRead(multiHandle, msgsInQueue);

		if (result && Features::IsActive(Features::Id::Curl))
		{
			if (SettingsMgr->ShouldFlood(SettingsMgr->Floods.bCurl))
				printf("[CURL] multi_info_read result=%p msgs_in_queue=%lld\n", (void*)result, msgsInQueue ? *msgsInQueue : -1);

			if (*(uint32_t*)result == 1)
			{
				__int64 easyHandle = *(__int64*)(result + 8);

				if (!bCurlSawResponse)
				{
					auto urlIt = CurlHandleUrls.find(easyHandle);
					if (urlIt != CurlHandleUrls.end() && urlIt->second.find("/access") != std::string::npos)
					{
						auto structIt = CurlHandleStructs.find(easyHandle);
						if (structIt != CurlHandleStructs.end())
							ParseAccessResponse(structIt->second);
					}
				}

				CurlHandleUrls.erase(easyHandle);
			}
		}

		return result;
	}
};


// Hooks
namespace MK12Hook::Hooks {

	bool DisablePakSigning()
	{
		printf("\n==DisablePakSigning==\n");

		// Same site as PakTocCheck, the 2nd call is the signing keys delegate getter
		uint64_t patAddr = GamePatcher->ResolvePattern(SettingsMgr->pPakTocCheck, "PakTocCheck");
		if (!patAddr)
			return false;

		uint64_t getterAddr = ProcessPatch::ResolveDestination(patAddr + 9);
		if (!getterAddr)
		{
			printfError("Couldn't resolve the pak signing keys getter");
			return false;
		}

		if (!GamePatcher->ReplaceFunctionWith(getterAddr, MK12Hook::Proxies::GetUnboundPakSigningKeysDelegate))
			return false;

		if (SettingsMgr->ShouldLog(Log::Verbose))
			printf("Pak signing keys getter at: %p\n", (void*)getterAddr);
		printfSuccess("Pak Signing Disabled");

		return true;
	}

	// Start of a name-to-text function, from a pattern on a call inside it
	static uint64_t FindNameFunction(const std::string& Pattern, const char* Name, uint64_t CallOffset, uint64_t BackToStart)
	{
		uint64_t patAddr = GamePatcher->ResolvePattern(Pattern, Name);
		if (!patAddr)
			return 0;
		if (!MK12::FNameToWStr)
			MK12::FNameToWStr = (MK12::FNameToWStrType*)ProcessPatch::ResolveDestination(patAddr + CallOffset);
		uint64_t Start = patAddr - BackToStart;
		if (*(uint8_t*)Start != 0x48)
		{
			printfError("%s start not found!", Name);
			return 0;
		}
		return Start;
	}

	bool UNameTableGetter()
	{
		printf("\n==UNameObjectGetter==\n");

		uint64_t patAddr = GamePatcher->ResolvePattern(SettingsMgr->pUNameObjGetPat, "UNameObjGet");
		if (!patAddr)
			return false;

		uint64_t* lpPattern = (uint64_t*)patAddr;

		uint64_t UNameMain	= ProcessPatch::ResolveDestination((uint64_t)lpPattern, 2, 7, 4) - 0x8;
		uint64_t UNameSub	= ProcessPatch::ResolveDestination((uint64_t)lpPattern + 0x12, 3, 7, 4);

		if (SettingsMgr->ShouldLog(Log::Verbose))
		{
			printf("UNameMain Object found at: %p\n", (uint64_t*)UNameMain);
			printf("UNameSub Object found at: %p\n", (uint64_t*)UNameSub);
		}

		// Find NameTable here for now until moving it to a different place
		MK12::UNameTable = (MK12::UNameTableStruct*)UNameSub;
		MK12::UMainNameTable = (MK12::UNameTableMainStruct*)UNameMain; // Can be UNameTable - 0x10

		printfSuccess("UName Objects Saved!\n");

		printf("\n==InitializeNameTableProxy==\n");

		uint64_t InitNameTableFuncAddr = (uint64_t)lpPattern + 0x22;
		if (*(uint8_t*)InitNameTableFuncAddr != 0xE8) // Call
		{
			printfError("InitializeNameTable Function not found!\n");
			return false;
		}
		if (SettingsMgr->ShouldLog(Log::Verbose))
		{
			printf("InitializeNameTable Function found at: %p\n", (uint64_t*)InitNameTableFuncAddr);
		}

		MK12::InitializeNameTable = (MK12::InitializeNameTableType*)ProcessPatch::ResolveDestination(InitNameTableFuncAddr);
		printfSuccess("InitializeNameTableFunction!\n");

		return true;
	}

	bool NameTableHooks()
	{ // Our copy of how the game turns names into text, it also does string swaps
		printf("\n==NameTableHooks==\n");
		MK12::ReadFNameToWStrWithIdStart = (uint64_t*)FindNameFunction(SettingsMgr->pFPathLoadPat, "FPathLoad", 0xB, 0x6F);
		MK12::ReadFNameToWStrNoIdStart = (uint64_t*)FindNameFunction(SettingsMgr->pFPath2LoadPat, "FPath2Load", 0xB, 0x55);
		MK12::ReadFNameToWStrCommonStart = (uint64_t*)FindNameFunction(SettingsMgr->pFPathCLoadPat, "FPathCLoad", 0x0, 0x10);
		if (!MK12::ReadFNameToWStrWithIdStart || !MK12::ReadFNameToWStrNoIdStart || !MK12::ReadFNameToWStrCommonStart
			|| !MK12::FNameToWStr || !UNameTableGetter())
		{
			printfError("Name Table not working!");
			return false;
		}

		GamePatcher->ReplaceFunctionWith((uint64_t)MK12::ReadFNameToWStrWithIdStart, MK12::Remake::FNameInfoToWStringWithId);
		GamePatcher->ReplaceFunctionWith((uint64_t)MK12::ReadFNameToWStrNoIdStart, MK12::Remake::FNameInfoToWStringNoId);
		GamePatcher->ReplaceFunctionWith((uint64_t)MK12::ReadFNameToWStrCommonStart, MK12::Remake::FNameInfoToWString);
		printfSuccess("Name Table On");
		return true;
	}

	bool OverrideGameEndpointsData()
	{
		printf("\n==OverrideGameEndpointsData==\n");

		if (SettingsMgr->szServerUrl.empty())
		{
			printfWarning("Server Url is empty or not specified. Skipping!");
			return false;
		}

		return GamePatcher->ProxyByPattern(SettingsMgr->pEndpointLoader, "EndpointLoader", 0x00,
			MK12Hook::Proxies::OverrideGameEndpoint, &MK12::GetEndpointKeyValue, PATCH_CALL);
	}

	bool PrivateServerHooks()
	{ // Restart only by design: changing servers mid session would break the game's sessions
		if (!OverrideGameEndpointsData())
			return false;
		HookMetadata::eServerProxyMode = HookMetadata::PROXY_CONFIG;
		printfInfo("Private Server: %s", SettingsMgr->szServerUrl.c_str());
		return true;
	}

	bool ProfileGetterHooks()
	{
		printf("\n==ProfileGetterHooks==\n");
		uint64_t pat = GamePatcher->ResolvePattern(SettingsMgr->pOnlineSubsystem, "OnlineSubsystem");
		if (pat)
			OnlineIds::SetSubsystemGetter((void*)ProcessPatch::ResolveDestination(pat));
		else
			printfWarning("Profile Getter: online ids only show once Floyd's challenges are read");
		pat = GamePatcher->ResolvePattern(SettingsMgr->pOnlineCheck, "OnlineCheck");
		if (pat)
			OnlineIds::SetOnlineCheck((void*)ProcessPatch::ResolveDestination(pat + 3), (void*)ProcessPatch::ResolveDestination(pat + 27));
		else
			printfWarning("Profile Getter: can't tell offline from online, online ids are read for a minute from the main menu");
		pat = GamePatcher->ResolvePattern(SettingsMgr->pAuthToken, "AuthToken");
		if (pat)
			OnlineIds::SetAuthReaders((void*)pat, (void*)ProcessPatch::ResolveDestination(pat + 21), (void*)ProcessPatch::ResolveDestination(pat + 37));
		else
			printfWarning("Profile Getter: the access token and the platform ticket can't be read in this version of the game");

		return GamePatcher->ProxyByPattern(SettingsMgr->pProfileGetter, "ProfileGetter", 30,
			MK12Hook::Proxies::MKWScanfProxyForCrypto, &MK12::MKWScanf, PATCH_CALL);
	}

	bool ResolveVersionFunctions()
	{
		printf("\n==ResolveVersionFunctions==\n");
		uint64_t pat;

		pat = GamePatcher->ResolvePattern(SettingsMgr->pNRSClientVersion, "NRSClientVersion");
		if (pat) MK12::GetNRSClientVersion = (MK12::GetNRSClientVersionType*)ProcessPatch::ResolveDestination(pat + 3);

		pat = GamePatcher->ResolvePattern(SettingsMgr->pBuildVersion, "BuildVersion");
		if (pat) MK12::GetBuildVersion = (MK12::GetBuildVersionType*)ProcessPatch::ResolveDestination(pat + 29);

		pat = GamePatcher->ResolvePattern(SettingsMgr->pBuildDate, "BuildDate");
		if (pat)
		{
			uint64_t intermediate = ProcessPatch::ResolveDestination(pat + 23);
			if (intermediate) MK12::GetBuildDate = (MK12::GetBuildDateType*)ProcessPatch::ResolveDestination(intermediate + 0xD);
		}

		pat = GamePatcher->ResolvePattern(SettingsMgr->pChangelist, "Changelist");
		if (pat) MK12::GetChangelist = (MK12::GetChangelistType*)ProcessPatch::ResolveDestination(pat + 15);

		int resolved = 0;
		if (MK12::GetBuildDate)
		{
			if (SettingsMgr->ShouldLog(Log::Verbose))
				printf("Build Date: %ws\n", MK12::GetBuildDate());
			resolved++;
		}
		if (MK12::GetBuildVersion)
		{
			if (SettingsMgr->ShouldLog(Log::Verbose))
				printf("Build Version: %ws\n", MK12::GetBuildVersion());
			resolved++;
		}
		if (MK12::GetChangelist)
		{
			if (SettingsMgr->ShouldLog(Log::Verbose))
				printf("Changelist: %d\n", MK12::GetChangelist());
			resolved++;
		}
		if (MK12::GetNRSClientVersion) // Cannot print because not yet initialized
			resolved++;

		if (resolved == 4)
			printfSuccess("Resolved %d/4 version functions", resolved);
		else if (resolved > 0)
			printfWarning("Resolved %d/4 version functions", resolved);
		else
			printfError("Resolved %d/4 version functions", resolved);
		return resolved > 0;
	}

	bool GameStateHooks()
	{ // Loading and online checks, plus the per frame tick other features run from
		printf("\n==GameStateHooks==\n");
		uint64_t pat;

		pat = GamePatcher->ResolvePattern(SettingsMgr->pOnlineModeCheck, "OnlineModeCheck");
		if (pat)
		{
			MK12::GetModeManager = (MK12::GetModeManagerType*)ProcessPatch::ResolveDestination(pat + 8);
			MK12::GameModeOnlineClass = (MK12::GameModeOnlineClassType*)ProcessPatch::ResolveDestination(pat + 16);
			MK12::IsModeOnStack = (MK12::IsModeOnStackType*)ProcessPatch::ResolveDestination(pat + 27);
		}

		pat = GamePatcher->ResolvePattern(SettingsMgr->pMainLoop, "MainLoop");
		if (pat) MK12::lpEngineExitRequested = (uint8_t*)ProcessPatch::ResolveDestination(pat + 12, 2, 7, 4);

		pat = GamePatcher->ResolvePattern(SettingsMgr->pModeManagerGetter, "ModeManagerGetter");
		if (pat) MK12::lpGamePhaseOwner = (uint64_t*)ProcessPatch::ResolveDestination(pat + 7, 3, 7, 4);

		if (!GameState::IsReady())
		{
			printfError("Game State not working!");
			return false;
		}

		// A call inside the engine's frame function, it runs however the frame is reached. Its start can't be
		// hooked, MK1Hook finds and hooks it by its first bytes
		if (!GamePatcher->ProxyByPattern(SettingsMgr->pFrameTick, "FrameTick", 6,
			MK12Hook::Proxies::FrameTickProxy, &MK12::FrameTick, PATCH_CALL))
		{
			printfError("Game State not working!");
			return false;
		}

		printfSuccess("Game State On");
		return true;
	}

	bool GameMessageBoxHooks()
	{
		printf("\n==GameMessageBoxHooks==\n");
		uint64_t pat;

		pat = GamePatcher->ResolvePattern(SettingsMgr->pTextFromString, "TextFromString");
		if (pat) MK12::TextFromString = (MK12::TextFromStringType*)ProcessPatch::ResolveDestination(pat + 4);

		pat = GamePatcher->ResolvePattern(SettingsMgr->pMessageBoxSetTitle, "MessageBoxSetTitle");
		if (pat) MK12::MessageBoxSetTitle = (MK12::MessageBoxSetTitleType*)ProcessPatch::ResolveDestination(pat + 15);

		MK12::FindText = (MK12::FindTextType*)GamePatcher->ResolvePattern(SettingsMgr->pFindText, "FindText");
		MK12::MessageBoxConstruct = (MK12::MessageBoxConstructType*)GamePatcher->ResolvePattern(SettingsMgr->pMessageBoxConstruct, "MessageBoxConstruct");
		MK12::MessageBoxAddButton = (MK12::MessageBoxAddButtonType*)GamePatcher->ResolvePattern(SettingsMgr->pMessageBoxAddButton, "MessageBoxAddButton");
		MK12::MessageBoxShow = (MK12::MessageBoxShowType*)GamePatcher->ResolvePattern(SettingsMgr->pMessageBoxShow, "MessageBoxShow");
		MK12::MessageBoxClose = (MK12::MessageBoxCloseType*)GamePatcher->ResolvePattern(SettingsMgr->pMessageBoxClose, "MessageBoxClose");
		MK12::MessageBoxDestruct = (MK12::MessageBoxDestructType*)GamePatcher->ResolvePattern(SettingsMgr->pMessageBoxDestruct, "MessageBoxDestruct");

		if (!GameMessageBox::IsReady())
		{
			printfError("Game Message Box not working!");
			return false;
		}

		MK12::GetLocalPlayerInfo = (MK12::GetLocalPlayerInfoType*)GamePatcher->ResolvePattern(SettingsMgr->pLocalPlayerInfo, "LocalPlayerInfo");
		pat = GamePatcher->ResolvePattern(SettingsMgr->pMessageBoxCallback, "MessageBoxCallback");
		if (pat)
		{
			MK12::MessageBoxCallback.Function = (void*)ProcessPatch::ResolveDestination(pat, 3, 7, 4);
			MK12::MessageBoxCallback.VTable = (void*)ProcessPatch::ResolveDestination(pat + 11, 3, 7, 4);
			MK12::MessageBoxCallback.Invoker = (void*)ProcessPatch::ResolveDestination(pat + 22, 3, 7, 4);
		}
		if (!MK12::GetLocalPlayerInfo || !MK12::MessageBoxCallback.IsReady())
			printfWarning("Game Message Box: Keyboard, controller and sounds may not work on its buttons");

		pat = GamePatcher->ResolvePattern(SettingsMgr->pPlayerInputPressed, "PlayerInputPressed");
		uint64_t InputStart = pat ? ProcessPatch::ResolveDestination(pat + 3) : 0;
		if (!InputStart || !GamePatcher->ProxyFunctionAt(InputStart, MK12Hook::Proxies::PlayerInputPressedProxy, &MK12::PlayerInputPressed, "PlayerInputPressed"))
			printfWarning("Game Message Box: The screen behind a message box may take its button presses");
		uint64_t UpdateStart = GamePatcher->ResolvePattern(SettingsMgr->pPlayerInputUpdate, "PlayerInputUpdate");
		if (!UpdateStart || !GamePatcher->ProxyFunctionAt(UpdateStart, MK12Hook::Proxies::PlayerInputUpdateProxy, &MK12::PlayerInputUpdate, "PlayerInputUpdate"))
			printfWarning("Game Message Box: Keyboard and controller may not reach its buttons");
		pat = GamePatcher->ResolvePattern(SettingsMgr->pToastDriverCall, "ToastDriverCall");
		if (pat) MK12::ToastDriver = (MK12::ToastDriverType*)ProcessPatch::ResolveDestination(pat);
		MK12::ToastEnqueue = (MK12::ToastEnqueueType*)GamePatcher->ResolvePattern(SettingsMgr->pToastEnqueue, "ToastEnqueue");
		MK12::Realloc = (MK12::ReallocType*)GamePatcher->ResolvePattern(SettingsMgr->pRealloc, "Realloc");
		if (!GameToast::IsReady())
			printfWarning("Game Toast not found, notifications won't show");
		if (!MK12::MessageBoxSetTitle)
			printfWarning("Game Message Box titles not found, popups show without them");

		printfSuccess("Game Message Box On");
		return true;
	}

	bool ModSettingsHooks()
	{
		printf("\n==ModSettingsHooks==\n");
		uint64_t pat;

		pat = GamePatcher->ResolvePattern(SettingsMgr->pMenuEntryConstruct, "MenuEntryConstruct");
		if (pat) MK12::MenuEntryConstruct = (MK12::MenuEntryConstructType*)ProcessPatch::ResolveDestination(pat);
		pat = GamePatcher->ResolvePattern(SettingsMgr->pSettingsTabConstruct, "SettingsTabConstruct");
		if (pat) MK12::SettingsTabConstruct = (MK12::SettingsTabConstructType*)ProcessPatch::ResolveDestination(pat + 7);
		pat = GamePatcher->ResolvePattern(SettingsMgr->pSettingsTabDestruct, "SettingsTabDestruct");
		if (pat) MK12::SettingsTabDestruct = (MK12::SettingsTabDestructType*)ProcessPatch::ResolveDestination(pat + 4);
		pat = GamePatcher->ResolvePattern(SettingsMgr->pGrowArray, "GrowArray");
		if (pat) MK12::GrowArray = (MK12::GrowArrayType*)ProcessPatch::ResolveDestination(pat + 3);

		pat = GamePatcher->ResolvePattern(SettingsMgr->pStringReserve, "StringReserve");
		if (pat) MK12::StringReserve = (MK12::StringReserveType*)ProcessPatch::ResolveDestination(pat + 12);
		uint64_t FindTextPlatformStart = GamePatcher->ResolvePattern(SettingsMgr->pFindTextPlatform, "FindTextPlatform");
		uint64_t FindTextStart = (uint64_t)MK12::FindText; // Found by the message box hooks

		if (!MK12::MenuEntryConstruct || !MK12::SettingsTabConstruct || !MK12::SettingsTabDestruct || !MK12::GrowArray
			|| !MK12::StringReserve || !FindTextPlatformStart || !FindTextStart)
		{
			printfError("Mod Settings not working!");
			return false;
		}

		// The tab bar copy must be hooked before the page, a page without its tab would shift every tab after it
		if (!GamePatcher->ProxyByPattern(SettingsMgr->pSettingsCopyTabs, "SettingsCopyTabs", 8,
			MK12Hook::Proxies::SettingsCopyTabsProxy, &MK12::SettingsCopyTabs, PATCH_CALL)
			|| !GamePatcher->ProxyByPattern(SettingsMgr->pSettingsBuildTabs, "SettingsBuildTabs", 0,
			MK12Hook::Proxies::SettingsBuildTabsProxy, &MK12::SettingsBuildTabs, PATCH_CALL)
			|| !GamePatcher->ProxyFunctionAt(FindTextStart, MK12Hook::Proxies::FindTextProxy, &MK12::FindText, "FindText")
			|| !GamePatcher->ProxyFunctionAt(FindTextPlatformStart, MK12Hook::Proxies::FindTextPlatformProxy, &MK12::FindTextPlatform, "FindTextPlatform"))
		{
			printfError("Mod Settings not working!");
			return false;
		}

		MK12::MemoryFree = (MK12::MemoryFreeType*)GamePatcher->ResolvePattern(SettingsMgr->pMemoryFree, "MemoryFree");
		if (!MK12::MemoryFree || !GamePatcher->ProxyByPattern(SettingsMgr->pMainMenuCopyTabs, "MainMenuCopyTabs", 9,
			MK12Hook::Proxies::MainMenuCopyTabsProxy, &MK12::MainMenuCopyTabs, PATCH_CALL))
			printfWarning("Mod Settings: No MK12TTH entry in the main menu, the tab in Settings still works");

		if (!GamePatcher->ProxyByPattern(SettingsMgr->pSettingsVersionText, "SettingsVersionText", 9,
			MK12Hook::Proxies::SettingsVersionTextProxy, &MK12::SettingsVersionText, PATCH_CALL))
			printfWarning("Mod Settings: Our menu shows the game's version");

		uint64_t RunStart = GamePatcher->ResolvePattern(SettingsMgr->pSettingsRun, "SettingsRun");
		if (!RunStart || !GamePatcher->ProxyFunctionAt(RunStart, MK12Hook::Proxies::SettingsRunProxy, &MK12::SettingsRun, "SettingsRun"))
			printfWarning("Mod Settings: The restart question shows while you're still in the menu");

		// The game's page switch steps, for SECTION rows
		pat = GamePatcher->ResolvePattern(SettingsMgr->pSettingsPageSwitch, "SettingsPageSwitch");
		if (pat && *(uint8_t*)(pat + 5) == 0xE8 && *(uint8_t*)(pat + 19) == 0xE8 && *(uint8_t*)(pat + 64) == 0xE8)
		{
			MK12::SettingsUnfocusRow = (MK12::SettingsScreenStepType*)ProcessPatch::ResolveDestination(pat + 5);
			MK12::SettingsShowPage = (MK12::SettingsScreenStepType*)ProcessPatch::ResolveDestination(pat + 19);
			MK12::SettingsRefreshFooter = (MK12::SettingsScreenStepType*)ProcessPatch::ResolveDestination(pat + 64);
			if (*(uint8_t*)(pat + 56) == 0xE8)
				MK12::SettingsFocusRows = (MK12::SettingsFocusRowsType*)ProcessPatch::ResolveDestination(pat + 56);
		}
		if (!MK12::SettingsUnfocusRow)
			printfWarning("Mod Settings: No SECTION rows, each page gets its own entry");

		// COPY in the footer: the game's prompt text, and its footer build
		pat = GamePatcher->ResolvePattern(SettingsMgr->pPromptText, "PromptText");
		if (pat && *(uint8_t*)(pat + 12) == 0xE8)
			MK12::PromptText = (MK12::PromptTextType*)ProcessPatch::ResolveDestination(pat + 12);
		if (!MK12::PromptText || !MK12::Realloc || !GamePatcher->ProxyByPattern(SettingsMgr->pFooterBuild, "FooterBuild", 0,
			MK12Hook::Proxies::FooterBuildProxy, &MK12::FooterBuild, PATCH_CALL))
			printfWarning("Mod Settings: No COPY button, values can be read in the description");

		// MOD SETTINGS in the pause menu: the entry list, its growth, the panel holder, and the KONTROLS panel setup
		pat = GamePatcher->ResolvePattern(SettingsMgr->pGrowPauseEntries, "GrowPauseEntries");
		if (pat) MK12::GrowPauseEntries = (MK12::GrowPauseEntriesType*)ProcessPatch::ResolveDestination(pat + 32);
		pat = GamePatcher->ResolvePattern(SettingsMgr->pPanelHolder, "PanelHolder");
		if (pat && *(uint8_t*)(pat + 25) == 0xE8 && *(uint8_t*)(pat + 41) == 0xE8)
		{
			MK12::GameAlloc = (MK12::GameAllocType*)ProcessPatch::ResolveDestination(pat + 25);
			MK12::PanelHolderConstruct = (MK12::PanelHolderConstructType*)ProcessPatch::ResolveDestination(pat + 41);
		}
		pat = GamePatcher->ResolvePattern(SettingsMgr->pPauseSelectedIndex, "PauseSelectedIndex");
		if (pat) MK12::PauseSelectedIndex = (MK12::PauseSelectedIndexType*)ProcessPatch::ResolveDestination(pat + 12);
		uint64_t CloseStart = GamePatcher->ResolvePattern(SettingsMgr->pPauseClose, "PauseClose");
		bool bPause = MK12::GrowPauseEntries && MK12::GameAlloc && MK12::PanelHolderConstruct && MK12::PauseSelectedIndex
			&& CloseStart && GamePatcher->ProxyFunctionAt(CloseStart, MK12Hook::Proxies::PauseCloseProxy, &MK12::PauseClose, "PauseClose")
			&& GamePatcher->ProxyByPattern(SettingsMgr->pPausePanelPageSetup, "PausePanelPageSetup", 13,
				MK12Hook::Proxies::PausePanelPageSetupProxy, &MK12::PausePanelPageSetup, PATCH_CALL)
			&& GamePatcher->ProxyByPattern(SettingsMgr->pPauseCollectEntries, "PauseCollectEntries", 8,
				MK12Hook::Proxies::PauseCollectEntriesProxy, &MK12::PauseCollectEntries, PATCH_CALL);
		if (!bPause)
			printfWarning("Mod Settings: No MOD SETTINGS in the pause menu");

		printfSuccess("Mod Settings On");
		return true;
	}

	bool PakModsHooks()
	{ // Lists the pak mods and keeps the ones turned off from mounting. Must go in before the game mounts paks
		printf("\n==PakModsHooks==\n");
		PakMods::Init();

		uint64_t MountStart = GamePatcher->ResolvePattern(SettingsMgr->pPakMount, "PakMount");
		if (!MountStart || !GamePatcher->ProxyFunctionAt(MountStart, MK12Hook::Proxies::PakMountProxy, &MK12::PakMount, "PakMount"))
		{
			printfError("Pak Mods not working!");
			return false;
		}
		printfSuccess("Pak Mods On");
		return true;
	}

	bool AttractModeHooks()
	{ // The main menu's idle timer: its frame limit sets the delay, restarting it stops attract mode
		printf("\n==AttractModeHooks==\n");
		uint64_t LimitAt = GamePatcher->ResolvePattern(SettingsMgr->pAttractModeLimit, "AttractModeLimit");
		bool bHooked = LimitAt && AttractMode::Init(LimitAt + 14)
			&& GamePatcher->ProxyByPattern(SettingsMgr->pAttractModeIdle, "AttractModeIdle", 6,
			MK12Hook::Proxies::AttractModeIdleProxy, &MK12::IsModeOnTop, PATCH_CALL);
		if (bHooked)
			AttractMode::SetDelay(AttractMode::FromIni(SettingsMgr->szAttractModeDelay.c_str()));
		if (bHooked)
			printfSuccess("Attract Mode Switch On");
		else
			printfError("Attract Mode Switch not working!");
		return bHooked;
	}

	bool FloydTrackingHooks()
	{
		printf("\n==FloydTrackingHooks==\n");

		int HooksCtr = 0;
		int ExpectedHooksCtr = SettingsMgr->bFloydTowerFromGame ? 5 : 4;

		HooksCtr += GamePatcher->ProxyByPattern(SettingsMgr->pGetChallengesFromHash, "GetChallengesFromHash", 14,
			MK12Hook::Proxies::GenerateFloydCluesFromHashProxy, &MK12::GenerateFloydCluesFromHash, PATCH_CALL);

		HooksCtr += GamePatcher->ProxyByPattern(SettingsMgr->pGetFloydHashInputString, "GetFloydHashInputString", 11,
			MK12Hook::Proxies::CustomCityHashProxy, &MK12::CustomCityHash, PATCH_CALL);

		HooksCtr += GamePatcher->ProxyByPattern(SettingsMgr->pGetFloydHashInputString2, "GetFloydHashInputString2", 11,
			MK12Hook::Proxies::CustomCityHashProxy, &MK12::CustomCityHash, PATCH_CALL);

		HooksCtr += GamePatcher->ProxyByPattern(SettingsMgr->pGetTacoHashInputString, "GetTacoHashInputString", 10,
			MK12Hook::Proxies::TacoCityHashProxy, &MK12::CustomCityHash, PATCH_CALL);

		// Floyd Tower source picked by szFloydTowerMode, Calculated needs no extra hook
		if (_stricmp(SettingsMgr->szFloydTowerMode.c_str(), "Game") && _stricmp(SettingsMgr->szFloydTowerMode.c_str(), "Calculated"))
			printfWarning("Unknown szFloydTowerMode \"%s\", using Game", SettingsMgr->szFloydTowerMode.c_str());
		if (SettingsMgr->bFloydTowerFromGame)
		{
			HooksCtr += GamePatcher->ProxyByPattern(SettingsMgr->pGetTacoSchedule, "GetTacoSchedule", 14,
				MK12Hook::Proxies::TacoScheduleProxy, &MK12::UpdateTacoSchedule, PATCH_CALL);
		}
		printfInfo("Floyd Tower Mode: %s", SettingsMgr->bFloydTowerFromGame ? "Game" : "Calculated");

		if (HooksCtr == ExpectedHooksCtr)
			printfSuccess("Floyd Tracking On");
		else if (HooksCtr)
			printfWarning("Not all Floyd Trackers functioning!");
		else
			printfError("Floyd Tracking not working!");

		return HooksCtr == ExpectedHooksCtr;
	}

	bool FloydTowerAlertHooks()
	{ // Living Towers opening: Floyd Tower alert and Floyd stats
		printf("\n==FloydTowerAlertHooks==\n");
		MK12::GetFloydVictories = (MK12::GetFloydVictoriesType*)GamePatcher->ResolvePattern(SettingsMgr->pGetFloydVictories, "GetFloydVictories");
		MK12::GetStatsContainer = (MK12::GetStatsContainerType*)GamePatcher->ResolvePattern(SettingsMgr->pGetStatsContainer, "GetStatsContainer");
		MK12::GetProfileStat = (MK12::GetProfileStatType*)GamePatcher->ResolvePattern(SettingsMgr->pGetProfileStat, "GetProfileStat");
		uint64_t pat = GamePatcher->ResolvePattern(SettingsMgr->pGetProfileStatBits, "GetProfileStatBits");
		if (pat) MK12::GetProfileStatBits = (MK12::GetProfileStatBitsType*)ProcessPatch::ResolveDestination(pat + 11);
		// The seed function: the profile hash it calls
		pat = GamePatcher->ResolvePattern(SettingsMgr->pFloydSeed, "FloydSeed");
		if (pat && *(uint8_t*)(pat + 0x34) == 0xE8)
			MK12::FloydProfileHash = (MK12::FloydProfileHashType*)ProcessPatch::ResolveDestination(pat + 0x34);
		if (!MK12::FloydProfileHash)
			printfWarning("Floyd Tower Alert: Floyd's challenges show once a tower starts");
		if (!MK12::GetFloydVictories || !GamePatcher->ProxyByPattern(SettingsMgr->pTowersSetup, "TowersSetup", 16,
			MK12Hook::Proxies::TowersSetupProxy, &MK12::TowersSetup, PATCH_CALL))
		{
			printfError("Floyd Tower Alert not working!");
			return false;
		}
		if (!MK12::GetStatsContainer || !MK12::GetProfileStat)
			printfWarning("Floyd Tower Alert: no Floyd stats in PROFILE");
		printfSuccess("Floyd Tower Alert On");
		return true;
	}

	static int32_t* KeepRunningFlag = nullptr; // The game's own "don't pause without focus" setting, its config can reset it
	static bool bKeepRunningForced = false;
	static int32_t KeepRunningGameValue = 0; // What the game had before we set it, given back when it's turned off
	static uint8_t* FocusLostFlag = nullptr; // The game's "paused because it lost focus"

	bool KeepRunningHooks()
	{ // The game's focus lost handler: it pauses quietly unless its setting says no, or pauses with a message
		printf("\n==KeepRunningHooks==\n");
		uint64_t pat = GamePatcher->ResolvePattern(SettingsMgr->pKeepRunning, "KeepRunning");
		if (!pat)
			return false;
		KeepRunningFlag = (int32_t*)ProcessPatch::ResolveDestination(pat, 2, 7, 4);

		pat = GamePatcher->ResolvePattern(SettingsMgr->pFocusLost, "FocusLost");
		bool bStep = false;
		if (pat)
		{
			FocusLostFlag = (uint8_t*)ProcessPatch::ResolveDestination(pat + 6, 2, 7, 4);
			bStep = GamePatcher->ProxyCallAt(pat + 13, MK12Hook::Proxies::FocusLostStepProxy, &MK12::FocusLostStep, PATCH_CALL);
			if (bStep)
				printfSuccess("FocusLost Hooked");
		}
		bool bPause = FocusLostFlag && GamePatcher->ProxyByPattern(SettingsMgr->pFocusLostPause, "FocusLostPause", 8,
			MK12Hook::Proxies::FocusLostPauseProxy, &MK12::FocusLostPause, PATCH_JUMP);
		if (!bStep || !bPause)
			printfWarning("Keep Running: some screens may still pause without focus");
		printfSuccess("Keep Running On");
		return true;
	}

	void SetKeepRunning(bool bOn)
	{ // Read each time the window loses focus. Off leaves the game's own value alone, or gives back the one it had
		if (!KeepRunningFlag)
			return;
		if (bOn)
		{
			if (!bKeepRunningForced)
				KeepRunningGameValue = *KeepRunningFlag;
			bKeepRunningForced = true;
			*KeepRunningFlag = 1;
		}
		else if (bKeepRunningForced)
		{
			bKeepRunningForced = false;
			if (*KeepRunningFlag == 1) // Unless the game changed it since
				*KeepRunningFlag = KeepRunningGameValue;
		}
	}
}

namespace MK12Hook::Proxies {
	void __fastcall FocusLostStepProxy()
	{ // The window just lost focus, the game checks its setting right after
		MK12::FocusLostStep();
		if (Features::IsActive(Features::Id::KeepRunning))
			MK12Hook::Hooks::SetKeepRunning(true);
	}

	void __fastcall FocusLostPauseProxy(void* Mode)
	{ // Screens that pause with a message whatever the setting says
		if (!Features::IsActive(Features::Id::KeepRunning))
		{
			MK12::FocusLostPause(Mode);
			return;
		}
		if (MK12Hook::Hooks::FocusLostFlag)
			*MK12Hook::Hooks::FocusLostFlag = 0; // Same as the quiet path when the setting says no
	}
}

namespace MK12Hook::Hooks {

	constexpr int FAST_BOOT_FULL = 2; // Fast boot's modes: off, splash, full

	bool FastBootHooks()
	{ // The three waits of the logo and legal screens, and in FULL the studio logos and intro movies
		printf("\n==FastBootHooks==\n");
		int Hooked = 0;
		Hooked += GamePatcher->ProxyByPattern(SettingsMgr->pBootWaitFirst, "BootWaitFirst", 3,
			MK12Hook::Proxies::BootScreenWaitProxy, &MK12::BootScreenWait, PATCH_CALL);
		Hooked += GamePatcher->ProxyByPattern(SettingsMgr->pBootWaitSecond, "BootWaitSecond", 5,
			MK12Hook::Proxies::BootScreenWaitProxy, &MK12::BootScreenWait, PATCH_CALL);
		Hooked += GamePatcher->ProxyByPattern(SettingsMgr->pBootWaitThird, "BootWaitThird", 5,
			MK12Hook::Proxies::BootScreenWaitProxy, &MK12::BootScreenWait, PATCH_CALL);
		int Wanted = 3;
		if (Features::StartupMode(Features::Id::FastBoot) == FAST_BOOT_FULL)
		{
			Wanted += 2;
			Hooked += GamePatcher->ProxyByPattern(SettingsMgr->pBootStudioLogos, "BootStudioLogos", 38,
				MK12Hook::Proxies::StudioLogosProxy, &MK12::PlayMovie, PATCH_CALL);
			Hooked += GamePatcher->ProxyByPattern(SettingsMgr->pBootMovieSkip, "BootMovieSkip", 44,
				MK12Hook::Proxies::SkipBootMovieProxy, &MK12::CommandLineParam, PATCH_CALL);
		}
		if (Hooked)
			printfSuccess("Fast Boot On (%d/%d)", Hooked, Wanted);
		else
			printfError("Fast Boot not working!");
		return Hooked > 0;
	}

	bool FloydFightHooks()
	{ // Floyd's AI level as his fight is built, and the game's retry limit (read when a match ends and in the pause menu)
		printf("\n==FloydFightHooks==\n");
		int Hooked = GamePatcher->ProxyByPattern(SettingsMgr->pFloydLevel, "FloydLevel", 11,
			MK12Hook::Proxies::FloydSetLevelProxy, &MK12::FloydSetLevel, PATCH_CALL);
		Hooked += GamePatcher->ProxyByPattern(SettingsMgr->pFloydLevelSession, "FloydLevelSession", 11,
			MK12Hook::Proxies::FloydSetLevelProxy, &MK12::FloydSetLevel, PATCH_CALL);
		if (Hooked < 2)
		{
			printfError("Floyd Fight: Floyd's level hooks not found");
			return false;
		}

		// The secret fight data's vtable, its slot 7 returns the retry limit: mov eax, [limit]; ret
		uint64_t pat = GamePatcher->ResolvePattern(SettingsMgr->pFloydRetries, "FloydRetries");
		uint8_t* Getter = nullptr;
		if (pat)
		{
			void** VTable = (void**)ProcessPatch::ResolveDestination(pat + 0x16, 3, 7, 4);
			Getter = VTable ? (uint8_t*)VTable[7] : nullptr;
		}
		if (!Getter || Getter[0] != 0x8B || Getter[1] != 0x05 || Getter[6] != 0xC3)
		{
			printfError("Floyd Fight: the retry limit wasn't found");
			return false;
		}
		FloydFight::SetRetryLimit((uint32_t*)ProcessPatch::ResolveDestination((uint64_t)Getter, 2, 6, 4));
		printfSuccess("Floyd Fight On");
		return true;
	}

	bool AnnouncerHooks()
	{ // Applied with the game's own function, at fight start, ladder setup and main menu load
		printf("\n==AnnouncerHooks==\n");
		uint64_t pat;
		pat = GamePatcher->ResolvePattern(SettingsMgr->pApplyAnnouncer, "ApplyAnnouncer");
		if (pat) MK12::ApplyAnnouncer = (MK12::ApplyAnnouncerType*)ProcessPatch::ResolveDestination(pat + 9);
		pat = GamePatcher->ResolvePattern(SettingsMgr->pEngineNameConstruct, "EngineNameConstruct");
		if (pat) MK12::EngineNameConstruct = (MK12::EngineNameConstructType*)ProcessPatch::ResolveDestination(pat + 12);
		if (!MK12::ApplyAnnouncer || !MK12::EngineNameConstruct
			|| !GamePatcher->ProxyByPattern(SettingsMgr->pAnnouncerMenuApply, "AnnouncerMenuApply", 26,
			MK12Hook::Proxies::AnnouncerMenuApplyProxy, &MK12::AnnouncerSetter, PATCH_JUMP))
		{
			printfError("Announcer Override not working!");
			return false;
		}
		if (!GamePatcher->ProxyByPattern(SettingsMgr->pAnnouncerReroll, "AnnouncerReroll", 9,
			MK12Hook::Proxies::AnnouncerRerollProxy, &MK12::AnnouncerReroll, PATCH_CALL))
			printfWarning("Announcer Override: ladder fights may re-roll a random announcer");

		// AUTO: the first human player's fighter
		pat = GamePatcher->ResolvePattern(SettingsMgr->pGetCurrentMission, "GetCurrentMission");
		if (pat) MK12::GetCurrentMission = (MK12::GetCurrentMissionType*)ProcessPatch::ResolveDestination(pat);
		pat = GamePatcher->ResolvePattern(SettingsMgr->pGetTeam, "GetTeam");
		if (pat) MK12::GetTeam = (MK12::GetTeamType*)ProcessPatch::ResolveDestination(pat);
		pat = GamePatcher->ResolvePattern(SettingsMgr->pGetTeamDefinitions, "GetTeamDefinitions");
		if (pat) MK12::GetTeamDefinitions = (MK12::GetTeamDefinitionsType*)ProcessPatch::ResolveDestination(pat);
		if (!MK12::GetCurrentMission || !MK12::GetTeam || !MK12::GetTeamDefinitions)
			printfWarning("Announcer Override: AUTO uses your own setting");
		printfSuccess("Announcer Override On");
		return true;
	}

	bool EngineConfigHooks()
	{ // Engine.ini values of pak mods, put in the engine's config right after it loads
		printf("\n==EngineConfigHooks==\n");
		EngineConfig::Scan();
		uint64_t Globals = GamePatcher->ResolvePattern(SettingsMgr->pConfigGlobals, "ConfigGlobals");
		EngineConfig::GameFunctions Functions = {};
		if (Globals)
		{
			Functions.Config = (void**)ProcessPatch::ResolveDestination(Globals, 3, 7, 4);
			Functions.EngineIniName = (void*)ProcessPatch::ResolveDestination(Globals + 0x24, 3, 7, 4);
		}
		Functions.SetString = (void*)GamePatcher->ResolvePattern(SettingsMgr->pConfigSetString, "ConfigSetString");
		Functions.GetString = (void*)GamePatcher->ResolvePattern(SettingsMgr->pConfigGetString, "ConfigGetString");
		Functions.FindFile = (void*)GamePatcher->ResolvePattern(SettingsMgr->pConfigFindFile, "ConfigFindFile");
		MK12::LoadFileToString = (MK12::LoadFileToStringType*)GamePatcher->ResolvePattern(SettingsMgr->pLoadFileToString, "LoadFileToString");
		if (!Functions.Config || !Functions.EngineIniName || !Functions.SetString || !Functions.GetString
			|| !GamePatcher->ProxyByPattern(SettingsMgr->pConfigInit, "ConfigInit", 9,
			MK12Hook::Proxies::ConfigInitProxy, &MK12::InitializeConfigSystem, PATCH_CALL))
		{
			printfError("Engine Config not working!");
			return false;
		}
		EngineConfig::SetGameFunctions(Functions);
		if (!MK12::LoadFileToString)
			printfWarning("Engine Config: only loose Engine.ini files, none from inside paks");
		printfSuccess("Engine Config On");
		return true;
	}

	bool StringSwapHooks()
	{ // Nothing to hook: the name table does the swaps, this loads them
		MK12Hook::Mods::StringSwaps();
		return true;
	}

	bool ExtractFightMetadataFromSecretFightSetupStage()
	{
		printf("\n==ExtractFightMetadataFromSecretFightSetupStage==\n");

		uint64_t patAddr = GamePatcher->ResolvePattern(SettingsMgr->pSecretFightCondPat, "SecretFightConditionSetup");
		if (!patAddr)
			return false;

		MK12::KlassicTowerSecretFightDataOffset = 0x80;

		GamePatcher->ProxyCallAt(patAddr + 77, MK12Hook::Proxies::SetupSecretFightConditionsProxy, &MK12::SetupSecretFightConditions, PATCH_JUMP);
		printfSuccess("SecretFightConditionSetup Proxied");

		return true;
	}

	bool PatchCurl()
	{
		printf("\n==PatchCurl==\n");
		int capabilities = 0;

		// URL Redirect capability
		if (!SettingsMgr->pCurlSetOpt.empty())
		{
			uint64_t pat = GamePatcher->ResolvePattern(SettingsMgr->pCurlSetOpt, "CurlSetOpt");
			if (pat && GamePatcher->ProxyFunctionAt(pat - 0x12, MK12Hook::Proxies::CurlSetOptProxy, &MK12::CurlSetOpt, "curl_easy_setopt"))
			{
				HookMetadata::ActiveModsMap["bCurlRedirect"] = true;
				printfSuccess("Curl: URL Redirect Enabled");
				capabilities++;
			}
		}

		// Request Capture capability
		if (!SettingsMgr->pCurlMultiAddHandle.empty())
		{
			uint64_t pat = GamePatcher->ResolvePattern(SettingsMgr->pCurlMultiAddHandle, "CurlMultiAddHandle");
			if (pat && GamePatcher->ProxyFunctionAt(pat - 0xD, MK12Hook::Proxies::CurlMultiAddHandleProxy, &MK12::CurlMultiAddHandle, "curl_multi_add_handle"))
			{
				HookMetadata::ActiveModsMap["bCurlRequestCapture"] = true;
				printfSuccess("Curl: Request Capture Enabled");
				capabilities++;
			}
		}

		// Response Capture capability
		if (!SettingsMgr->pCurlMultiInfoRead.empty())
		{
			uint64_t pat = GamePatcher->ResolvePattern(SettingsMgr->pCurlMultiInfoRead, "CurlMultiInfoRead");
			if (pat && GamePatcher->ProxyFunctionAt(pat - 0x13, MK12Hook::Proxies::CurlMultiInfoReadProxy, &MK12::CurlMultiInfoRead, "curl_multi_info_read"))
			{
				HookMetadata::ActiveModsMap["bCurlResponseCapture"] = true;
				printfSuccess("Curl: Response Capture Enabled");
				capabilities++;
			}
		}

		if (capabilities > 0)
			printfSuccess("Curl Interception Active (%d/3)", capabilities);
		else
			printfError("Curl Interception: No capabilities enabled");

		return capabilities > 0;
	}

};

namespace MK12Hook::Mods {
	int StringSwaps()
	{
		printf("\n==Swapping Strings==\n");
		std::ifstream file("string_swaps.txt");

		CPPython::string line;
		int iSwapsAmount = 0;

		if (!file.is_open()) {
			printfInfo("No string_swaps.txt, nothing to swap");
			return 0;
		}

		while (std::getline(file, line)) {
			CPPython::string stripped_line = line.strip();
			auto parts = stripped_line.split("->");

			if (parts.size() == 2) {
				CPPython::string a1 = parts[0].strip(" ");
				CPPython::string a2 = parts[1].strip(" ");
				HookMetadata::FSwapTable.insert(a1.c_str(), a2.c_str());
				std::cout << "Inserted: " << a1 << " -> " << a2 << std::endl;
				iSwapsAmount++;
			}
		}

		file.close();
		printfYellow("%d Strings swapped.\n", iSwapsAmount);
		return iSwapsAmount;
	}
}

namespace HookMetadata {
	HHOOK KeyboardProcHook = nullptr;
	HMODULE CurrentDllModule = NULL;
	HANDLE Console = NULL;
};