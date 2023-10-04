#include "mk11hook.h"
#include "../includes.h"


MK1Functions::ReadFString*			MK1Functions::MK1ReadFString;

HANDLE __stdcall MK1Functions::CreateFileProxy(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	if (lpFileName)
	{
		//std::wstring fileName = lpFileName;
		wchar_t* wcFileName = (wchar_t*)lpFileName;
		std::wstring fileName(wcFileName, wcslen(wcFileName));
		if (!wcsncmp(wcFileName, L"..", 2))
		{
			std::wstring wsSwapFolder = L"MKSwap";
			std::wstring newFileName = L"..\\" + wsSwapFolder + fileName.substr(2, fileName.length() - 2);
			if (std::filesystem::exists(newFileName.c_str()))
			{
				wprintf(L"Loading %s from %s\n", wcFileName, wsSwapFolder.c_str());
				MK11::vSwappedFiles.push_back(wcFileName);
				return CreateFileW(newFileName.c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
			}
		}

	}

	return CreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

__int64 MK1Functions::ReadFStringProxy(__int64 rcx, __int64 rdx)
{
	uint32_t* RCXArray = (uint32_t*) rcx;
	uint64_t* R8Array = (uint64_t*)(0x14788f648);
	uint32_t ArrayValue = RCXArray[0];
	uint32_t ArrayValue2 = RCXArray[1];
	uint64_t* r8 = (uint64_t*) R8Array[ArrayValue];
	ArrayValue2 /= 8;
	uint32_t* rax = (uint32_t*)r8[4];
	uint64_t rsi = rax[ArrayValue2];
	rsi += r8[6];
	char* szFString = (char*)rsi;
	char* szBPClass = (char*)(r8[6]);
	printfYellow("szBPClass: %s\nszFString: %s\n", szBPClass, szFString);
	return MK1Functions::MK1ReadFString(rcx, rdx);
}

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

namespace HookMetadata {
	HHOOK KeyboardProcHook		= nullptr;
	HMODULE CurrentDllModule	= NULL;
	HANDLE Console				= NULL;
}

// Hooks
using namespace Memory::VP;
using namespace hook;

bool MK1Hooks::DisableSignatureCheck()
{
	std::cout << "==bDisableSignatureCheck==" << std::endl;
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
		std::cout << "SigCheck Pattern found at: " << std::hex << lpSigCheckPattern << std::dec << std::endl;
	Patch(GetGameAddr(hook_address) - 0x14, (uint8_t)0xC3); // ret
	Patch(GetGameAddr(hook_address) - 0x14 + 1, (uint32_t)0x90909090); // Nop
				
	printfSuccess("SigCheck Patched\n");

	return true;
}

bool MK1Hooks::DisableSignatureWarn()
{
	std::cout << "==bDisableSignatureWarn" << std::endl;
	if (SettingsMgr->pSigWarn.empty())
	{
		printfRed("pSigWarn Not Specified. Please Add Pattern to ini file!\n");
		return false;
	}
		
	uint64_t* lpSigWarnPattern = FindPattern(GetModuleHandleA(NULL), SettingsMgr->pSigWarn);
	if (!lpSigWarnPattern)
	{
		printfError("Couldn't find SigWarn Pattern\n");
		return false;
	}
	uint64_t hook_address = (uint64_t)lpSigWarnPattern;
	if (SettingsMgr->iLogLevel)
		std::cout << "SigWarn Pattern found at: " << std::hex << lpSigWarnPattern << std::dec << std::endl;

	// Shift address by -1
	ConditionalJumpToJump(hook_address, 0xA);
	printfSuccess("SigWarn Patched\n");

	return true;
}

bool MK1Hooks::bDisableChunkSigCheck()
{
	std::cout << "==bDisablePakChunkSigCheck==" << std::endl;
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
		printfError("Couldn't find ChunkSigCheck Pattern\n");
		return false;
	}
	if (SettingsMgr->iLogLevel)
		std::cout << "ChunkSigCheck Pattern found at: " << std::hex << lpChunkSigCheckPattern << std::dec << std::endl;

	uint64_t* lpChunkSigCheckFuncPattern = FindPattern(GetModuleHandleA(NULL), SettingsMgr->pChunkSigCheckFunc);
	if (!lpChunkSigCheckFuncPattern)
	{
		printfError("Couldn't find ChunkSigCheckFunc Pattern\n");
		return false;
	}
	if (SettingsMgr->iLogLevel)
		std::cout << "ChunkSigCheckFunc Pattern found at: " << std::hex << lpChunkSigCheckFuncPattern << std::dec << std::endl;

	uint32_t FuncOffset = ((uint64_t)lpChunkSigCheckFuncPattern) - (((uint64_t)lpChunkSigCheckPattern) + 0xE + 5); // 5 is the size of the opcode, E is the offset to the opcode
	Patch<uint32_t>(((uint64_t)lpChunkSigCheckPattern) + 0xF, FuncOffset);
	printfSuccess("PakChunkSigCheck Patched\n");

	return true;
		
}

bool MK1Hooks::bDisableTOCSigCheck()
{
	std::cout << "==bDisableTOCSigCheck==" << std::endl;
	if (SettingsMgr->pTocCheck.empty())
	{
		printfRed("pTocCheck Not Specified. Please Add Pattern to ini file!\n");
		return false;
	}

	uint64_t* lpTocSigCheckPattern = FindPattern(GetModuleHandleA(NULL), SettingsMgr->pTocCheck);
	if (!lpTocSigCheckPattern)
	{
		printfError("Couldn't find TocSigCheck Pattern\n");
		return false;
	}
		
	uint64_t hook_address = (uint64_t)lpTocSigCheckPattern;
	if (SettingsMgr->iLogLevel)
		std::cout << "TocSigCheck Pattern found at: " << std::hex << lpTocSigCheckPattern << std::dec << std::endl;

	ConditionalJumpToJump(hook_address, 0x12);
	printfSuccess("TocSigCheck Patched\n");

	return true;		
}