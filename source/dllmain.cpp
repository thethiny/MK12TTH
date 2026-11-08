#include "includes.h"
#include "src/mkutils.h"
#include "src/mk12.h"
#include "src/mk12hook.h"
#include "src/GamePatchManager.h"
#include "src/FloydTower.h"
#include "src/GameMessageBox.h"
#include "src/GameToast.h"
#include "src/ModSettings.h"
#include "src/Features.h"
#include "src/Announcer.h"
#include "src/StateFile.h"
#include "mk12sdk/sdk.h"
#include <tlhelp32.h>
#include <string>
#include <VersionHelpers.h>

constexpr const char * CURRENT_HOOK_VERSION = "1.0.0";


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
	//	{t
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
	GamePatcher->Init(GetModuleHandle(nullptr), CURRENT_HOOK_VERSION);
	if (SettingsMgr->ShouldLog(Log::Verbose))
		printf("Generated Trampolines\n");
	 IATable = ProcessPatch::ParseImportTable();

	// Every feature, in dependency order. Each one ends up installed, off in the ini, failed or blocked
	ModSettings::SetModVersion(CURRENT_HOOK_VERSION);
	Features::DefineAll();
	Features::InstallAll();
	if (Features::IsInstalled(Features::Id::ModSettings))
		ModSettings::Init(Features::IsInstalled(Features::Id::PakMods));
	Features::PrintSummary();

	CachedPatternsMgr->Flush();
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

inline bool VerifyProcessName(std::string expected_process) {
	std::string process_name = GetProcessName();

	for (size_t i = 0; i < process_name.length(); ++i) {
		process_name[i] = std::tolower(process_name[i]);
	}

	for (size_t i = 0; i < expected_process.length(); ++i) {
		expected_process[i] = std::tolower(expected_process[i]);
	}

	return (process_name == expected_process);
}


bool OnInitializeHook()
{
	StateFile::Migrate();
	FirstRunMgr->Init();
	SettingsMgr->Init();

	if (!HandleWindowsVersion())
		return false;

	if (!SettingsMgr->bAllowNonMK && !VerifyProcessName("mk12.exe"))
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
		printf("Current Log Level -> %d\n", SettingsMgr->iLogLevel);
		int result = MessageBoxA(0, "Press OK to continue, Cancel to quit.", "MK12TTH - Paused", MB_OKCANCEL | MB_ICONINFORMATION);
		if (result == IDCANCEL)
			ExitProcess(0);
	}

	ProcessSettings(); // Parse Settings

	uint64_t EXEHash = ProcessPatch::HashTextSection();
	char dllPath[MAX_PATH];
	GetModuleFileNameA(HookMetadata::CurrentDllModule, dllPath, MAX_PATH);
	std::string dllName = dllPath;
	size_t lastSlash = dllName.find_last_of("\\/");
	if (lastSlash != std::string::npos) dllName = dllName.substr(lastSlash + 1);
	size_t dot = dllName.find_last_of('.');
	if (dot != std::string::npos) dllName = dllName.substr(0, dot);
	for (size_t i = 0; i < dllName.length(); i++) dllName[i] = std::tolower(dllName[i]);
	CachedPatternsMgr->Init(EXEHash, CURRENT_HOOK_VERSION, dllName.c_str());

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
	if (Features::IsInstalled(Features::Id::Announcer))
		Announcer::OnFightStartup();
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

// Text from mods and the game for the overlay: a lone % could be read as a format code, so it's doubled
static std::string OverlaySafe(std::string Text)
{
	for (size_t At = Text.find('%'); At != std::string::npos; At = Text.find('%', At + 2))
		Text.insert(At, 1, '%');
	return Text;
}

void MK12HookPlugin::TabFunction()
{
	if (!MK12HOOKSDK::IsOK())
		return;

	static int counter = 0;
	MK12HOOKSDK::ImGui_Text(GetVersionedHookName());

	if (Features::IsInstalled(Features::Id::VersionInfo))
	{
		static char versionLabel[256] = {};
		static bool versionResolved = false;

		if (!versionResolved)
		{
			const wchar_t* buildDate = MK12::GetBuildDate ? MK12::GetBuildDate() : nullptr;
			int changelist = MK12::GetChangelist ? MK12::GetChangelist() : 0;
			const wchar_t* nrsVer = nullptr;

			if (MK12::GetNRSClientVersion)
			{
				MK12::FString* ver = MK12::GetNRSClientVersion();
				if (ver && !ver->IsEmpty())
					nrsVer = ver->c_str();
			}

			if (buildDate || changelist || nrsVer)
			{
				snprintf(versionLabel, sizeof(versionLabel), "Game: %ws | %s%ws | CL-%d",
					buildDate ? buildDate : L"",
					nrsVer ? "v" : "",
					nrsVer ? nrsVer : L"",
					changelist);

				if (nrsVer)
					versionResolved = true;
			}
		}

		if (versionLabel[0])
			MK12HOOKSDK::ImGui_Text(versionLabel);
	}

	if (MK12HOOKSDK::ImGui_CollapsingHeader("Features"))
	{
		// How each feature started, the switches are in LIVE FEATURES below and in Settings, MK12TTH
		for (int i = 0; i < (int)Features::Id::Count; i++)
		{
			Features::Id Which = (Features::Id)i;
			const char* State = "not set up";
			switch (Features::GetState(Which))
			{
			case Features::State::Installed:	State = Features::IsActive(Which) ? "on" : "switched off"; break;
			case Features::State::Disabled:		State = "off in Settings, MK12TTH"; break;
			case Features::State::Failed:		State = "couldn't be set up"; break;
			case Features::State::Blocked:		State = "needs another feature"; break;
			default: break;
			}
			char Line[192];
			snprintf(Line, sizeof(Line), "%ls: %s", Features::Get(Which).Title, State);
			MK12HOOKSDK::ImGui_Text(Line);
		}
	}

	// The same pages and rows as MODS in the main menu, changes apply the same way
	if (ModSettings::IsAvailable())
	{
		for (const ModSettings::OverlayPage& Page : ModSettings::OverlayPages())
		{
			if (!MK12HOOKSDK::ImGui_CollapsingHeader(Page.Title.c_str()))
				continue;
			for (const ModSettings::OverlayRow& Row : Page.Rows)
			{
				char Label[256];
				snprintf(Label, sizeof(Label), "%s##%p", Row.Title.c_str(), Row.Id);
				if (Row.bCategory)
				{
					MK12HOOKSDK::ImGui_Separator();
					MK12HOOKSDK::ImGui_Text(OverlaySafe(Row.Title).c_str());
				}
				else if (Row.bAction)
				{
					if (MK12HOOKSDK::ImGui_Button(Label))
						ModSettings::OverlayPress(Row.Id);
				}
				else if (!Row.Choices.empty())
				{
					const char* Shown = Row.Picked >= 0 && Row.Picked < (int)Row.Choices.size() ? Row.Choices[Row.Picked].c_str() : "";
					if (MK12HOOKSDK::ImGui_BeginCombo(Label, Shown))
					{
						for (int i = 0; i < (int)Row.Choices.size(); i++)
						{
							char Item[160];
							snprintf(Item, sizeof(Item), "%s##%d", Row.Choices[i].c_str(), i);
							if (MK12HOOKSDK::ImGui_Selectable(Item, i == Row.Picked) && i != Row.Picked)
								ModSettings::OverlayPick(Row.Id, i);
							if (i == Row.Picked)
								MK12HOOKSDK::ImGui_SetItemDefaultFocus();
						}
						MK12HOOKSDK::ImGui_EndCombo();
					}
				}
				else
				{
					std::string Line = Row.Title + ": " + Row.Value;
					MK12HOOKSDK::ImGui_Text(OverlaySafe(Line).c_str());
					// Read-only values: what's under the row's own description (lists, times, warnings)
					size_t Rest = Row.Value.empty() ? std::string::npos : Row.Text.find("\n\n");
					if (Rest != std::string::npos)
						MK12HOOKSDK::ImGui_Text(OverlaySafe("    " + Row.Text.substr(Rest + 2)).c_str());
				}
			}
		}
	}

	if (Features::IsInstalled(Features::Id::NameTable))
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

	if (MK12HOOKSDK::ImGui_CollapsingHeader("Network"))
	{
		if (Features::IsInstalled(Features::Id::PrivateServer))
		{
			char buffer[512];
			snprintf(buffer, sizeof(buffer), "Server Proxy: %s (Config)", SettingsMgr->szServerUrl.c_str());
			MK12HOOKSDK::ImGui_Text(buffer);
		}
		else
		{
			MK12HOOKSDK::ImGui_Text("Server Proxy: Disabled");
		}

		if (Features::IsInstalled(Features::Id::Curl))
		{
			MK12HOOKSDK::ImGui_Separator();

			bool bRedirect = HookMetadata::ActiveModsMap["bCurlRedirect"];
			bool bRequestCapture = HookMetadata::ActiveModsMap["bCurlRequestCapture"];
			bool bResponseCapture = HookMetadata::ActiveModsMap["bCurlResponseCapture"];

			MK12HOOKSDK::ImGui_Checkbox("URL Redirect", &bRedirect);
			MK12HOOKSDK::ImGui_Checkbox("Request Capture", &bRequestCapture);
			MK12HOOKSDK::ImGui_Checkbox("Response Capture", &bResponseCapture);
		}

		MK12HOOKSDK::ImGui_Separator();

		static char platformBuffer[16];
		strncpy(platformBuffer, HookMetadata::sUserKeys.Platform.c_str(), sizeof(platformBuffer) - 1);
		MK12HOOKSDK::ImGui_InputText("Auth Platform", platformBuffer, sizeof(platformBuffer));

		static char ticketBuffer[2048];
		strncpy(ticketBuffer, HookMetadata::sUserKeys.PlatformTicket.c_str(), sizeof(ticketBuffer) - 1);
		MK12HOOKSDK::ImGui_InputText("Auth Ticket", ticketBuffer, sizeof(ticketBuffer));

		static char tokenBuffer[2048];
		strncpy(tokenBuffer, HookMetadata::sUserKeys.AccessToken.c_str(), sizeof(tokenBuffer) - 1);
		MK12HOOKSDK::ImGui_InputText("Access Token", tokenBuffer, sizeof(tokenBuffer));
	}

	if (Features::IsInstalled(Features::Id::ProfileGetter))
	{
		if (MK12HOOKSDK::ImGui_CollapsingHeader("Profile Information"))
		{
			auto DisplayWCharField = [](const char* label, const wchar_t* value) {
				static char buffer[512];
				if (value)
					snprintf(buffer, sizeof(buffer), "%ws", value);
				else
					buffer[0] = '\0';
				MK12HOOKSDK::ImGui_InputText(label, buffer, sizeof(buffer));
			};

			DisplayWCharField("Platform", HookMetadata::UserProfileInfo.Platform);
			DisplayWCharField("PlatformId", HookMetadata::UserProfileInfo.PlatformId);
			DisplayWCharField("SaveKey", HookMetadata::UserProfileInfo.SaveKey);

			// The game's online identity, read by the profile getter (or the Floyd hash)
			if (Features::IsInstalled(Features::Id::ProfileGetter) || Features::IsInstalled(Features::Id::FloydTracking))
			{
				DisplayWCharField("HydraId", HookMetadata::UserProfileInfo.HydraId);
				DisplayWCharField("WBId", HookMetadata::UserProfileInfo.WBId);
			}
			else
			{
				MK12HOOKSDK::ImGui_Text("HydraId and WBId need Profile Getter or Floyd Tracking");
			}
		}
	}
	else
	{
		MK12HOOKSDK::ImGui_Text("Profile Info Extraction is not enabled");
	}

	if (Features::IsInstalled(Features::Id::Popups))
	{
		if (MK12HOOKSDK::ImGui_CollapsingHeader("Game Message Box"))
		{
			static char Message[512] = "";
			static bool bPausesGame = false;
			static GameMessageBox::Handle LastNotice = GameMessageBox::NoHandle;
			MK12HOOKSDK::ImGui_InputText("Message", Message, sizeof(Message));
			MK12HOOKSDK::ImGui_Checkbox("Pauses Game", &bPausesGame);
			if (MK12HOOKSDK::ImGui_Button("Show Message Box") && Message[0])
				GameMessageBox::Confirm(GameMessageBox::Text::Plain(Message), nullptr, L"MK12TTH", bPausesGame);
			if (MK12HOOKSDK::ImGui_Button("Show Notice") && Message[0])
			{
				GameMessageBox::Dismiss(LastNotice);
				LastNotice = GameMessageBox::Notice(GameMessageBox::Text::Plain(Message), L"MK12TTH", bPausesGame);
			}
			if (MK12HOOKSDK::ImGui_Button("Show Toast") && Message[0])
				GameToast::Show(L"MK12TTH", GameMessageBox::Text::Plain(Message));
			if (MK12HOOKSDK::ImGui_Button("Dismiss Notice"))
			{
				GameMessageBox::Dismiss(LastNotice);
				LastNotice = GameMessageBox::NoHandle;
			}
			MK12HOOKSDK::ImGui_Text("Waits for loading screens and online modes to end");
		}
	}
	else
	{
		MK12HOOKSDK::ImGui_Text("Game Message Box is not enabled");
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
		printfInfo("Initializing MK12TTH");
		OnInitializeHook();
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		MK12HookPlugin::OnShutdown();
		break;
	}
	return true;
}