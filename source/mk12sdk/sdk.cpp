#include "SDK.h"
#include <Windows.h>
#include <iostream>

bool MK12HOOKSDK::ms_bIsInitialized = false;
bool (*MK12HOOKSDK::IsMenuActive)(void) = nullptr;
MKCharacter* (*MK12HOOKSDK::GetCharacterObject)(PLAYER_NUM) = nullptr;
PlayerInfo* (*MK12HOOKSDK::GetPlayerInfo)(PLAYER_NUM) = nullptr;
FGGameInfo* (*MK12HOOKSDK::GetGameInfo)(void) = nullptr;

bool (*MK12HOOKSDK::ImGui_Checkbox)(const char*, bool*) = nullptr;
void (*MK12HOOKSDK::ImGui_Text)(const char*) = nullptr;
bool (*MK12HOOKSDK::ImGui_Button)(const char*) = nullptr;
bool (*MK12HOOKSDK::ImGui_InputInt)(const char*, int*) = nullptr;
bool (*MK12HOOKSDK::ImGui_InputFloat)(const char*, float*) = nullptr;
bool (*MK12HOOKSDK::ImGui_InputText)(const char*, char*, size_t) = nullptr;
bool (*MK12HOOKSDK::ImGui_BeginCombo)(const char*, const char*) = nullptr;
void (*MK12HOOKSDK::ImGui_EndCombo)() = nullptr;
bool (*MK12HOOKSDK::ImGui_Selectable)(const char*, bool) = nullptr;
void (*MK12HOOKSDK::ImGui_SetItemDefaultFocus)() = nullptr;
void (*MK12HOOKSDK::ImGui_Separator)() = nullptr;
bool (*MK12HOOKSDK::ImGui_CollapsingHeader)(const char*) = nullptr;
bool (*MK12HOOKSDK::ImGui_ColorEdit4)(const char*, float*) = nullptr;
uintptr_t(*MK12HOOKSDK::GetPattern)(const char*, int) = nullptr;
int (*MK12HOOKSDK::CreateHook)(LPVOID, LPVOID, LPVOID*) = nullptr;
void (*MK12HOOKSDK::PushNotif)(int, const char*) = nullptr;
const char* (*MK12HOOKSDK::GetVersion)() = nullptr;

void MK12HOOKSDK::Initialize()
{
	// This function is only called by MK1Hook's EHP Loader

	if (ms_bIsInitialized) // Already initialized from before somehow
		return;

	HMODULE hook = GetModuleHandleW(L"mk1hook.asi");
	if (!hook)
	{
		ms_bIsInitialized = false;
		return;
	}

	IsMenuActive = (bool(*)())GetProcAddress(hook, "MK12HOOK_GetMenuActive");
	if (!IsMenuActive)
	{
		ms_bIsInitialized = false;
		return;
	}

	GetCharacterObject = (MKCharacter * (*)(PLAYER_NUM))GetProcAddress(hook, "MK12HOOK_GetCharacterObject");
	if (!GetCharacterObject)
	{
		ms_bIsInitialized = false;
		return;
	}

	GetPlayerInfo = (PlayerInfo * (*)(PLAYER_NUM))GetProcAddress(hook, "MK12HOOK_GetPlayerInfo");
	if (!GetPlayerInfo)
	{
		ms_bIsInitialized = false;
		return;
	}

	GetGameInfo = (FGGameInfo * (*)())GetProcAddress(hook, "MK12HOOK_GetGameInfo");
	if (!GetGameInfo)
	{
		ms_bIsInitialized = false;
		return;
	}

	ImGui_Checkbox = (bool(*)(const char*, bool*))GetProcAddress(hook, "MK12HOOK_ImGui_Checkbox");
	if (!ImGui_Checkbox)
	{
		ms_bIsInitialized = false;
		return;
	}

	ImGui_Text = (void(*)(const char*))GetProcAddress(hook, "MK12HOOK_ImGui_Text");
	if (!ImGui_Text)
	{
		ms_bIsInitialized = false;
		return;
	}

	ImGui_Button = (bool(*)(const char*))GetProcAddress(hook, "MK12HOOK_ImGui_Button");
	if (!ImGui_Button)
	{
		ms_bIsInitialized = false;
		return;
	}


	ImGui_InputInt = (bool(*)(const char*, int*))GetProcAddress(hook, "MK12HOOK_ImGui_InputInt");
	if (!ImGui_InputInt)
	{
		ms_bIsInitialized = false;
		return;
	}

	ImGui_InputFloat = (bool(*)(const char*, float*))GetProcAddress(hook, "MK12HOOK_ImGui_InputFloat");
	if (!ImGui_InputFloat)
	{
		ms_bIsInitialized = false;
		return;
	}

	ImGui_InputText = (bool(*)(const char*, char*, size_t))GetProcAddress(hook, "MK12HOOK_ImGui_InputText");
	if (!ImGui_InputText)
	{
		ms_bIsInitialized = false;
		return;
	}

	ImGui_BeginCombo = (bool(*)(const char*, const char*))GetProcAddress(hook, "MK12HOOK_ImGui_BeginCombo");
	if (!ImGui_BeginCombo)
	{
		ms_bIsInitialized = false;
		return;
	}

	ImGui_EndCombo = (void(*)())GetProcAddress(hook, "MK12HOOK_ImGui_EndCombo");
	if (!ImGui_EndCombo)
	{
		ms_bIsInitialized = false;
		return;
	}

	ImGui_Selectable = (bool(*)(const char*, bool))GetProcAddress(hook, "MK12HOOK_ImGui_Selectable");
	if (!ImGui_Selectable)
	{
		ms_bIsInitialized = false;
		return;
	}

	ImGui_SetItemDefaultFocus = (void(*)())GetProcAddress(hook, "MK12HOOK_ImGui_SetItemDefaultFocus");
	if (!ImGui_SetItemDefaultFocus)
	{
		ms_bIsInitialized = false;
		return;
	}

	ImGui_Separator = (void(*)())GetProcAddress(hook, "MK12HOOK_ImGui_Separator");
	if (!ImGui_Separator)
	{
		ms_bIsInitialized = false;
		return;
	}

	ImGui_CollapsingHeader = (bool(*)(const char*))GetProcAddress(hook, "MK12HOOK_ImGui_CollapsingHeader");
	if (!ImGui_CollapsingHeader)
	{
		ms_bIsInitialized = false;
		return;
	}

	ImGui_ColorEdit4 = (bool(*)(const char*, float*))GetProcAddress(hook, "MK12HOOK_ImGui_ColorEdit4");
	if (!ImGui_ColorEdit4)
	{
		ms_bIsInitialized = false;
		return;
	}

	GetPattern = (uintptr_t(*)(const char*, int))GetProcAddress(hook, "MK12HOOK_GetPattern");
	if (!GetPattern)
	{
		ms_bIsInitialized = false;
		return;
	}

	CreateHook = (int(*)(LPVOID, LPVOID, LPVOID*))GetProcAddress(hook, "MK12HOOK_CreateHook");
	if (!CreateHook)
	{
		ms_bIsInitialized = false;
		return;
	}

	PushNotif = (void(*)(int, const char*))GetProcAddress(hook, "MK12HOOK_PushNotif");
	if (!PushNotif)
	{
		ms_bIsInitialized = false;
		return;
	}

	GetVersion = (const char* (*)())GetProcAddress(hook, "MK12HOOK_GetVersion");
	if (!GetVersion)
	{
		ms_bIsInitialized = false;
		return;
	}

	ms_bIsInitialized = true;
}

bool MK12HOOKSDK::IsOK()
{
	return ms_bIsInitialized;
}