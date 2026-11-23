#include "mkutils.h"
#include "PauseMenu.h"
#include "ModSettings.h"
#include "GameState.h"
#include "eSettingsManager.h"
#include "mk12.h"
#include <cstring>
#include <cwchar>

namespace PauseMenu {
	// MOD SETTINGS has a panel of its own: a new object of the class KONTROLS' panel is made of (the game's settings
	// panel for the pause menu), made the way the game makes KONTROLS' (in the transient package, enabled) and set up by
	// the game like every entry's panel. Every pause menu gets it, with or without KONTROLS
	constexpr const wchar_t*	PANEL_CLASS			= L"/Game/Disk/Shared/UI/UISettings/PauseGenericSettings.PauseGenericSettings_C";
	constexpr uint32_t			PANEL_ENABLED		= 0x40;		// The panel sets up only while enabled
	constexpr uint32_t			PANEL_VOICE_CHAT	= 0x41;
	constexpr uint32_t			PANEL_KEEP_SCREEN	= 0x42;
	constexpr int				SLOT_GET_TAB		= 0x280 / 8;	// Panel vtable: (self, out tab) its label and the action that goes into it
	constexpr int				SLOT_CLONE			= 8;		// Pick action vtable: (self, destination callback)
	constexpr uint32_t			OBJECT_OUTER		= 0x20;		// The settings screen a panel sets up is made with the panel as its outer
	constexpr int32_t			KontrolsOrder		= 49;		// KONTROLS' place in the pause menu, ours sits right after it (first without it)
	static void* const			ANY_PACKAGE			= (void*)-1;

	typedef void(__fastcall CloneType)(void* Self, MK12::FMenuEntryCallback* Destination);
	typedef MK12::FSettingsTab*(__fastcall GetTabType)(void* Self, MK12::FSettingsTab* Out);

	static void*	PanelClass		= nullptr;	// Loaded once the main menu is up, kept loaded
	static bool		bClassTried		= false;
	static void*	PauseScreen		= nullptr;	// The pause menu our entry went into, until it closes
	static void*	OurPanel		= nullptr;	// Our entry's panel object

	static void Forget()
	{
		PauseScreen = nullptr;
		OurPanel = nullptr;
	}

	static bool IsReady()
	{
		return PanelClass && MK12::StaticConstructObject && MK12::TransientPackage && MK12::GrowPauseEntries && MK12::GameAlloc
			&& MK12::PanelHolderConstruct && MK12::MenuEntryConstruct && MK12::SettingsTabConstruct && MK12::SettingsTabDestruct;
	}

	static void LoadPanelClass()
	{ // Not every fight loads it: loaded once at the main menu, never while a pause menu opens
		if (bClassTried || !GameState::IsMainMenuUp() || !MK12::StaticFindObject || !MK12::StaticLoadObject)
			return;
		bClassTried = true;
		void* AnyClass = MK12::StaticFindObject(nullptr, ANY_PACKAGE, L"/Script/CoreUObject.Object", false);
		void* Class = AnyClass ? MK12::StaticLoadObject(AnyClass, nullptr, PANEL_CLASS, nullptr, 0, nullptr, true, nullptr) : nullptr;
		if (Class && MK12::KeepLoaded(Class))
			PanelClass = Class;
		else
			printfWarning("Pause Menu: No MOD SETTINGS in the pause menu (its panel didn't load)");
	}

	static int32_t FindKontrols(MK12::TArray<MK12::FPauseEntry>* Entries)
	{
		for (int32_t i = 0; i < Entries->ElementsCount; i++)
		{
			MK12::TArray<MK12::FMenuEntry>& Labels = Entries->Data[i].Tab.Entries;
			if (Labels.ElementsCount == 1 && Labels.Data[0].Key.Data && !wcscmp(Labels.Data[0].Key.Data, AfterKey) && Entries->Data[i].PanelHolder)
				return i;
		}
		return -1;
	}

	static void* MakePanel()
	{
		MK12::FConstructObjectParams Params = {};
		Params.Class = PanelClass;
		Params.Outer = MK12::TransientPackage();
		char* Panel = (char*)MK12::StaticConstructObject(&Params);
		if (!Panel)
			return nullptr;
		Panel[PANEL_ENABLED] = 1;
		Panel[PANEL_VOICE_CHAT] = 0; // Its pages as KONTROLS', not voice chat's
		Panel[PANEL_KEEP_SCREEN] = 0;
		return Panel;
	}

	static bool MakeLabel(void* Panel, MK12::FMenuEntry& Label)
	{ // The panel's own tab gives the action that goes into it (and KONTROLS' look): ours is a copy with our key
		MK12::FSettingsTab Tab = {};
		((GetTabType*)(*(void***)Panel)[SLOT_GET_TAB])(Panel, &Tab);
		bool bMade = false;
		if (Tab.Entries.ElementsCount == 1 && Tab.Entries.Data[0].OnPick)
		{
			MK12::FMenuEntry& Source = Tab.Entries.Data[0];
			MK12::FMenuEntryCallback OnPick;
			((CloneType*)(*(void***)Source.OnPick)[SLOT_CLONE])(Source.OnPick, &OnPick);
			if (OnPick.Instance)
			{
				MK12::MenuEntryConstruct(&Label, EntryKey, &OnPick);
				Label.bLocalized = false; // Shows the key as the label
				Label.FlagsA = Source.FlagsA;
				Label.FlagsB = Source.FlagsB;
				bMade = true;
			}
		}
		MK12::SettingsTabDestruct(&Tab);
		return bMade;
	}

	void OnEntriesCollected(MK12::TArray<MK12::FPauseEntry>* Entries)
	{
		Forget();
		if (!Entries || !IsReady() || GameState::IsOnline() || !ModSettings::HasInGameRows())
			return;
		void* Screen = (char*)Entries - MK12::PauseEntriesOffset;
		void* Panel = MakePanel();
		MK12::FMenuEntry Label = {};
		if (!Panel || !MakeLabel(Panel, Label))
			return;

		// The menu keeps the panel alive through a holder of its own, deleted with the entry
		void* Memory = MK12::GameAlloc(MK12::PanelHolderSize);
		void* Holder = Memory ? MK12::PanelHolderConstruct(Memory, Panel) : nullptr;
		if (!Holder)
			return;

		// Entries are moved bit for bit, the game does the same when it fills the list
		MK12::FPauseEntry Ours = {};
		MK12::SettingsTabConstruct(&Ours.Tab, &Label, KontrolsOrder);
		Ours.PanelHolder = Holder;
		int32_t Kontrols = FindKontrols(Entries);
		int32_t Old = Entries->ElementsCount++;
		if (Entries->ElementsCount > Entries->Size)
			MK12::GrowPauseEntries(Entries, Old);
		int32_t At = Kontrols + 1; // First without KONTROLS
		memmove(&Entries->Data[At + 1], &Entries->Data[At], (size_t)(Old - At) * sizeof(MK12::FPauseEntry));
		memcpy(&Entries->Data[At], &Ours, sizeof(Ours));
		PauseScreen = Screen;
		OurPanel = Panel;
		if (SettingsMgr->ShouldLog(Log::Debug))
			printf("Pause Menu: MOD SETTINGS added %s\n", Kontrols >= 0 ? "after KONTROLS" : "first");
	}

	void OnPanelSetUp(MK12::USettingsScreen* Screen, void* Player, int64_t Mode)
	{ // Any pause panel filling its pages: ours gets our rows, the others stay the game's
		if (!Screen || !OurPanel || *(void**)((char*)Screen + OBJECT_OUTER) != OurPanel)
			return;
		ModSettings::OnPausePanelSetUp(Screen);
		if (SettingsMgr->ShouldLog(Log::Debug))
			printf("Pause Menu: MOD SETTINGS panel set up\n");
	}

	void OnPauseClosed(void* Pause)
	{
		if (!Pause || Pause != PauseScreen)
			return;
		if (SettingsMgr->ShouldLog(Log::Debug))
			printf("Pause Menu: the pause menu with our entry closed\n");
		Forget();
	}

	void Tick()
	{
		LoadPanelClass();
		// The pause menu goes away with the fight
		if (PauseScreen && (GameState::IsLoading() || GameState::IsMainMenuUp()))
			Forget();
	}
}
