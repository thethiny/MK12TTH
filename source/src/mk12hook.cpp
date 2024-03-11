#include "mk12hook.h"


HookMetadata::ActiveMods			HookMetadata::sActiveMods;
HookMetadata::CheatsStruct			HookMetadata::sCheatsStruct;
HookMetadata::LibMapsStruct			HookMetadata::sLFS;
HookMetadata::UserKeysStruct		HookMetadata::sUserKeys;
HookMetadata::GameReadyState		HookMetadata::sGameState;
HookMetadata::SwapTable				HookMetadata::FSwapTable;



namespace MK12Hook::Proxies {

	HANDLE __stdcall CreateFile(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
	{
		//if (lpFileName)
		//{
		//	//std::wstring fileName = lpFileName;
		//	wchar_t* wcFileName = (wchar_t*)lpFileName;
		//	std::wstring fileName(wcFileName, wcslen(wcFileName));
		//	if (!wcsncmp(wcFileName, L"..", 2))
		//	{
		//		std::wstring wsSwapFolder = L"MKSwap";
		//		std::wstring newFileName = L"..\\" + wsSwapFolder + fileName.substr(2, fileName.length() - 2);
		//		if (std::filesystem::exists(newFileName.c_str()))
		//		{
		//			wprintf(L"Loading %s from %s\n", wcFileName, wsSwapFolder.c_str());
		//			MK12::vSwappedFiles.push_back(wcFileName);
		//			return CreateFileW(newFileName.c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
		//		}
		//	}

		//}

		return CreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
	}

	__int64 ReadFString(__int64 rcx, __int64 rdx)
	{
		uint32_t* RCXArray = (uint32_t*)rcx;
		uint64_t* R8Array = (uint64_t*)(0x14788f648);
		uint32_t ArrayValue = RCXArray[0];
		uint32_t ArrayValue2 = RCXArray[1];
		uint64_t* r8 = (uint64_t*)R8Array[ArrayValue];
		ArrayValue2 /= 8;
		uint32_t* rax = (uint32_t*)r8[4];
		uint64_t rsi = rax[ArrayValue2];
		rsi += r8[6];
		char* szFString = (char*)rsi;
		char* szBPClass = (char*)(r8[6]);
		printfYellow("szBPClass: %s\nszFString: %s\n", szBPClass, szFString);
		return MK12::ReadFString(rcx, rdx);
	}

	//const char* SubzeroFatality = "/Game/Disk/Char/SubZero/Cine/Fatality/Roster/A/FatalityA.FatalityA";
	const char* SubzeroFatality = "/Game/WwiseAudio/Switches/SWITCHES_All/SWGP_Announcer/SWGP_Announcer-SWTC_ANNO_ShangTsung.SWGP_Announcer-SWTC_ANNO_ShangTsung";
	const uint16_t SubzeroFatalitySize = (uint16_t)strlen(SubzeroFatality);
	/*MK12::FName* ThanksGivingFatalityFName = MK12::FNameFunc::FromStr("/Game/DLC/REL_OmniMan/Shared/Cine/Fatality/Fatality.Fatality");*/
	// MK12::FName* ThanksGivingFatalityFName = MK12::FNameFunc::FromStr("/Game/Disk/Char/SubZero/Cine/Fatality/Roster/B/FatalityB.FatalityB"); // See if the issue is the size therefore another function needs to be hooked
	MK12::FName* ThanksGivingFatalityFName = MK12::FNameFunc::FromStr("/Game/WwiseAudio/Switches/SWITCHES_All/SWGP_Announcer/SWGP_Announcer-SWTC_ANNO_SubZero.SWGP_Announcer-SWTC_ANNO_SubZero"); // See if the issue is the size therefore another function needs to be hooked

	void ReadFNameToWStrId(MK12::FName& Name, char* dest)
	{
		printf("Proxy FNameToWStrId::Captured::");
		MK12::FNameFunc::Print(Name);

		MK12::FName *current_fname = ReadFNameToWStr(Name, dest);

		RegisterHacks::MoveToRBX(MK12::FNameFunc::GetSize(*current_fname));
	}
	void ReadFNameToWStrNoId(MK12::FName& Name, char* dest)
	{
		printf("Proxy FNameToWStrNoId::Captured::");
		MK12::FNameFunc::Print(Name);

		MK12::FName *current_fname = ReadFNameToWStr(Name, dest);

		RegisterHacks::MoveToRBX((uint64_t)&current_fname->NameSize);
	}

	void ReadFNameToWStrCommon(MK12::FName& Name, char* dest)
	{
		printf("Proxy FNameToWStrCommon::Captured::");
		MK12::FNameFunc::Print(Name);

		MK12::FName* current_fname = ReadFNameToWStr(Name, dest);

		RegisterHacks::MoveToRBX((uint64_t)&current_fname->NameSize); // Needs testing
	}

	MK12::FName* ReadFNameToWStr(MK12::FName &Name, char* dest)
	{
		const char* name = MK12::FNameFunc::ToStr(Name);
		MK12::FName* swap = HookMetadata::FSwapTable.get(name);
		if (swap == nullptr)
		{
			MK12::FNameToWStr(Name, dest);
			return &Name;
		}
		
		printf("Swap To::");
		MK12::FNameFunc::Print(*swap);
		MK12::FNameToWStr(*swap, dest);
		
		return swap;
	}

	wchar_t** OverrideGameEndpoint(MK12::JSONEndpointValue obj, wchar_t*& strAddr)
	{
		CPPython::string ServerUrl = SettingsMgr->szServerUrl;

		if (HookMetadata::sActiveMods.bGameEndpointSwap && !ServerUrl.empty())
		{
			ServerUrl = ServerUrl.strip("/");

			int StringLength = (ServerUrl.size() + 1) * 2;
			std::wstring wServerUrl = std::wstring(ServerUrl.begin(), ServerUrl.end());
			wprintf(L"Replacing endpoint \"%s\" with \"%s\"!\n", obj.ValuePointer, wServerUrl.c_str());

			obj.ValuePointer = new wchar_t[StringLength];
			memcpy(obj.ValuePointer, wServerUrl.c_str(), StringLength);

			obj.ValueLength = ServerUrl.size() + 1; // Characters count in str and not wstr
			obj.ValueLength8BAligned = (obj.ValueLength + 7) & (~7);

		}

		wchar_t** returned_value = MK12::GetEndpointKeyValue(obj, strAddr);
		return returned_value;

	}
};

std::map<int, const char*> CURL_MAP
{
	{46,	"CURLOPT_UPLOAD"},
	{47,	"CURLOPT_POST"},
	{10001,	"CURLOPT_WRITEDATA"},
	{10029,	"CURLOPT_HEADERDATA"},
	{10002,	"CURLOPT_URL"},
	{10004,	"CURLOPT_PROXY"},
	{10173,	"CURLOPT_USERNAME"},
	{10005,	"CURLOPT_USERPWD"},
	{10023,	"CURLOPT_HTTPHEADER"},
	{60,	"CURLOPT_POSTFIELDSIZE"},
	{20012,	"CURLOPT_READFUNCTION"},
	{10009,	"CURLOPT_READDATA"},
	{10010,	"CURLOPT_ERRORBUFFER"},
	{8,		"CURLPROTO_FTPS"},
	{10015,	"CURLOPT_POSTFIELDS"},
	{20011,	"CURLOPT_WRITEFUNCTION"},
	{10018,	"CURLOPT_USERAGENT"},
	{80,	"CURLOPT_HTTPGET"},
	{81,	"CURLOPT_SSL_VERIFYHOST"},
	{14,	"CURLOPT_INFILESIZE"},
	{64,	"CURLOPT_SSL_VERIFYPEER"},
	{99,	"CURLOPT_NOSIGNAL"},
	{41,	"CURLOPT_VERBOSE"},
	{42,	"CURLOPT_HEADER"},
	{43,	"CURLOPT_NOPROGRESS"},
	{10086,	"CURLOPT_SSLCERTTYPE"},
	{20079, "CURLOPT_HEADERFUNCTION"},
	{20108, "CURLOPT_SSL_CTX_FUNCTION"},
	{10065, "CURLOPT_CAINFO"},
	{10097, "CURLOPT_CAPATH"},

};

bool bFirstPost = true;

enum class ArgTypes {
	ARGTYPE_NONE = 0, // UNK
	ARGTYPE_STRING, // String Pointer
	ARGTYPE_AGBINARY, // Data
	ARGTYPE_FUNCTION, // Callback
	ARGTYPE_CANCEL, // Return 0
	ARGTYPE_SETZERO, // Arg3 becomes 0
	ARGTYPE_INT, // Integer
	ARGTYPE_STRUCT, // Struct Pointer
};

ArgTypes GetArgType(const char* arg_type)
{
	if (arg_type == "CURLOPT_URL")
		return ArgTypes::ARGTYPE_STRING;
	if (arg_type == "CURLOPT_USERAGENT")
		return ArgTypes::ARGTYPE_STRING;
	if (arg_type == "CURLOPT_WRITEDATA")
		return ArgTypes::ARGTYPE_STRUCT;
	if (arg_type == "CURLOPT_USERNAME")
		return ArgTypes::ARGTYPE_STRUCT;
	if (arg_type == "CURLOPT_USERPWD")
		return ArgTypes::ARGTYPE_STRUCT;
	if (arg_type == "CURLOPT_HEADERDATA")
		return ArgTypes::ARGTYPE_AGBINARY;
	if (arg_type == "CURLOPT_READDATA")
		return ArgTypes::ARGTYPE_AGBINARY;
	if (arg_type == "CURLOPT_WRITEFUNCTION")
		return ArgTypes::ARGTYPE_FUNCTION;
	if (arg_type == "CURLOPT_READFUNCTION")
		return ArgTypes::ARGTYPE_FUNCTION;
	if (arg_type == "CURLOPT_SSL_VERIFYPEER")
		return ArgTypes::ARGTYPE_SETZERO;
	if (arg_type == "CURLOPT_SSL_VERIFYHOST")
		return ArgTypes::ARGTYPE_SETZERO;
	if (arg_type == "CURLOPT_INFILESIZE")
		return ArgTypes::ARGTYPE_INT;
	if (arg_type == "CURLOPT_POSTFIELDSIZE")
		return ArgTypes::ARGTYPE_INT;
	if (arg_type == "CURLOPT_CAPATH")
		return ArgTypes::ARGTYPE_STRING;
	if (arg_type == "CURLOPT_CAINFO")
		return ArgTypes::ARGTYPE_STRING;
	if (arg_type == "CURLOPT_HEADERFUNCTION")
		return ArgTypes::ARGTYPE_FUNCTION;
	if (arg_type == "CURLOPT_SSL_CTX_FUNCTION")
		return ArgTypes::ARGTYPE_FUNCTION;
	if (arg_type == "CURLOPT_SSLCERTTYPE")
		return ArgTypes::ARGTYPE_STRING;
	return ArgTypes::ARGTYPE_NONE;
}

// Hooks
namespace MK12Hook::Hooks {
	using namespace Memory::VP;
	using namespace hook;

	bool DisableSignatureCheck()
	{
		printf("\n==DisableSignatureCheck==\n");
		if (SettingsMgr->pSigCheck.empty())
		{
			printfRed("pSigCheck Not Specified. Please Add Pattern to ini file!\n");
			return false;
		}

		uint64_t* lpSigCheckPattern = FindPattern(GetModuleHandleA(NULL), SettingsMgr->pSigCheck);
		if (!lpSigCheckPattern)
		{
			printfError("Couldn't find SigCheck Pattern\n");
			return false;
		}

		uint64_t hook_address = (uint64_t)lpSigCheckPattern;
		if (SettingsMgr->iLogLevel)
			printf("SigCheck Pattern found at: %p\n", lpSigCheckPattern);
		Patch(GetGameAddr(hook_address) - 0x14, (uint8_t)0xC3); // ret
		Patch(GetGameAddr(hook_address) - 0x14 + 1, (uint32_t)0x90909090); // Nop

		printfSuccess("SigCheck Patched");

		return true;
	}

	bool DisableSignatureWarn()
	{
		printf("\n==DisableSignatureWarn==\n");
		if (SettingsMgr->pSigWarn.empty())
		{
			printfRed("pSigWarn Not Specified. Please Add Pattern to ini file!\n");
			return false;
		}

		uint64_t* lpSigWarnPattern = FindPattern(GetModuleHandleA(NULL), SettingsMgr->pSigWarn);
		if (!lpSigWarnPattern)
		{
			printfError("Couldn't find SigWarn Pattern");
			return false;
		}
		uint64_t hook_address = (uint64_t)lpSigWarnPattern;
		if (SettingsMgr->iLogLevel)
			printf("SigWarn Pattern found at: %p\n", lpSigWarnPattern);

		// Shift address by -1
		ConditionalJumpToJump(hook_address, 0xA);
		printfSuccess("SigWarn Patched");

		return true;
	}

	bool DisableChunkSigCheck()
	{
		printf("\n==DisablePakChunkSigCheck==\n");
		if (SettingsMgr->pChunkSigCheck.empty())
		{
			printfRed("pChunkSigCheck Not Specified. Please Add Pattern to ini file!\n");
			return false;
		}
		if (SettingsMgr->pChunkSigCheckFunc.empty())
		{
			printfRed("pChunkSigCheckFunc Not Specified. Please Add Pattern to ini file!\n");
			return false;
		}

		uint64_t* lpChunkSigCheckPattern = FindPattern(GetModuleHandleA(NULL), SettingsMgr->pChunkSigCheck);
		if (!lpChunkSigCheckPattern)
		{
			printfError("Couldn't find ChunkSigCheck Pattern");
			return false;
		}
		if (SettingsMgr->iLogLevel)
			printf("ChunkSigCheck Pattern found at: %p\n", lpChunkSigCheckPattern);

		uint64_t* lpChunkSigCheckFuncPattern = FindPattern(GetModuleHandleA(NULL), SettingsMgr->pChunkSigCheckFunc);
		if (!lpChunkSigCheckFuncPattern)
		{
			printfError("Couldn't find ChunkSigCheckFunc Pattern");
			return false;
		}
		if (SettingsMgr->iLogLevel)
			printf("ChunkSigCheckFunc Pattern found at: %p\n", lpChunkSigCheckFuncPattern);

		uint32_t FuncOffset = ((uint64_t)lpChunkSigCheckFuncPattern) - (((uint64_t)lpChunkSigCheckPattern) + 0xE + 5); // 5 is the size of the opcode, E is the offset to the opcode
		Patch<uint32_t>(((uint64_t)lpChunkSigCheckPattern) + 0xF, FuncOffset);
		printfSuccess("PakChunkSigCheck Patched");

		return true;

	}

	bool DisableTOCSigCheck()
	{
		printf("\n==DisableTOCSigCheck==\n");
		if (SettingsMgr->pTocCheck.empty())
		{
			printfRed("pTocCheck Not Specified. Please Add Pattern to ini file!\n");
			return false;
		}

		uint64_t* lpTocSigCheckPattern = FindPattern(GetModuleHandleA(NULL), SettingsMgr->pTocCheck);
		if (!lpTocSigCheckPattern)
		{
			printfError("Couldn't find TocSigCheck Pattern");
			return false;
		}

		uint64_t hook_address = (uint64_t)lpTocSigCheckPattern;
		if (SettingsMgr->iLogLevel)
			printf("TocSigCheck Pattern found at: %p\n", lpTocSigCheckPattern);

		ConditionalJumpToJump(hook_address, 0x12);
		printfSuccess("TocSigCheck Patched");

		return true;
	}

	bool DisablePakTOCCheck()
	{
		printf("\n==DisablePakTOCCheck==\n");
		if (SettingsMgr->pPakTocCheck.empty())
		{
			printfRed("pPakTocCheck Not Specified. Please Add Pattern to ini file!\n");
			return false;
		}

		uint64_t* lpPakTocCheckPattern = FindPattern(GetModuleHandleA(NULL), SettingsMgr->pPakTocCheck);
		if (!lpPakTocCheckPattern)
		{
			printfError("Couldn't find PakTocCheck Pattern");
			return false;
		}

		uint64_t hook_address = (uint64_t)lpPakTocCheckPattern;
		if (SettingsMgr->iLogLevel)
			printf("PakTocCheck Pattern found at: %p\n", lpPakTocCheckPattern);

		Patch<uint8_t>(hook_address + 0x12, 0xEB); // je to short jump
		printfSuccess("PakTocCheck Patched");

		return true;
	}

	bool FNameToStrWithIdLoader(Trampoline* GameTramp)
	{
		printf("\n==ProxyFPathIdLoader==\n");
		std::string pattern = SettingsMgr->pFPathLoadPat;
		if (pattern.empty())
		{
			printfRed("pFPathLoadPat Not Specified. Please Add Pattern to ini file!\n");
			return false;
		}

		uint64_t* lpPattern = FindPattern(GetModuleHandleA(NULL), pattern);
		if (!lpPattern)
		{
			printfError("Couldn't find FPathLoad Pattern");
			return false;
		}

		uint64_t call_address = ((uint64_t)lpPattern) + 0xB;
		if (SettingsMgr->iLogLevel)
			printf("FPathLoad Pattern found at: %p\n", lpPattern);

		MakeProxyFromOpCode(GameTramp, call_address, (uint8_t)4, MK12Hook::Proxies::ReadFNameToWStrId, &MK12::FNameToWStr, PATCH_CALL);	

		printfSuccess("FPathLoad Proxied");

		uint64_t func_start = ((uint64_t)lpPattern) - 0x6F;
		if (*(uint8_t*)func_start != 0x48)
		{
			printfError("FPathLoad Start not found!");
		}
		else
		{
			MK12::ReadFNameToWStrWithIdStart = (uint64_t*)func_start;
			if (SettingsMgr->iLogLevel)
				printf("FPathLoad Func Start is at: %p\n", MK12::ReadFNameToWStrWithIdStart);
		}

		return true;
	}

	bool FNameToStrNoIdLoader(Trampoline* GameTramp)
	{
		printf("\n==ProxyFPathNoIdLoader==\n");
		std::string pattern = SettingsMgr->pFPath2LoadPat;
		if (pattern.empty())
		{
			printfRed("pFPath2LoadPat Not Specified. Please Add Pattern to ini file!\n");
			return false;
		}

		uint64_t* lpPattern = FindPattern(GetModuleHandleA(NULL), pattern);
		if (!lpPattern)
		{
			printfError("Couldn't find FPath2Load Pattern");
			return false;
		}

		uint64_t call_address = ((uint64_t)lpPattern) + 0xB;
		if (SettingsMgr->iLogLevel)
			printf("FPath2Load Pattern found at: %p\n", lpPattern);

		MakeProxyFromOpCode(GameTramp, call_address, (uint8_t)4, MK12Hook::Proxies::ReadFNameToWStrNoId, &MK12::FNameToWStr, PATCH_CALL); // Using the same FNameToWStr var cuz all functions use the same call

		printfSuccess("FPath2Load Proxied");

		uint64_t func_start = ((uint64_t)lpPattern) - 0x55;
		if (*(uint8_t*)func_start != 0x48)
		{
			printfError("FPath2Load Start not found!");
		}
		else
		{
			MK12::ReadFNameToWStrNoIdStart = (uint64_t*)func_start;
			if (SettingsMgr->iLogLevel)
				printf("FPath2Load Func Start is at: %p\n", MK12::ReadFNameToWStrNoIdStart);
		}

		return true;
	}

	bool FNameToStrCommonLoader(Trampoline* GameTramp)
	{
		printf("\n==ProxyFPathCommonLoader==\n");
		std::string pattern = SettingsMgr->pFPathCLoadPat;
		if (pattern.empty())
		{
			printfError("pFPathCLoadPath Not Specified. Please Add Pattern to ini file!");
			return false;
		}

		uint64_t* lpPattern = FindPattern(GetModuleHandleA(NULL), pattern);
		if (!lpPattern)
		{
			printfError("Couldn't find FPathCLoad Pattern");
			return false;
		}

		uint64_t call_address = ((uint64_t)lpPattern) + 0x00;
		if (SettingsMgr->iLogLevel)
			printf("FPathCLoad Pattern Found at: %p\n", lpPattern);

		MakeProxyFromOpCode(GameTramp, call_address, (uint8_t)4, MK12Hook::Proxies::ReadFNameToWStrCommon, &MK12::FNameToWStr, PATCH_CALL);

		printfSuccess("FPathCLoad Proxied");

		uint64_t func_start = ((uint64_t)lpPattern) - 0x10; // Change this address
		if (*(uint8_t*)func_start != 0x48)
		{
			printfError("FPathCLoad Start not found!");
		}
		else
		{
			MK12::ReadFNameToWStrCommonStart = (uint64_t*)func_start;
			if (SettingsMgr->iLogLevel)
				printf("FPathCLoad Func Start is at: %p\n", MK12::ReadFNameToWStrCommonStart);
		}

		return true;
	}

	bool UNameTableGetter()
	{
		printf("\n==UNameObjectGetter==\n");
		std::string pattern = SettingsMgr->pUNameObjGetPat;
		if (pattern.empty())
		{
			printfRed("pUNameObjGetPat Not Specified. Please Add Pattern to ini file!\n");
			return false;
		}

		uint64_t* lpPattern = FindPattern(GetModuleHandleA(NULL), pattern);
		if (!lpPattern)
		{
			printfError("Couldn't find UNameObjGet Pattern");
			return false;
		}

		if (SettingsMgr->iLogLevel)
			printf("UNameObjGet Pattern found at: %p\n", lpPattern);

		uint64_t UNameMain	= GetDestinationFromOpCode((uint64_t)lpPattern, 2, 7, 4) - 0x8;
		uint64_t UNameSub	= GetDestinationFromOpCode((uint64_t)lpPattern + 0x12, 3, 7, 4);

		if (SettingsMgr->iLogLevel)
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
		if (SettingsMgr->iLogLevel)
		{
			printf("InitializeNameTable Function found at: %p\n", (uint64_t*)InitNameTableFuncAddr);
		}

		MK12::InitializeNameTable = (MK12::InitializeNameTableType*)GetDestinationFromOpCode(InitNameTableFuncAddr);
		printfSuccess("InitializeNameTableFunction!\n");

		return true;
	}

	bool OverrideFNameToWStrFuncs(Trampoline* GameTramp)
	{
		if (SettingsMgr->iLogLevel)
			printfYellow("String Swap Mode set to Override!\n");

		if (!SettingsMgr->bUNameGetter)
		{
			printfYellow("Overriding FNameToWStr disabled::UNameGetter Disabled!\n");
			return false;
		}
		if (!HookMetadata::sActiveMods.UNameTableGetter)
		{
			printfError("Overriding FNameToWStr disabled::UNameGetter Not found!");
			return false;
		}
			
		if (MK12::ReadFNameToWStrWithIdStart == nullptr)
		{
			if (SettingsMgr->bFNameToStrHook)
			{
				if (HookMetadata::sActiveMods.bFPathIdLoader)
				{
					printfYellow("Failed to override FNameToWStrId, but Proxy Loader is working.\n");
				}
				else
				{
					printfError("Failed to override FNameToWStrId and Proxy Loader is NOT working!");
				}
			}
		}
		else
		{
			InjectHook(GetGameAddr((uint64_t)MK12::ReadFNameToWStrWithIdStart), GameTramp->Jump(MK12::Remake::FNameInfoToWStringWithId), PATCH_JUMP);
			if (SettingsMgr->bFNameToStrHook && HookMetadata::sActiveMods.bFPathIdLoader)
				printfYellow("FNameToWStrId replacing Proxy Loader\n");
			else
				printfSuccess("Overrided FNameToWStrId skipping Proxy Loader");
		}

		if (MK12::ReadFNameToWStrNoIdStart == nullptr)
		{
			if (SettingsMgr->bFNameToStrHook)
			{
				if (HookMetadata::sActiveMods.bFPathNoIdLoader)
				{
					printfYellow("Failed to override FNameToWStrNoId, but Proxy Loader is working.\n");
				}
				else
				{
					printfError("Failed to override FNameToWStrNoId and Proxy Loader is NOT working!");
				}
			}
		}
		else
		{
			InjectHook(GetGameAddr((uint64_t)MK12::ReadFNameToWStrNoIdStart), GameTramp->Jump(MK12::Remake::FNameInfoToWStringNoId), PATCH_JUMP);
			if (SettingsMgr->bFNameToStrHook && HookMetadata::sActiveMods.bFPathNoIdLoader)
				printfYellow("FNameToWStrNoId replacing Proxy Loader\n");
			else
				printfSuccess("Overrided FNameToWStrNoId skipping Proxy Loader");
		}

		if (MK12::ReadFNameToWStrCommonStart == nullptr)
		{
			if (SettingsMgr->bFNameToStrHook)
			{
				if (HookMetadata::sActiveMods.bFPathCommonLoader)
				{
					printfYellow("Failed to override FNameToWStrCommon, but Proxy Loader is working.\n");
				}
				else
				{
					printfError("Failed to override FNameToWStrCommon and Proxy Loader is NOT working!");
				}
			}
		}
		else
		{
			InjectHook(GetGameAddr((uint64_t)MK12::ReadFNameToWStrCommonStart), GameTramp->Jump(MK12::Remake::FNameInfoToWString), PATCH_JUMP);
			if (SettingsMgr->bFNameToStrHook && HookMetadata::sActiveMods.bFPathCommonLoader)
				printfYellow("FNameToWStrCommon replacing Proxy Loader\n");
			else
				printfSuccess("Overrided FNameToWStrCommon skipping Proxy Loader");
		}

		return true;
	}

	bool OverrideGameEndpointsData(Trampoline* GameTramp)
	{
		printf("\n==OverrideGameEndpointsData==\n");
		std::string pattern = SettingsMgr->pEndpointLoader;
		if (pattern.empty())
		{
			printfError("pEndpointLoader Not Specified. Please Add Pattern to ini file!");
			return false;
		}

		if (SettingsMgr->szServerUrl.empty())
		{
			printfWarning("Server Url is empty or not specified. Skipping!");
			return false;
		}

		uint64_t* lpPattern = FindPattern(GetModuleHandleA(NULL), pattern);
		if (!lpPattern)
		{
			printfError("Couldn't find EndpointLoader Pattern");
			return false;
		}

		uint64_t call_address = ((uint64_t)lpPattern) + 0x00;
		if (SettingsMgr->iLogLevel)
			printf("EndpointLoader Pattern Found at: %p", lpPattern);

		MakeProxyFromOpCode(GameTramp, call_address, (uint8_t)4, MK12Hook::Proxies::OverrideGameEndpoint, &MK12::GetEndpointKeyValue, PATCH_CALL);

		printfSuccess("EndpointLoader Proxied");

		return true;
	}

};

namespace MK12Hook::Mods {
	static const char* AnnouncerSwapString = "/Game/WwiseAudio/Switches/SWITCHES_All/SWGP_Announcer/SWGP_Announcer-SWTC_ANNO_%s.SWGP_Announcer-SWTC_ANNO_%s";
	static const std::string* AllAnnouncerStrings[] = {
		&SettingsMgr->AnnouncerSwap.szDefault,
		&SettingsMgr->AnnouncerSwap.szGeras,
		&SettingsMgr->AnnouncerSwap.szJohnnyCage,
		&SettingsMgr->AnnouncerSwap.szLiuKang,
		&SettingsMgr->AnnouncerSwap.szOmniMan,
		&SettingsMgr->AnnouncerSwap.szShangTsung,
		&SettingsMgr->AnnouncerSwap.szSindel,
		&SettingsMgr->AnnouncerSwap.szSubZero
	};
	static const char* AllAnnouncerNames[] = {
		"Default",
		"Geras",
		"JohnnyCage",
		"LiuKang",
		"OmniMan",
		"ShangTsung",
		"Sindel",
		"SubZero"
	};
	int AnnouncerSwap()
	{
		printf("\n==Swapping Announcers==\n");
		if (!SettingsMgr->bFNameToStrHook)
		{
			printfError("No FPath Swapper has been loaded. Make sure bFPathLoader is enabled.");
			return false;
		}
		if (!HookMetadata::sActiveMods.bFPathIdLoader)
		{
			printfError("No FPath Swapper has been loaded. FNameToStrWithIdLoader Patching was unsuccessful.");
			return false;
		}
	
		char CurAnnoString[256];
		char SwapAnnoString[256];
		int count = 0;
		for (int i = 0; i < 8; i++)
		{
			if (AllAnnouncerStrings[i]->empty())
				continue;

			if (strcmp(AllAnnouncerStrings[i]->c_str(), AllAnnouncerNames[i]) == 0) // No Swap
				continue;
			
			sprintf(CurAnnoString, AnnouncerSwapString, AllAnnouncerNames[i], AllAnnouncerNames[i]);
			sprintf(SwapAnnoString, AnnouncerSwapString, AllAnnouncerStrings[i]->c_str(), AllAnnouncerStrings[i]->c_str());
			HookMetadata::FSwapTable.insert(CurAnnoString, SwapAnnoString);
			count++;
		}

		printfYellow("%d Announcers swapped.\n", count);
		
		return count;
	}
}

namespace HookMetadata {
	HHOOK KeyboardProcHook = nullptr;
	HMODULE CurrentDllModule = NULL;
	HANDLE Console = NULL;

};