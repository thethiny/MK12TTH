#pragma once
#include "mk12.h"

// A menu entry's pick action is a small game object whose vtable has 11 slots (8 clone, 9 run, 10 run when picked).
// Ours are copies of a game action's vtable whose runs call our code around the game's own: the entry then opens
// exactly what the game's would, with our switches set for the visit. One copy per kind of game action and wrap
namespace MenuActions {
	typedef void (Callback)();
	struct Wrap {
		Callback*	Before;		// Before the game's own run, or null
		Callback*	After;		// After it (the run waits until the player backs out of what it opened), or null
	};

	// Our vtable for this game action with this wrap (Wrap must live as long as the game: a static). Null: no room left
	void** VTable(void* Action, const Wrap* With);
	// A copy of the game action (a menu entry's OnPick) that runs through our vtable. The entry it goes to takes it over
	// and frees it. False: no copy made
	bool Clone(void* Action, const Wrap* With, MK12::FMenuEntryCallback& Out);
	bool IsOurs(void* Action); // It runs through one of our vtables
}
