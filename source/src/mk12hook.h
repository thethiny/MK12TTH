#pragma once
#include "../includes.h"
#include "../utils/Trampoline.h"
#include <unordered_map>
#include "mk12.h"
#include "mkutils.h"

namespace MK12Hook {
	namespace Proxies {
		__int64			__fastcall	ReadFString(__int64, __int64);
		MK12::FName*	__fastcall	ReadFNameToWStr(MK12::FName&, char*);
		HANDLE			__stdcall	CreateFile(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
		wchar_t**		__fastcall	OverrideGameEndpoint(MK12::JSONEndpointValue, wchar_t*&);
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
	};

	namespace Mods {
		int AnnouncerSwap();
	}
}

namespace HookMetadata { //Namespace for helpers for game functions
	struct ActiveMods {
		bool bAntiSigCheck		= false;
		bool bAntiChunkSigCheck = false;
		bool bAntiSigWarn		= false;
		bool bAntiTocSigCheck	= false;
		bool bAntiPakTocCheck	= false;

		bool bFPathIdLoader		= false;
		bool bFPathNoIdLoader	= false;
		bool bFPathCommonLoader = false;
		bool UNameTableGetter	= false;

		bool bGameEndpointSwap	= false;
	};

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


	extern ActiveMods			sActiveMods;
	extern LibMapsStruct		sLFS;
	extern CheatsStruct			sCheatsStruct;
	extern UserKeysStruct		sUserKeys;
	extern HHOOK				KeyboardProcHook;
	extern HMODULE				CurrentDllModule;
	extern HANDLE				Console;
	extern GameReadyState		sGameState;
	extern SwapTable			FSwapTable;
	
};