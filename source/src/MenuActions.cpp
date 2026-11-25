#include "MenuActions.h"
#include <cstring>

namespace MenuActions {
	constexpr int VTABLE_SLOTS		= 11;
	constexpr int SLOT_CLONE		= 8;	// (self, destination callback)
	constexpr int SLOT_RUN			= 9;	// (self, context)
	constexpr int SLOT_RUN_PICKED	= 10;	// (self, context), what a pick calls
	constexpr int MAX_KINDS			= 32;	// Every main menu entry has its own kind of action

	typedef void (__fastcall CloneType)(void* Self, MK12::FMenuEntryCallback* Destination);
	typedef __int64 (__fastcall RunType)(void* Self, void* Context);

	struct Kind {
		void**		Game;							// The game action's vtable
		const Wrap*	With;
		void*		WithInfo[VTABLE_SLOTS + 1];		// Ours, with the slot before it (the type info)
		CloneType*	Clone;
		RunType*	Run;
		RunType*	RunPicked;
	};
	static Kind	Kinds[MAX_KINDS]; // Game thread only
	static int	KindCount = 0;

	void** Ours(Kind& Of)
	{
		return &Of.WithInfo[1];
	}

	Kind* KindOf(void* Self)
	{
		void** VTable = *(void***)Self;
		for (int i = 0; i < KindCount; i++)
		{
			if (Ours(Kinds[i]) == VTable)
				return &Kinds[i];
		}
		return nullptr;
	}

	void __fastcall OurClone(void* Self, MK12::FMenuEntryCallback* Destination)
	{
		Kind* Of = KindOf(Self);
		Of->Clone(Self, Destination); // Writes the game's vtable into the copy
		if (Destination && Destination->Instance)
			*(void***)Destination->Instance = Ours(*Of);
	}

	__int64 RunWrapped(Kind* Of, RunType* Run, void* Self, void* Context)
	{
		if (Of->With->Before)
			Of->With->Before();
		__int64 Result = Run(Self, Context);
		if (Of->With->After)
			Of->With->After();
		return Result;
	}

	__int64 __fastcall OurRun(void* Self, void* Context)
	{
		Kind* Of = KindOf(Self);
		return RunWrapped(Of, Of->Run, Self, Context);
	}

	__int64 __fastcall OurRunPicked(void* Self, void* Context)
	{
		Kind* Of = KindOf(Self);
		return RunWrapped(Of, Of->RunPicked, Self, Context);
	}

	bool IsOurs(void* Action)
	{
		return Action && KindOf(Action) != nullptr;
	}

	Kind* KindFor(void* Action, const Wrap* With)
	{
		if (!Action || !With)
			return nullptr;
		void** Game = *(void***)Action;
		for (int i = 0; i < KindCount; i++)
		{
			if ((Kinds[i].Game == Game || Ours(Kinds[i]) == Game) && Kinds[i].With == With)
				return &Kinds[i];
		}
		if (KindCount == MAX_KINDS)
			return nullptr;
		Kind& New = Kinds[KindCount++];
		New.Game = Game;
		New.With = With;
		memcpy(New.WithInfo, Game - 1, sizeof(New.WithInfo));
		New.Clone = (CloneType*)Game[SLOT_CLONE];
		New.Run = (RunType*)Game[SLOT_RUN];
		New.RunPicked = (RunType*)Game[SLOT_RUN_PICKED];
		Ours(New)[SLOT_CLONE] = (void*)OurClone;
		Ours(New)[SLOT_RUN] = (void*)OurRun;
		Ours(New)[SLOT_RUN_PICKED] = (void*)OurRunPicked;
		return &New;
	}

	void** VTable(void* Action, const Wrap* With)
	{
		Kind* Of = KindFor(Action, With);
		return Of ? Ours(*Of) : nullptr;
	}

	bool Clone(void* Action, const Wrap* With, MK12::FMenuEntryCallback& Out)
	{ // The game's clone of the action (whichever vtable it has now), then ours on the copy
		Kind* Of = KindFor(Action, With);
		if (!Of)
			return false;
		Out = MK12::FMenuEntryCallback();
		Of->Clone(Action, &Out);
		if (!Out.Instance)
			return false;
		*(void***)Out.Instance = Ours(*Of);
		return true;
	}
}
