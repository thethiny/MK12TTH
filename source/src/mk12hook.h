#pragma once
#include "../includes.h"
#include "../utils/Trampoline.h"
#include <unordered_map>
#include <fstream>
#include "mk12.h"
#include "mkutils.h"

namespace MK12Hook {
	namespace Proxies {
		wchar_t**								__fastcall	OverrideGameEndpoint(MK12::JSONEndpointValue, wchar_t**);
		MK12::FKlassicLadderSecretFightData*	__fastcall	SetupSecretFightConditionsProxy(MK12::FKlassicLadderSecretFightData*);
		MK12::TArray<uint32_t>*					__fastcall	GenerateFloydCluesFromHashProxy(MK12::TArray<uint32_t>* ArrayLocation, uint64_t ShuffleSeed, uint64_t unk, uint64_t unk2);
		uint32_t								__fastcall	CustomCityHashProxy(wchar_t** StringToHash);
		uint32_t								__fastcall	TacoCityHashProxy(wchar_t** StringToHash);
		__int64									__fastcall	TacoScheduleProxy(MK12::UTacoTowerSchedule* Schedule, int64_t NowTicks);
		wchar_t*								__fastcall	MKWScanfProxyForCrypto(wchar_t* ResultString, const wchar_t* Format, ...);
		uint64_t*								__fastcall	GetUnboundPakSigningKeysDelegate();
		__int64									__fastcall	CurlSetOptProxy(__int64 handle, __int64 option, __int64 value);
		__int64									__fastcall	CurlMultiAddHandleProxy(__int64 multiHandle, __int64 easyHandle);
		__int64									__fastcall	CurlMultiInfoReadProxy(__int64 multiHandle, __int64* msgsInQueue);
		__int64									__fastcall	FrameTickProxy();
		bool									__fastcall	PlayerInputPressedProxy(void* Player, int32_t Input, MK12::FString* Id, uint32_t Flags);
		void									__fastcall	PlayerInputUpdateProxy(MK12::FPlayerInput* Input, void* Sample);
		__int64									__fastcall	SettingsBuildTabsProxy(MK12::USettingsScreen* Screen, bool bOnline);
		void									__fastcall	SettingsCopyTabsProxy(void* Out, MK12::FSettingsTab* Tabs, int32_t Count, int32_t ExtraA, int32_t ExtraB);
		MK12::FText*							__fastcall	FindTextProxy(MK12::FText* Out, const wchar_t* Table, const wchar_t* Key);
		MK12::FString*							__fastcall	SettingsVersionTextProxy();
		__int64									__fastcall	FooterBuildProxy(MK12::USettingsScreen* Screen, MK12::TArray<MK12::FText>* Labels, void* FocusedRow, bool bNarrate, MK12::TArray<uint8_t>* Bits);
		void									__fastcall	SettingsRunProxy(void* Screen, void* Player, int64_t Mode);
		void									__fastcall	PauseCollectEntriesProxy(void* Source, void* Player, MK12::TArray<MK12::FPauseEntry>* Out);
		void									__fastcall	PausePanelPageSetupProxy(MK12::USettingsScreen* Screen, void* Player, int64_t Mode);
		void									__fastcall	MainMenuCopyTabsProxy(void* Out, MK12::FSettingsTab* Tabs, int32_t Count, int32_t ExtraA, int32_t ExtraB);
		__int64									__fastcall	PakMountProxy(void* PakPlatformFile, const wchar_t* PakFilename, uint32_t PakOrder, const wchar_t* MountPath, bool bLoadIndex);
		MK12::FText*							__fastcall	FindTextPlatformProxy(MK12::FText* Out, const wchar_t* Table, const wchar_t* Key);
		bool									__fastcall	AttractModeIdleProxy(void* ModeManager, void* ModeClass);
		char									__fastcall	TowersSetupProxy(void* TacoSchedule);
		bool									__fastcall	BootScreenWaitProxy(int32_t FirstBootFrames, int32_t Frames, int32_t Counter);
		bool									__fastcall	StudioLogosProxy(const wchar_t* Name, int32_t A, int32_t B, int32_t C, int32_t D, void* Out);
		bool									__fastcall	SkipBootMovieProxy(const wchar_t* CommandLine, const wchar_t* Param);
		__int64									__fastcall	AnnouncerRerollProxy(void* AudioSettings);
		void									__fastcall	PauseCloseProxy(void* Pause);
		uint8_t*								__fastcall	FloydSetLevelProxy(uint8_t* AI, uint8_t Level);
		void									__fastcall	ConfigInitProxy();
		__int64									__fastcall	AnnouncerMenuApplyProxy(void* AudioSettings, MK12::FString* ItemId);
		void									__fastcall	FocusLostStepProxy();
		void									__fastcall	FocusLostPauseProxy(void* Mode);
	};

	namespace Hooks {
		bool DisablePakSigning();
		bool UNameTableGetter();
		bool NameTableHooks();
		bool StringSwapHooks();
		bool OverrideGameEndpointsData();
		bool PrivateServerHooks();
		bool ExtractFightMetadataFromSecretFightSetupStage();
		bool ProfileGetterHooks();
		bool ResolveVersionFunctions();
		bool FloydTrackingHooks();
		bool FloydTowerAlertHooks();
		bool KeepRunningHooks();
		void SetKeepRunning(bool bOn);
		bool FastBootHooks();
		bool AnnouncerHooks();
		bool FloydFightHooks();
		bool EngineConfigHooks();
		bool PatchCurl();
		bool GameStateHooks();
		bool GameMessageBoxHooks();
		bool ModSettingsHooks();
		bool PakModsHooks();
		bool AttractModeHooks();
	};

	namespace Mods {
		int StringSwaps();
	}
}

namespace HookMetadata { //Namespace for helpers for game functions

	typedef std::unordered_map<std::string, bool> ActiveMods;

	struct LibMapsStruct {
		LibFuncStruct ModLoader;
		LibFuncStruct AntiCheatEngine;
		LibFuncStruct CurlSetOpt;
		LibFuncStruct CurlPerform;
	};


	struct CheatsStruct {
		// Pointers
		uint64_t* lpMercy;
		uint64_t* lpGround;
		uint64_t* lpBrut;
		uint64_t* lpBrutB;
		uint64_t* lpMeteor;
		uint64_t* lpDizzy;
		uint64_t* lpFatality;
		uint64_t* lpFatCombo;
		uint64_t* lpNoBlock;
		uint64_t* lpFatalBlow;

		// Flags
		bool bMercy				= false;
		bool bGround			= false;
		bool bBrut				= false;
		bool bBrutB				= false;
		bool bMeteor			= false;
		bool bDizzy				= false;
		bool bFatality			= false;
		bool bFatCombo			= false;
		bool bNoBlock			= false;
		bool bFatalBlow			= false;
	};


	struct UserKeysStruct
	{
		PyString Platform = "";       // "steam" or "epic"
		PyString PlatformTicket = ""; // Steam hex ticket or EGS JWT
		PyString AccessToken = "";    // Session token from /access response
	};

	class SwapTable {
	private:
		std::unordered_map<std::string, MK12::FName*> FMap;
	public:
		void insert(const char* k, const char* v)
		{
			FMap[k] = MK12::FName::FromStr(v);
		}
		void insert(const char* k, MK12::FName* v)
		{
			FMap[k] = v;
		}
		void remove(const char* k) {
			if (has(k))
				FMap.erase(k);
		}
		bool has(const char* k)
		{
			auto it = FMap.find(k);
			return it != FMap.end();
		}
		MK12::FName* get(const char* k) {
			if (has(k))
				return FMap[k];
			return nullptr;
		}
		uint64_t size() { return FMap.size(); }
	};

	struct FloydCluesInfo {
		std::vector<uint8_t> Clues;
		uint32_t UserProfileHash = 0;
		uint32_t FloydEncounters = 0;
		bool HasClues = false; // Encounters starts at 0, so the first set needs its own flag
	};

	struct TacoTowerInfo {
		uint32_t UserProfileHash = 0;
	};

	struct ProfileInfo {
		wchar_t* Platform = nullptr;
		wchar_t* PlatformId = nullptr;
		wchar_t* SaveKey = nullptr;
		wchar_t* HydraId = nullptr;
		wchar_t* WBId = nullptr;
	};


	enum ServerProxyMode { PROXY_NONE, PROXY_CONFIG, PROXY_CURL };
	extern ServerProxyMode		eServerProxyMode;
	extern ActiveMods			ActiveModsMap;
	extern LibMapsStruct		sLFS;
	extern CheatsStruct			sCheatsStruct;
	extern UserKeysStruct		sUserKeys;
	extern HHOOK				KeyboardProcHook;
	extern HMODULE				CurrentDllModule;
	extern HANDLE				Console;
	extern SwapTable			FSwapTable;
	extern FloydCluesInfo		CurrentFloydInfo;
	extern TacoTowerInfo		CurrentTacoInfo;
	extern ProfileInfo			UserProfileInfo;

	bool HasResponseCapture();
	bool CurlCaptureComplete();
};