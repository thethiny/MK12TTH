#include "mkutils.h"
#include "GameToast.h"
#include "GameState.h"
#include "mk12.h"
#include "eSettingsManager.h"
#include <cstring>
#include <map>

namespace GameToast {
	constexpr uint8_t TOAST_PRIORITY = 2; // Same as the game's social toasts

	struct Request {
		GameMessageBox::Text	Title;
		GameMessageBox::Text	Subtitle;
		AcceptCallback*			OnAccept;
	};

	// Queue and Accepted are shared with other threads
	static SRWLOCK								QueueLock	= SRWLOCK_INIT;
	static std::vector<Request>					Queue;
	static std::vector<uint64_t>				Accepted;
	static std::map<uint64_t, AcceptCallback*>	Actions;	// Waiting for their toast's ACCEPT, by id
	static uint64_t								NextId		= 1;

	// The game copies, runs and frees this delegate with its own allocator. Layout: vtable, handle at +0x10, then ours
	struct FToastDelegate {
		void**		VTable;
		uint64_t	Unused;
		uint64_t	Handle;
		uint64_t	Id;
	};
	constexpr int32_t DELEGATE_SLOTS = sizeof(FToastDelegate) / 16; // Its size the way the game counts it

	void Accept(uint64_t Id)
	{ // ACCEPT comes from the game's toast fiber, the action runs in Tick
		AcquireSRWLockExclusive(&QueueLock);
		Accepted.push_back(Id);
		ReleaseSRWLockExclusive(&QueueLock);
	}

	void __fastcall DelegateDestroy(FToastDelegate*, int32_t) {} // The game frees the memory
	void* __fastcall DelegateNull(FToastDelegate*) { return nullptr; }
	bool __fastcall DelegateFalse(FToastDelegate*) { return false; }
	bool __fastcall DelegateTrue(FToastDelegate*) { return true; }
	uint64_t* __fastcall DelegateHandle(FToastDelegate* Self, uint64_t* Out) { *Out = Self->Handle; return Out; }
	void __fastcall DelegateExecute(FToastDelegate* Self) { Accept(Self->Id); }
	bool __fastcall DelegateExecuteIfSafe(FToastDelegate* Self) { Accept(Self->Id); return true; }

	void __fastcall DelegateCopy(FToastDelegate* Self, MK12::FMenuEntryCallback* Destination)
	{ // Same as the game's own copy: release what's there, size the block, fill it
		if (Destination->bAllocated && Destination->Instance)
			(*(void(__fastcall**)(void*, int32_t))*(void***)Destination->Instance)(Destination->Instance, 0);
		if (Destination->bAllocated != DELEGATE_SLOTS)
		{
			Destination->Instance = MK12::Realloc(Destination->Instance, sizeof(FToastDelegate), 0);
			Destination->bAllocated = DELEGATE_SLOTS;
		}
		memcpy(Destination->Instance, Self, sizeof(FToastDelegate));
	}

	static void* DelegateVTable[] = {
		(void*)DelegateDestroy, (void*)DelegateNull, (void*)DelegateNull, (void*)DelegateNull, (void*)DelegateFalse,
		(void*)DelegateFalse, (void*)DelegateTrue, (void*)DelegateHandle, (void*)DelegateCopy, (void*)DelegateExecute,
		(void*)DelegateExecuteIfSafe };

	MK12::FMenuEntryCallback MakeDelegate(uint64_t Id)
	{
		MK12::FMenuEntryCallback Callback;
		FToastDelegate* Delegate = (FToastDelegate*)MK12::Realloc(nullptr, sizeof(FToastDelegate), 0);
		if (!Delegate)
			return Callback;
		Delegate->VTable = DelegateVTable;
		Delegate->Unused = 0;
		Delegate->Handle = Id;
		Delegate->Id = Id;
		Callback.Instance = Delegate;
		Callback.bAllocated = DELEGATE_SLOTS;
		return Callback;
	}

	bool IsReady()
	{
		return MK12::ToastDriver && MK12::ToastEnqueue && MK12::TextFromString;
	}

	bool IsUp()
	{
		return IsReady() && GameState::CanShowUI() && MK12::ToastDriver();
	}

	void Show(const GameMessageBox::Text& Title, const GameMessageBox::Text& Subtitle, AcceptCallback* OnAccept)
	{
		if (!IsReady())
			return;
		Request Req = { Title, Subtitle, OnAccept };
		AcquireSRWLockExclusive(&QueueLock);
		Queue.push_back(Req);
		ReleaseSRWLockExclusive(&QueueLock);
	}

	void Tick()
	{
		if (!IsReady())
			return;
		std::vector<uint64_t> Pressed;
		AcquireSRWLockExclusive(&QueueLock);
		Pressed.swap(Accepted);
		ReleaseSRWLockExclusive(&QueueLock);
		for (size_t i = 0; i < Pressed.size(); i++)
		{
			std::map<uint64_t, AcceptCallback*>::iterator Action = Actions.find(Pressed[i]);
			if (Action == Actions.end())
				continue;
			AcceptCallback* Run = Action->second;
			Actions.erase(Action);
			Run();
		}
		if (!GameState::CanShowUI())
			return;

		void* Driver = MK12::ToastDriver(); // Null outside the main menu
		if (!Driver)
			return;

		std::vector<Request> Ready;
		AcquireSRWLockExclusive(&QueueLock);
		Ready.swap(Queue);
		ReleaseSRWLockExclusive(&QueueLock);
		for (size_t i = 0; i < Ready.size(); i++)
		{
			const Request& Req = Ready[i];
			// The game releases both texts
			MK12::FText Title = GameMessageBox::ToGameText(Req.Title);
			MK12::FText Subtitle = GameMessageBox::ToGameText(Req.Subtitle);
			MK12::FMenuEntryCallback OnAccept, NoCallbackB, NoCallbackC;
			bool bAction = Req.OnAccept && MK12::Realloc;
			if (bAction)
			{
				uint64_t Id = NextId++;
				Actions[Id] = Req.OnAccept;
				OnAccept = MakeDelegate(Id);
				if (SettingsMgr->ShouldLog(Log::Debug))
					printf("Game Toast: Shown with ACCEPT and DECLINE\n");
			}
			MK12::ToastEnqueue(Driver, &Title, &Subtitle, bAction, bAction ? ActionDuration : DefaultDuration, TOAST_PRIORITY,
				&OnAccept, &NoCallbackB, &NoCallbackC);
		}
	}
}
