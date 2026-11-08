#pragma once
#include <cstdint>
#include <string>

namespace MK12 { class USettingsScreen; struct FSettingsTab; class FText; class FString; struct FPlayerInput; }

// Our own tab in the game's Settings screen (main menu only). Each row reads and writes
// its ini key, nothing goes into the game's save. Rows that need a restart offer one
namespace ModSettings {
	constexpr const wchar_t* TabKey = L"MK12TTH"; // Tab label
	constexpr int32_t TabOrder = 90; // After the game's last tab (Other is 80)

	void Init(bool bWithMods); // Reads the current ini values, call once at startup. bWithMods adds the MODS page (pak mods)
	void SetOwnMenu(bool bOpen); // Set while the settings screen opened from our main menu entry is up: only our pages show
	void SetModVersion(const char* Version); // Shown at the bottom left of our menu
	const std::wstring& ModVersion();
	MK12::FString* VersionText(MK12::FString* GameVersion); // Ours in our menu, the game's elsewhere
	bool IsReady(); // The rows were built from the game's own rows
	bool IsAvailable(); // Init ran, the hooks are in
	bool HasOwnMenu(); // Our main menu entry opens MODS: pak mods, live features, logging and profile

	// Called by the screen hooks, game thread only
	void OnPagesBuilt(MK12::USettingsScreen* Screen); // Adds our page after the game's pages
	void CopyTabs(void* Out, MK12::FSettingsTab* Tabs, int32_t Count, int32_t ExtraA, int32_t ExtraB); // Adds our tab after the game's tabs
	bool FindText(MK12::FText* Out, const wchar_t* Table, const wchar_t* Key); // Fills Out for our titles and descriptions, false for the game's own

	// The settings screen opened and closed (called around its run, which lasts until the player backs out).
	// The restart question waits until it's closed, so it asks once for every change made there
	void OnScreenOpened();
	void OnScreenClosed();
	void OnInputUpdate(MK12::FPlayerInput* Input); // Enter on one of our pages asks right away

	// Game thread only, called every frame by the hook
	void Tick();
}
