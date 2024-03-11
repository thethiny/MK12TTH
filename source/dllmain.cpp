#include "includes.h"
#include "src/mkutils.h"
#include "src/mk12.h"
#include "src/mk12hook.h"
#include "mk12sdk/sdk.h"
#include <tlhelp32.h> 
#include <VersionHelpers.h>

constexpr const char * CURRENT_HOOK_VERSION = "0.2.3";

Trampoline* GameTramp, * User32Tramp;

void CreateConsole();
void SpawnError(const char*);
void PreGameHooks();
void ProcessSettings();
bool OnInitializeHook();

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
	FreeConsole();
	AllocConsole();

	FILE* fNull;
	freopen_s(&fNull, "CONOUT$", "w", stdout);
	freopen_s(&fNull, "CONOUT$", "w", stderr);

	std::string consoleName = "thethiny's MK1 Mod Console";
	SetConsoleTitleA(consoleName.c_str());
	HookMetadata::Console = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD dwMode = 0;
	GetConsoleMode(HookMetadata::Console, &dwMode);
	dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode(HookMetadata::Console, dwMode);

	printfCyan("MK12TTH - Maintained by ");
	printfInfo("@thethiny");
	printfGreen("ESettingsManager::bEnableConsoleWindow = true\n");
}

void PreGameHooks()
{
	GameTramp = Trampoline::MakeTrampoline(GetModuleHandle(nullptr));
	if (SettingsMgr->iLogLevel)
		printf("Generated Trampolines\n");
	 IATable = ParsePEHeader();	 

	 if (false)
	 {
		 std::cout << "Test" << std::endl;
		 auto adddddr = 0x14230B620;
		 Memory::VP::InjectHook(adddddr, GameTramp->Jump(MK12::Remake::GetArgBoolByNameWrapper), PATCH_JUMP); // Function not implemented properly
		 std::cout << "DONE" << std::endl;
	 }

	if (SettingsMgr->bDisableSignatureCheck)
	{
		HookMetadata::sActiveMods.bAntiSigCheck			= MK12Hook::Hooks::DisableSignatureCheck();
	}
	if (SettingsMgr->bDisableSignatureWarn)
	{
		HookMetadata::sActiveMods.bAntiSigWarn			= MK12Hook::Hooks::DisableSignatureWarn();
	}
	if (SettingsMgr->bDisableChunkSigCheck)
	{
		HookMetadata::sActiveMods.bAntiChunkSigCheck	= MK12Hook::Hooks::DisableChunkSigCheck();
	}
	if (SettingsMgr->bDisableTOCSigCheck)
	{
		HookMetadata::sActiveMods.bAntiTocSigCheck		= MK12Hook::Hooks::DisableTOCSigCheck();
	}
	if (SettingsMgr->bDisablePakTOCCheck)
	{
		HookMetadata::sActiveMods.bAntiPakTocCheck		= MK12Hook::Hooks::DisablePakTOCCheck();
	}
	if (SettingsMgr->bFNameToStrHook)
	{
		RegisterHacks::EnableRegisterHacks();
		HookMetadata::sActiveMods.bFPathIdLoader		= MK12Hook::Hooks::FNameToStrWithIdLoader(GameTramp);
		HookMetadata::sActiveMods.bFPathNoIdLoader		= MK12Hook::Hooks::FNameToStrNoIdLoader(GameTramp);
		HookMetadata::sActiveMods.bFPathCommonLoader	= MK12Hook::Hooks::FNameToStrCommonLoader(GameTramp);
	}
	if (SettingsMgr->bUNameGetter)
	{
		HookMetadata::sActiveMods.UNameTableGetter		= MK12Hook::Hooks::UNameTableGetter();
		MK12Hook::Hooks::OverrideFNameToWStrFuncs(GameTramp);
	}
	if (SettingsMgr->AnnouncerSwap.bEnable)
	{
		int swaps = MK12Hook::Mods::AnnouncerSwap();
	}
	if (SettingsMgr->bEnableServerProxy)
	{
		HookMetadata::sActiveMods.bGameEndpointSwap		= MK12Hook::Hooks::OverrideGameEndpointsData(GameTramp);
	}


	// Temp until address
	/*MK1Functions::MK1ReadFString = (MK1Functions::ReadFString*)(uint64_t*)GetGameAddr(0x22956D0);
	InjectHook(GetGameAddr(0x22954E7), GameTramp->Jump(MK1Functions::ReadFStringProxy), PATCH_CALL);*/
	
}

void ProcessSettings()
{
	// KeyBinds
	SettingsMgr->iVKCheats				= StringToVK(SettingsMgr->hkCheats);
	SettingsMgr->iVKMenuToggle			= StringToVK(SettingsMgr->hkMenu);
	SettingsMgr->iVKMenuInfo			= StringToVK(SettingsMgr->hkInfo);

	// DLL Procs
	HookMetadata::sLFS.ModLoader		= ParseLibFunc(SettingsMgr->szModLoader);
	HookMetadata::sLFS.AntiCheatEngine	= ParseLibFunc(SettingsMgr->szAntiCheatEngine);
	HookMetadata::sLFS.CurlSetOpt		= ParseLibFunc(SettingsMgr->szCurlSetOpt);
	HookMetadata::sLFS.CurlPerform		= ParseLibFunc(SettingsMgr->szCurlPerform);

	printfCyan("Parsed Settings\n");
}

void SpawnError(const char* msg)
{
	MessageBoxA(NULL, msg, "MK12TTH", MB_ICONEXCLAMATION);
}

bool HandleWindowsVersion()
{
	if (IsWindows10OrGreater())
	{
		return true;
	}

	if (IsWindows7SP1OrGreater())
	{
		SpawnError("ASIMK1 doesn't officially support Windows 8 or 7 SP1. It may misbehave.");
		return true;
	}

	SpawnError("ASIMK1 doesn't support Windows 7 or Earlier. Might not work.");
	return true;

	
}

#include <string>

inline bool VerifyProcessName() {
	std::string expected_process = "mk12.exe";
	std::string process_name = GetProcessName();

	for (size_t i = 0; i < process_name.length(); ++i) {
		process_name[i] = std::tolower(process_name[i]);
	}

	return (process_name == expected_process);
}


bool OnInitializeHook()
{
	FirstRunMgr->Init();
	SettingsMgr->Init();

	if (!HandleWindowsVersion())
		return false;

	if (!SettingsMgr->bAllowNonMK && !VerifyProcessName())
	{
		SpawnError("This dll is made to work only with MK12.exe");
		return false;
	}

	if (SettingsMgr->bEnableConsoleWindow)
	{
		CreateConsole();
	}

	if (SettingsMgr->bEnableKeyboardHotkeys)
	{
		if (!(HookMetadata::KeyboardProcHook = SetWindowsHookEx(WH_KEYBOARD, KeyboardProc, HookMetadata::CurrentDllModule, GetCurrentThreadId())))
		{
			char x[100];
			sprintf(x, "Failed To Hook Keyboard FN: 0x%X", GetLastError());
			MessageBox(NULL, x, "Error", MB_ICONERROR);
		}
	}

	if (SettingsMgr->bPauseOnStart)
	{
		std::cout << "Current cOut Log Level" << SettingsMgr->iLogLevel << std::endl;
		printf("Current stdOut Log Level -> %d\n", SettingsMgr->iLogLevel);
		MessageBoxA(0, "Freezing Game Until OK", ":)", MB_ICONINFORMATION);
	}

	ProcessSettings(); // Parse Settings
	PreGameHooks(); // Queue Blocker

	return true;
}

// Plugin Stuff

const char* MK12HookPlugin::GetPluginName()
{
	return "MK12TTH";
}

const char* MK12HookPlugin::GetPluginProject()
{
	return "MK12HOOK";
}

const char* MK12HookPlugin::GetPluginTabName()
{
	return "TT Hook";
}

void MK12HookPlugin::OnInitialize()
{
	if (MK12HOOKSDK::IsOK())
		return;

	printfInfo("Called as an EHP Plugin");
	MK12HOOKSDK::Initialize();
}

void MK12HookPlugin::OnShutdown()
{
	if (HookMetadata::KeyboardProcHook) // Will be unloaded once by DLL, and once by EHP.
	{
		UnhookWindowsHookEx(HookMetadata::KeyboardProcHook);
		HookMetadata::KeyboardProcHook = nullptr;
	}
}

void MK12HookPlugin::OnFrameTick()
{
	// Things like async calls and polling tasks
}

void MK12HookPlugin::OnFightStartup()
{
	// not implemented yet
}

char* GetVersionedHookName()
{
	static char tabText[512];
	static bool bPrecalc = false;

	if (bPrecalc)
		return tabText;

	std::strcpy(tabText, MK12HookPlugin::GetPluginName());
	std::strcat(tabText, " Version ");
	std::strcat(tabText, CURRENT_HOOK_VERSION);
	bPrecalc = true;
	return tabText;
}

void MK12HookPlugin::TabFunction()
{
	if (!MK12HOOKSDK::IsOK())
		return;

	static int counter = 0;
	MK12HOOKSDK::ImGui_Text(GetVersionedHookName());

	if (MK12HOOKSDK::ImGui_CollapsingHeader("Patches"))
	{

		bool toggles[] = {
			HookMetadata::sActiveMods.bAntiSigCheck,
			HookMetadata::sActiveMods.bAntiChunkSigCheck,
			HookMetadata::sActiveMods.bAntiTocSigCheck,
			HookMetadata::sActiveMods.bFPathIdLoader,
		};
		MK12HOOKSDK::ImGui_Checkbox("Pak Signature", &toggles[0]);
		MK12HOOKSDK::ImGui_Checkbox("Chunk Signature", &toggles[1]);
		MK12HOOKSDK::ImGui_Checkbox("TOC Signature", &toggles[2]);
		MK12HOOKSDK::ImGui_Checkbox("FString - Announcer", &toggles[3]);

		//if (MK12HOOKSDK::ImGui_Button("button"))
		//	counter++;
	}
	MK12HOOKSDK::ImGui_Separator();

	if (HookMetadata::sActiveMods.UNameTableGetter)
	{
		if (MK12HOOKSDK::ImGui_CollapsingHeader("UE4"))
		{
			MK12HOOKSDK::ImGui_Text("UNameTable");

			bool bIsNameInit = MK12::UMainNameTable->IsInitialized;
			int iNameTablesCount = MK12::UNameTable->TablesCount;
			int iNameTablesSize = MK12::UNameTable->ProbTablesSize;

			MK12HOOKSDK::ImGui_Checkbox("Is UName Initialized", &bIsNameInit);
			MK12HOOKSDK::ImGui_InputInt("UName Tables Count", &iNameTablesCount);
			MK12HOOKSDK::ImGui_InputInt("UName Tables Size", &iNameTablesSize);

		}
	}
	else
	{
		MK12HOOKSDK::ImGui_Text("UName Object Not Patched - UName Info Disabled");
	}
	/*if (MK12HOOKSDK::ImGui_CollapsingHeader("Input"))
	{
		MK12HOOKSDK::ImGui_InputInt("InputInt", &counter);


		static float flt = 1.0f;

		MK12HOOKSDK::ImGui_InputFloat("InputFloat", &flt);

		MK12HOOKSDK::ImGui_Separator();

		static char comboBuffer[256] = {};

		MK12HOOKSDK::ImGui_InputText("InputText", comboBuffer, sizeof(comboBuffer));

		static const char* comboContent[3] = {
			"One",
			"Two",
			"Three",
		};

		if (MK12HOOKSDK::ImGui_BeginCombo("Combo", comboBuffer))
		{
			for (int n = 0; n < 3; n++)
			{
				bool is_selected = (comboBuffer == comboContent[n]);
				if (MK12HOOKSDK::ImGui_Selectable(comboContent[n], is_selected))
					sprintf_s(comboBuffer, comboContent[n]);
				if (is_selected)
					MK12HOOKSDK::ImGui_SetItemDefaultFocus();

			}
			MK12HOOKSDK::ImGui_EndCombo();
		}
	}
	if (MK12HOOKSDK::ImGui_CollapsingHeader("Color"))
	{
		static float color[4] = {};
		MK12HOOKSDK::ImGui_ColorEdit4("RGB pick", color);
	}*/

}

// Dll Entry

bool APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpRes)
{
	HookMetadata::CurrentDllModule = hModule;
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		//MK12HookPlugin::OnInitialize(hModule);
		printfInfo("On Attach Initialize");
		OnInitializeHook();
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		MK12HookPlugin::OnShutdown();
		break;
	}
	return true;
}