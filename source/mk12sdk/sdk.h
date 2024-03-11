#pragma once

typedef __int64 MKCharacter;
typedef __int64 PlayerInfo;
typedef __int64 FGGameInfo;

enum PLAYER_NUM
{
	INVALID_PLAYER_NUM			= 0xFFFFFFFF,
	PLAYER1						= 0x0,
	PLAYER2,
	PLAYER3,
	PLAYER4,
	MAX_PLAYERS,
	CPU_PLAYER					= 0x5,
	NOT_CPU_PLAYER				= 0x6,
	BACKGROUND_PLAYER			= 0x7,
};

#include <Windows.h>

class MK12HOOKSDK {
public:
	static MKCharacter* (*GetCharacterObject)(PLAYER_NUM);
	static PlayerInfo* (*GetPlayerInfo)(PLAYER_NUM);
	static FGGameInfo* (*GetGameInfo)(void);
	static bool (*IsMenuActive)(void);
	static bool (*ImGui_Checkbox)(const char*, bool*);
	static void (*ImGui_Text)(const char*);
	static bool (*ImGui_Button)(const char*);
	static bool (*ImGui_InputInt)(const char*, int*);
	static bool (*ImGui_InputFloat)(const char*, float*);
	static bool (*ImGui_InputText)(const char*, char*, size_t);
	static bool (*ImGui_BeginCombo)(const char*, const char*);
	static void (*ImGui_EndCombo)();
	static bool (*ImGui_Selectable)(const char*, bool);
	static void (*ImGui_SetItemDefaultFocus)();
	static void (*ImGui_Separator)();
	static bool (*ImGui_CollapsingHeader)(const char*);
	static bool (*ImGui_ColorEdit4)(const char*, float*);
	static uintptr_t(*GetPattern)(const char*, int);
	static int (*CreateHook)(LPVOID, LPVOID, LPVOID*);
	static void (*PushNotif)(int, const char*);
	static const char* (*GetVersion)();

	static bool ms_bIsInitialized;
	static void Initialize();
	static bool IsOK();
};

#define PLUGIN_API __declspec(dllexport)

namespace MK12HookPlugin {
	// Plugin name to use when loading and printing errors to log
	extern "C" PLUGIN_API const char* GetPluginName();

	// Hook project name that this plugin is compatible with
	extern "C" PLUGIN_API const char* GetPluginProject();

	// GUI tab name that will be used in the Plugins section
	extern "C" PLUGIN_API const char* GetPluginTabName();

	// Initialization
	extern "C" PLUGIN_API void OnInitialize();

	// Shutdown
	extern "C" PLUGIN_API void OnShutdown();

	// Called every game tick
	extern "C" PLUGIN_API void OnFrameTick();

	// Called on match/fight start
	extern "C" PLUGIN_API void OnFightStartup();

	// Tab data for menu, remove this if you don't want a plugin tab
	extern "C" PLUGIN_API void TabFunction();

}
