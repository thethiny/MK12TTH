#pragma once
#include "../includes.h"
#include "../utils/Trampoline.h"
#include <unordered_map>
#include <fstream>
#include "mk12.h"
#include "mkutils.h"

namespace MK12Hook {
	namespace Proxies {
		__int64									__fastcall	ReadFString(__int64, __int64);
		MK12::FName*							__fastcall	ReadFNameToWStr(MK12::FName&, char*);
		HANDLE									__stdcall	CreateFile(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
		wchar_t**								__fastcall	OverrideGameEndpoint(MK12::JSONEndpointValue, wchar_t**);
		MK12::FKlassicLadderSecretFightData*	__fastcall	SetupSecretFightConditionsProxy(MK12::FKlassicLadderSecretFightData*);
		MK12::TArray<uint32_t>*					__fastcall	GenerateFloydCluesFromHashProxy(MK12::TArray<uint32_t>* ArrayLocation, uint64_t ShuffleSeed, uint64_t unk, uint64_t unk2);
		uint32_t								__fastcall	CustomCityHashProxy(wchar_t** StringToHash);
		wchar_t*								__fastcall	MKWScanfProxyForCrypto(wchar_t* ResultString, const wchar_t* Format, ...);
	};

	namespace Hooks {
		bool DisableSignatureCheck();
		bool DisableSignatureWarn();
		bool DisableChunkSigCheck();
		bool DisableTOCSigCheck();
		bool DisablePakTOCCheck();
		bool UNameTableGetter();
		bool FNameToStrWithIdLoader(Trampoline*);
		bool FNameToStrNoIdLoader(Trampoline*);
		bool FNameToStrCommonLoader(Trampoline*);
		bool OverrideFNameToWStrFuncs(Trampoline*);
		bool OverrideGameEndpointsData(Trampoline*);
		bool ExtractFightMetadataFromSecretFightSetupStage(Trampoline*);
		bool ProfileGetterHooks(Trampoline* GameTramp);
		int	 FloydTrackingHooks(Trampoline* GameTramp);
	};

	namespace Mods {
		int AnnouncerSwap();
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

	struct GameReadyState
	{
		PyString szGameVersion	= "";
		bool bSteamKey			= false;
		bool bAuthToken			= false;
		bool bAccessToken		= false;
		bool bUsername			= false;
		bool bSteamID			= false;
		bool bGameVersion		= false;
		bool bHash				= false;
		bool bUnlock			= true;
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
		PyString SteamKeyBody = ""; // Auth Token
		PyString AuthTokenBody = ""; // Send to Access Token
		PyString AuthTokenResponse = ""; // Access Token
	};

	class SwapTable {
	private:
		std::unordered_map<std::string, MK12::FName*> FMap;
		uint64_t count;
	public:
		void insert(const char* k, const char* v)
		{
			FMap[k] = MK12::FNameFunc::FromStr(v);
			count++;
		}
		void insert(const char* k, MK12::FName* v)
		{
			FMap[k] = v;
			count++;
		}
		void remove(const char* k) {
			if (has(k))
			{
				FMap.erase(k);
				count++;
			}
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
		uint64_t size() { return count; }
	};

	struct FloydCluesInfo {
		std::vector<uint8_t> Clues;
		uint32_t UserProfileHash = 0;
		uint32_t FloydEncounters = 0;
	};

	struct ProfileInfo {
		wchar_t* Platform = nullptr;
		wchar_t* PlatformId = nullptr;
		wchar_t* OfflineProfileId = nullptr;
		wchar_t* SaveKey = nullptr;
		wchar_t* HydraId = nullptr;
		wchar_t* WBId = nullptr;
	};


	extern ActiveMods			ActiveModsMap;
	extern LibMapsStruct		sLFS;
	extern CheatsStruct			sCheatsStruct;
	extern UserKeysStruct		sUserKeys;
	extern HHOOK				KeyboardProcHook;
	extern HMODULE				CurrentDllModule;
	extern HANDLE				Console;
	extern GameReadyState		sGameState;
	extern SwapTable			FSwapTable;
	extern FloydCluesInfo		CurrentFloydInfo;
	extern ProfileInfo			UserProfileInfo;
	
};