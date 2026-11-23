#include "mkutils.h"
#include "PauseMenu.h"
#include "ModSettings.h"
#include "GameState.h"
#include "eSettingsManager.h"
#include "mk12.h"
#include <cstring>
#include <cwchar>

namespace PauseMenu {
	// The pause menu sets up the KONTROLS panel once as it opens and then only shows it for the highlighted entry.
	// MOD SETTINGS shares that panel, so each frame the highlighted entry decides whose rows it holds
	constexpr int32_t KontrolsOrder = 49; // KONTROLS' place in the pause menu, ours sits right after it
	constexpr int SLOT_CLONE = 8; // Pick action vtable: (self, destination callback)

	typedef void(__fastcall CloneType)(void* Self, MK12::FMenuEntryCallback* Destination);

	static void* PauseScreen = nullptr; // The pause menu our entry went into, until the fight ends
	static int32_t OurIndex = -1; // Our entry's place in its list

	// The panel and what the game set it up with, to set it up again for KONTROLS
	static MK12::USettingsScreen* Panel = nullptr;
	static uint64_t PanelPlayer[2] = {};
	static int64_t PanelMode = 0;
	static bool bOursShown = false;

	static void Forget()
	{
		PauseScreen = nullptr;
		OurIndex = -1;
		Panel = nullptr;
		bOursShown = false;
	}

	static bool IsReady()
	{
		return MK12::GrowPauseEntries && MK12::GameAlloc && MK12::PanelHolderConstruct && MK12::MenuEntryConstruct
			&& MK12::SettingsTabConstruct && MK12::PausePanelPageSetup && MK12::PauseSelectedIndex;
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

	void OnEntriesCollected(MK12::TArray<MK12::FPauseEntry>* Entries)
	{
		Forget();
		if (!Entries || !IsReady() || GameState::IsOnline() || !ModSettings::HasInGameRows())
			return;
		int32_t Kontrols = FindKontrols(Entries);
		if (Kontrols < 0)
			return;
		MK12::FPauseEntry& Source = Entries->Data[Kontrols];
		MK12::FMenuEntry& SourceLabel = Source.Tab.Entries.Data[0];
		void* Panel = *(void**)((char*)Source.PanelHolder + MK12::PanelHolderObjectOffset);
		if (!Panel || !SourceLabel.OnPick)
			return;

		// A copy of KONTROLS' action: picking ours goes into the panel the same way. The entry takes it over and frees it
		MK12::FMenuEntryCallback OnPick;
		((CloneType*)(*(void***)SourceLabel.OnPick)[SLOT_CLONE])(SourceLabel.OnPick, &OnPick);
		if (!OnPick.Instance)
			return;

		MK12::FMenuEntry Label = {};
		MK12::MenuEntryConstruct(&Label, EntryKey, &OnPick);
		Label.bLocalized = false; // Shows the key as the label
		Label.FlagsA = SourceLabel.FlagsA;
		Label.FlagsB = SourceLabel.FlagsB;

		// The menu keeps the panel alive through a holder of its own, deleted with the entry
		void* Memory = MK12::GameAlloc(MK12::PanelHolderSize);
		void* Holder = Memory ? MK12::PanelHolderConstruct(Memory, Panel) : nullptr;
		if (!Holder)
			return;

		// Entries are moved bit for bit, the game does the same when it fills the list
		MK12::FPauseEntry Ours = {};
		MK12::SettingsTabConstruct(&Ours.Tab, &Label, KontrolsOrder);
		Ours.PanelHolder = Holder;
		int32_t Old = Entries->ElementsCount++;
		if (Entries->ElementsCount > Entries->Size)
			MK12::GrowPauseEntries(Entries, Old);
		int32_t At = Kontrols + 1;
		memmove(&Entries->Data[At + 1], &Entries->Data[At], (size_t)(Old - At) * sizeof(MK12::FPauseEntry));
		memcpy(&Entries->Data[At], &Ours, sizeof(Ours));
		PauseScreen = (char*)Entries - MK12::PauseEntriesOffset;
		OurIndex = At;
		if (SettingsMgr->ShouldLog(Log::Debug))
			printf("Pause Menu: MOD SETTINGS added after KONTROLS\n");
	}

	void OnPanelSetUp(MK12::USettingsScreen* Screen, void* Player, int64_t Mode)
	{
		Panel = Screen;
		if (Player)
			memcpy(PanelPlayer, Player, sizeof(PanelPlayer));
		PanelMode = Mode;
		bOursShown = false;
		Tick();
	}

	void OnPauseClosed(void* Pause)
	{
		if (Pause && Pause == PauseScreen)
			Forget();
	}

	void Tick()
	{
		// The pause menu goes away with the fight
		if (GameState::IsLoading() || GameState::IsMainMenuUp())
		{
			if (PauseScreen)
				Forget();
			return;
		}
		if (!PauseScreen || !Panel || OurIndex < 0)
			return;
		bool bOurs = MK12::PauseSelectedIndex((char*)PauseScreen + MK12::PauseMenuListOffset, 0) == OurIndex && !GameState::IsOnline();
		if (bOurs == bOursShown)
			return;
		bOursShown = bOurs;
		if (SettingsMgr->ShouldLog(Log::Debug))
			printf("Pause Menu: Panel shows %s\n", bOurs ? "MOD SETTINGS" : "KONTROLS");
		if (bOurs)
			ModSettings::OnPausePanelSetUp(Panel);
		else
			MK12::PausePanelPageSetup(Panel, PanelPlayer, PanelMode);
	}
}
