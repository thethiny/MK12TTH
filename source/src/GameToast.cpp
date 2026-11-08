#include "mkutils.h"
#include "GameToast.h"
#include "GameState.h"
#include "mk12.h"
#include "eSettingsManager.h"
#include <cstring>
#include <deque>
#include <map>
#include <mutex>

namespace GameToast {
	constexpr uint8_t TOAST_PRIORITY = 2; // What the game's social toasts use

	struct Request {
		GameMessageBox::Text Title;
		GameMessageBox::Text Subtitle;
		std::function<void()> OnAccept;
	};

	static std::mutex QueueLock;
	static std::deque<Request> Queue;

	// Actions waiting for their toast's ACCEPT, by id. The press comes from the game's toast fiber, so it only notes the
	// id and Tick runs the action on the game thread
	static std::map<uint64_t, std::function<void()>> Actions;
	static std::deque<uint64_t> Accepted; // Guarded by QueueLock
	static uint64_t NextId = 1;

	// A game delegate that notes its id when run. The game copies, runs and frees it with its own allocator; this is the
	// layout it expects: vtable, handle at +0x10, then our payload
	struct FToastDelegate {
		void** VTable;
		uint64_t Unused;
		uint64_t Handle;
		uint64_t Id;
	};
	constexpr int32_t DELEGATE_SLOTS = sizeof(FToastDelegate) / 16; // The delegate's size, as the game counts it

	static void Accept(uint64_t Id)
	{
		std::lock_guard<std::mutex> Guard(QueueLock);
		Accepted.push_back(Id);
	}

	static void __fastcall DelegateDestroy(FToastDelegate*, int32_t) {} // The game frees the memory itself
	static void* __fastcall DelegateNull(FToastDelegate*) { return nullptr; }
	static bool __fastcall DelegateFalse(FToastDelegate*) { return false; }
	static bool __fastcall DelegateTrue(FToastDelegate*) { return true; }
	static uint64_t* __fastcall DelegateHandle(FToastDelegate* Self, uint64_t* Out) { *Out = Self->Handle; return Out; }
	static void __fastcall DelegateExecute(FToastDelegate* Self) { Accept(Self->Id); }
	static bool __fastcall DelegateExecuteIfSafe(FToastDelegate* Self) { Accept(Self->Id); return true; }

	// Copies into the game's delegate the way the game's own copy does: release what's there, size the block, fill it
	static void __fastcall DelegateCopy(FToastDelegate* Self, MK12::FMenuEntryCallback* Destination)
	{
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

	static MK12::FMenuEntryCallback MakeDelegate(uint64_t Id)
	{
		MK12::FMenuEntryCallback Callback;
		auto* Delegate = (FToastDelegate*)MK12::Realloc(nullptr, sizeof(FToastDelegate), 0);
		if (!Delegate)
			return Callback;
		*Delegate = { DelegateVTable, 0, Id, Id };
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

	void Show(const GameMessageBox::Text& Title, const GameMessageBox::Text& Subtitle, std::function<void()> OnAccept)
	{
		if (!IsReady())
			return;
		std::lock_guard<std::mutex> Guard(QueueLock);
		Queue.push_back({ Title, Subtitle, std::move(OnAccept) });
	}

	void Tick()
	{
		if (!IsReady())
			return;
		std::deque<uint64_t> Pressed;
		{
			std::lock_guard<std::mutex> Guard(QueueLock);
			Pressed.swap(Accepted);
		}
		for (uint64_t Id : Pressed)
		{
			auto Action = Actions.find(Id);
			if (Action == Actions.end())
				continue;
			std::function<void()> Run = std::move(Action->second);
			Actions.erase(Action);
			Run();
		}
		if (!GameState::CanShowUI())
			return;

		void* Driver = MK12::ToastDriver(); // Null outside the main menu
		if (!Driver)
			return;

		std::deque<Request> Ready;
		{
			std::lock_guard<std::mutex> Guard(QueueLock);
			Ready.swap(Queue);
		}
		for (const Request& Req : Ready)
		{
			// The texts are handed over: the game releases them
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
