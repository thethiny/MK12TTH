#include "includes.h"
#include "src/mk11.h"
#include "src/mk11hook.h"
#include <tlhelp32.h> 
#include <VersionHelpers.h>


using namespace Memory::VP;
using namespace hook;


Trampoline* GameTramp, * User32Tramp;

HANDLE hConsole = NULL;

void CreateConsole();
void HooksMain();
void SyncAwait(std::string(*)(void), const char*, bool = false);
void SpawnError(const char*);
void AwaitSyncs();
void PreGameHooks();
void ProcessSettings();
bool OnInitializeHook();

enum ConsoleColors
{
	BLACK = 0,
	BLUE,
	GREEN,
	AQUA,
	RED,
	PURPLE,
	YELLOW,
	WHITE,
	GRAY,
	LIGHTBLUE,
	LIGHTGREEN,
	LIGHTAQUA,
	LIGHTRED,
	LIGHTPURPLE,
	LIGHTYELLOW,
	BRIGHTWHITE,



	GREY = 8, // Synonym
};

#define AsyncError(text) CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)(SpawnError), text, NULL, NULL)

LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
	bool state = lParam >> 31, transition = lParam & 0x40000000;
	auto RepeatCount = LOWORD(lParam);


	//if (code >= 0 && !state) // State 0 -> Down
	//{
	//	if (!transition) // Transition 0 -> Being Pressed (PosEdge), 1 -> Being Held.
	//	{
	//		if (wParam == SettingsMgr->iVKMenuToggle)
	//		{
	//			// GuiMenu->ToggleActive();
	//		}

	//		if (SettingsMgr->bDebug)
	//		{

	//		}

	//	}
	//}
	return CallNextHookEx(0, code, wParam, lParam);
}

void CreateConsole()
{
	AllocConsole();
	FILE* fNull;
	freopen_s(&fNull, "CONOUT$", "w", stdout);
	freopen_s(&fNull, "CONOUT$", "w", stderr);
		
	std::string consoleName = "thethiny's MK1 Mod Console";
	SetConsoleTitleA(consoleName.c_str());
	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD dwMode = 0;
	GetConsoleMode(hConsole, &dwMode);
	dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode(hConsole, dwMode);
	printfCyan("ASIMK12 - Maintained by ");
	printfInfo("@thethiny\n");
	printfGreen("ESettingsManager::bEnableConsoleWindow = true\n");
}


void HooksMain()
{
	// Write here hooks that require game to be loaded, or are not important for game startup like patching exe stuff
	CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)AwaitSyncs, NULL, NULL, NULL); // Async Thread that will perform Sync Blocking tasks
}

void SyncAwait(std::string(*function)(void), const char* string, bool not)
{
	while (function().empty() ^ not);
	std::cout << string << ": " << function() << std::endl;
}

void AwaitSyncs()
{
	// Nothing to Await
}

void PreGameHooks()
{
	GameTramp = Trampoline::MakeTrampoline(GetModuleHandle(nullptr));
	if (SettingsMgr->iVerbose)
		std::cout << "Generated Trampolines" << std::endl;
	 MK11::IAT = ParsePEHeader();

	if (SettingsMgr->bDisableSignatureCheck)
	{
		std::cout << "==bDisableSignatureCheck==" << std::endl;
		if (SettingsMgr->pSigCheck.empty())
		{
			printfRed("pSigCheck Not Specified. Please Add Pattern to ini file!\n");
		}
		else
		{
			uint64_t* lpSigCheckPattern = FindPattern(GetModuleHandleA(NULL), SettingsMgr->pSigCheck);
			if (!lpSigCheckPattern)
			{
				printfError("Couldn't find SigCheck Pattern\n");
			}
			else
			{
				uint64_t hook_address = (uint64_t)lpSigCheckPattern;
				if (SettingsMgr->iVerbose)
					std::cout << "SigCheck Pattern found at: " << std::hex << lpSigCheckPattern << std::dec << std::endl;
				Patch(GetGameAddr(hook_address) -0x14, (uint8_t)0xC3); // ret
				Patch(GetGameAddr(hook_address) - 0x14 + 1 , (uint32_t)0x90909090); // Nop
				MK11::sActiveMods.bAntiSigCheck = true;
				printfSuccess("SigCheck Patched\n");
			}
		}
	}
	//if (SettingsMgr->bDisableSignatureWarn)
	//{
	//	std::cout << "==bDisableSignatureWarn" << std::endl;
	//	if (SettingsMgr->pSigWarn.empty())
	//	{
	//		printfRed("pSigWarn Not Specified. Please Add Pattern to ini file!\n");
	//	}
	//	else
	//	{
	//		uint64_t* lpSigWarnPattern = FindPattern(GetModuleHandleA(NULL), SettingsMgr->pSigWarn);
	//		if (!lpSigWarnPattern)
	//		{
	//			printfError("Couldn't find SigWarn Pattern\n");
	//		}
	//		else
	//		{
	//			uint64_t hook_address = (uint64_t)lpSigWarnPattern;
	//			if (SettingsMgr->iVerbose)
	//				std::cout << "SigWarn Pattern found at: " << std::hex << lpSigWarnPattern << std::dec << std::endl;
	//			// Shift address by -1
	//			uint32_t offset = GetOffsetFromOpCode(hook_address + 0xA, 2, 4);
	//			Patch(GetGameAddr(hook_address) + 0xA, 0xE9); // jmp
	//			Patch(GetGameAddr(hook_address) + 0xA + 1, offset +1); // jmp address+1 cuz we reduced function size by 1
	//			Patch(GetGameAddr(hook_address) + 0xA + 5, (uint8_t)0x90); // nop
	//			MK11::sActiveMods.bAntiSigWarn = true;
	//			printfSuccess("SigWarn Patched\n");
	//		}
	//	}
	//}
	if (SettingsMgr->bDisableChunkSigCheck)
	{
		bool attempt = true;
		std::cout << "==bDisablePakChunkSigCheck==" << std::endl;
		if (SettingsMgr->pChunkSigCheck.empty())
		{
			printfRed("pChunkSigCheck Not Specified. Please Add Pattern to ini file!\n");
			attempt = false;
		}
		if (SettingsMgr->pChunkSigCheckFunc.empty())
		{
			printfRed("pChunkSigCheckFunc Not Specified. Please Add Pattern to ini file!\n");
			attempt = false;
		}
		if (attempt)
		{
			uint64_t* lpChunkSigCheckPattern = FindPattern(GetModuleHandleA(NULL), SettingsMgr->pChunkSigCheck);
			if (!lpChunkSigCheckPattern)
			{
				printfError("Couldn't find ChunkSigCheck Pattern\n");
			}
			else
			{
				if (SettingsMgr->iVerbose)
					std::cout << "ChunkSigCheck Pattern found at: " << std::hex << lpChunkSigCheckPattern << std::dec << std::endl;
			}
			uint64_t* lpChunkSigCheckFuncPattern = FindPattern(GetModuleHandleA(NULL), SettingsMgr->pChunkSigCheckFunc);
			if (!lpChunkSigCheckFuncPattern)
			{
				printfError("Couldn't find ChunkSigCheckFunc Pattern\n");
			}
			else
			{
				if (SettingsMgr->iVerbose)
					std::cout << "ChunkSigCheckFunc Pattern found at: " << std::hex << lpChunkSigCheckFuncPattern << std::dec << std::endl;
			}
			if (lpChunkSigCheckPattern && lpChunkSigCheckFuncPattern)
			{
				uint32_t FuncOffset = ((uint64_t)lpChunkSigCheckFuncPattern) - (((uint64_t)lpChunkSigCheckPattern) + 0xE + 5); // 5 is the size of the opcode, E is the offset to the opcode
				Patch<uint32_t>(((uint64_t)lpChunkSigCheckPattern) + 0xF, FuncOffset);
				MK11::sActiveMods.bAntiChunkSigCheck = true;
				printfSuccess("PakChunkSigCheck Patched\n");
			}
		}
	}
	if (SettingsMgr->bDisableTOCSigCheck)
	{
		std::cout << "==bDisableTOCSigCheck==" << std::endl;
		if (SettingsMgr->pTocCheck.empty())
		{
			printfRed("pTocCheck Not Specified. Please Add Pattern to ini file!\n");
		}
		else
		{
			uint64_t* lpTocSigCheckPattern = FindPattern(GetModuleHandleA(NULL), SettingsMgr->pTocCheck);
			if (!lpTocSigCheckPattern)
			{
				printfError("Couldn't find TocSigCheck Pattern\n");
			}
			else
			{
				uint64_t hook_address = (uint64_t)lpTocSigCheckPattern;
				if (SettingsMgr->iVerbose)
					std::cout << "TocSigCheck Pattern found at: " << std::hex << lpTocSigCheckPattern << std::dec << std::endl;
				ConditionalJumpToJump(hook_address, 0x12);
				MK11::sActiveMods.bAntiTocSigCheck = true;
				printfSuccess("TocSigCheck Patched\n");
			}
		}
	}

	// Temp until address
	/*MK1Functions::MK1ReadFString = (MK1Functions::ReadFString*)(uint64_t*)GetGameAddr(0x22956D0);
	InjectHook(GetGameAddr(0x22954E7), GameTramp->Jump(MK1Functions::ReadFStringProxy), PATCH_CALL);*/
	
}

void ProcessSettings()
{
	// KeyBinds
	SettingsMgr->iVKCheats		= StringToVK(SettingsMgr->hkCheats);
	SettingsMgr->iVKMenuToggle	= StringToVK(SettingsMgr->hkMenu);
	SettingsMgr->iVKMenuInfo	= StringToVK(SettingsMgr->hkInfo);

	// DLL Procs
	MK11::sLFS.ModLoader		= MK11::ParseLibFunc(SettingsMgr->szModLoader);
	MK11::sLFS.AntiCheatEngine	= MK11::ParseLibFunc(SettingsMgr->szAntiCheatEngine);
	MK11::sLFS.CurlSetOpt		= MK11::ParseLibFunc(SettingsMgr->szCurlSetOpt);
	MK11::sLFS.CurlPerform		= MK11::ParseLibFunc(SettingsMgr->szCurlPerform);

	printfCyan("Parsed Settings\n");
}



void SpawnError(const char* msg)
{
	MessageBoxA(NULL, msg, "ASIMK12", MB_ICONEXCLAMATION);
}

bool HandleWindowsVersion()
{
	if (IsWindows10OrGreater())
	{
		return true;
	}

	if (IsWindows7SP1OrGreater())
	{
		AsyncError("ASIMK1 doesn't officially support Windows 8 or 7 SP1. It may misbehave.");
		return true;
	}

	AsyncError("ASIMK1 doesn't support Windows 7 or Earlier. Might not work.");
	return true;

	
}

bool VerifyProcessName()
{
	std::string expected_process("MK12.exe");
	std::string process_name = GetProcessName();
	if (process_name != expected_process)
	{
		return false;

	}
	return true;
}

bool OnInitializeHook()
{

	if (!HandleWindowsVersion())
		return false;

	if (!VerifyProcessName())
		return false;

	SettingsMgr->Init();

	if (SettingsMgr->bEnableConsoleWindow)
	{
		CreateConsole();
	}

	if (SettingsMgr->bPauseOnStart)
	{
		MessageBoxA(0, "Freezing Game Until OK", ":)", MB_ICONINFORMATION);
	}

	ProcessSettings(); // Parse Settings
	PreGameHooks(); // Queue Blocker
	CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)HooksMain, NULL, NULL, NULL);

	return true;
}

bool APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpRes)
{
	HHOOK hook_ = nullptr;
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		if (!OnInitializeHook())
			break;
		if (!(hook_ = SetWindowsHookEx(WH_KEYBOARD, KeyboardProc, hModule, GetCurrentThreadId())))
		{
			char x[100];
			sprintf(x, "Failed To Hook Keyboard FN: 0x%X", GetLastError());
			MessageBox(NULL, x, "Error", MB_ICONERROR);
		}
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		if (hook_)
			UnhookWindowsHookEx(hook_);
		break;
	}
	return true;
}