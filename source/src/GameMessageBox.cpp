#include "mkutils.h"
#include "GameMessageBox.h"
#include "GameState.h"
#include "mk12.h"
#include <cstdio>

namespace GameMessageBox {
	constexpr char MESSAGE_BOX_STYLE = 2; // What nearly all of the game's boxes use
	static const wchar_t BUTTON_ID[] = L"MK12TTH"; // Tells our buttons' press checks apart from the screen's
	static const int32_t ButtonInputs[MaxButtons] = { MK12::MessageBoxInput::Confirm, MK12::MessageBoxInput::Alt, MK12::MessageBoxInput::Back };

	enum class Phase { Queued, Showing, Done };

	struct Move { Phase From; Phase To; };
	static const Move AllowedMoves[] = {
		{ Phase::Queued,	Phase::Showing },
		{ Phase::Queued,	Phase::Done },		// Dropped, or failed to open
		{ Phase::Showing,	Phase::Done },
	};

	struct Request {
		Text				Body;
		Text				Title;
		std::vector<Text>	Buttons;
		CloseCallback*		OnClose		= nullptr;
		AnswerCallback*		OnAnswer	= nullptr;
		PickCallback*		OnPick		= nullptr;
		Handle				Id			= NoHandle;
		bool				bPausesGame	= false;
		Phase				State		= Phase::Queued;
		MK12::UIMessageBox*	Box			= nullptr;
		void*				Player		= nullptr; // Whose presses pick its buttons
	};

	// A finished request's callback, run once the lock is released
	struct Callback {
		CloseCallback*		OnClose;
		AnswerCallback*		OnAnswer;
		PickCallback*		OnPick;
		int					Picked;
	};

	// Queue and DismissRequests are shared with other threads (QueueLock), the rest is the game thread's
	static SRWLOCK				QueueLock			= SRWLOCK_INIT;
	static std::vector<Request*> Queue;
	static std::vector<Handle>	DismissRequests;
	static Request*				Current				= nullptr;
	static volatile LONG		bCloseAllRequested	= 0;
	static volatile LONG		bDismissRequested	= 0;
	static volatile LONG		PendingCount		= 0;
	static volatile LONG64		LastHandle			= 0;
	static volatile bool		bTakingInput		= false;
	static uint32_t				KeptPresses			= 0; // Taken from the game for the box on screen

	static bool Advance(Request& Req, Phase To)
	{
		for (size_t i = 0; i < _countof(AllowedMoves); i++)
		{
			if (AllowedMoves[i].From == Req.State && AllowedMoves[i].To == To)
			{
				Req.State = To;
				return true;
			}
		}
		printfError("GameMessageBox: Refused move %d -> %d", (int)Req.State, (int)To);
		return false;
	}

	Text Text::Plain(const std::string& Utf8)
	{
		Text text;
		int Size = MultiByteToWideChar(CP_UTF8, 0, Utf8.c_str(), (int)Utf8.size(), nullptr, 0);
		text.Value.resize(Size);
		if (Size > 0)
			MultiByteToWideChar(CP_UTF8, 0, Utf8.c_str(), (int)Utf8.size(), &text.Value[0], Size);
		return text;
	}

	Text Text::Key(const wchar_t* Namespace, const wchar_t* Key)
	{
		Text text;
		text.Namespace = Namespace ? Namespace : L"";
		text.Value = Key ? Key : L"";
		return text;
	}

	namespace Labels {
		Text OK()		{ return Text::Key(L"MessageBox", L"OK"); }
		Text Yes()		{ return Text::Key(L"MessageBox", L"Yes"); }
		Text No()		{ return Text::Key(L"MessageBox", L"No"); }
		Text Cancel()	{ return Text::Key(L"MessageBox", L"CancelUC"); }
		Text Confirm()	{ return Text::Key(L"MessageBox", L"ConfirmUC"); }
		Text Back()		{ return Text::Key(L"MessageBox", L"Back"); }
	}

	bool IsReady()
	{
		return MK12::TextFromString && MK12::FindText && MK12::MessageBoxConstruct && MK12::MessageBoxAddButton
			&& MK12::MessageBoxShow && MK12::MessageBoxClose && MK12::MessageBoxDestruct;
	}

	static Handle Enqueue(Request* Req)
	{
		if (!IsReady())
		{
			delete Req;
			return NoHandle;
		}
		if (Req->Buttons.size() > MaxButtons)
		{
			printfError("GameMessageBox: Up to %d buttons, got %zu", MaxButtons, Req->Buttons.size());
			delete Req;
			return NoHandle;
		}

		Req->Id = (Handle)InterlockedIncrement64(&LastHandle);
		Handle Id = Req->Id;

		AcquireSRWLockExclusive(&QueueLock);
		Queue.push_back(Req);
		InterlockedIncrement(&PendingCount);
		ReleaseSRWLockExclusive(&QueueLock);
		return Id;
	}

	static Request* NewRequest(const Text& Body, const Text& Title, bool bPausesGame)
	{
		Request* Req = new Request;
		Req->Body = Body;
		Req->Title = Title;
		Req->bPausesGame = bPausesGame;
		return Req;
	}

	Handle Confirm(const Text& Body, CloseCallback* OnClose, const Text& Title, bool bPausesGame)
	{
		Request* Req = NewRequest(Body, Title, bPausesGame);
		Req->Buttons.push_back(Labels::OK());
		Req->OnClose = OnClose;
		return Enqueue(Req);
	}

	Handle Ask(const Text& Body, AnswerCallback* OnAnswer, const Text& Title, const Text& Yes, const Text& No, bool bPausesGame)
	{
		Request* Req = NewRequest(Body, Title, bPausesGame);
		Req->Buttons.push_back(Yes);
		Req->Buttons.push_back(No);
		Req->OnAnswer = OnAnswer;
		return Enqueue(Req);
	}

	Handle Choose(const Text& Body, const std::vector<Text>& Buttons, PickCallback* OnPick, const Text& Title, bool bPausesGame)
	{
		if (Buttons.empty())
		{
			printfError("GameMessageBox: Choose needs at least 1 button, use Notice for none");
			return NoHandle;
		}
		Request* Req = NewRequest(Body, Title, bPausesGame);
		Req->Buttons = Buttons;
		Req->OnPick = OnPick;
		return Enqueue(Req);
	}

	Handle Notice(const Text& Body, const Text& Title, bool bPausesGame)
	{
		return Enqueue(NewRequest(Body, Title, bPausesGame));
	}

	void Dismiss(Handle Box)
	{
		if (Box == NoHandle)
			return;
		AcquireSRWLockExclusive(&QueueLock);
		DismissRequests.push_back(Box);
		InterlockedExchange(&bDismissRequested, 1);
		ReleaseSRWLockExclusive(&QueueLock);
	}

	size_t Pending()
	{
		return (size_t)PendingCount;
	}

	bool IsTakingInput()
	{
		return bTakingInput;
	}

	bool IsOwnButton(const MK12::FString* Id)
	{
		return Id && Id->Data && !wcscmp(Id->Data, BUTTON_ID);
	}

	void OnInputUpdate(MK12::FPlayerInput* Input)
	{
		if (!bTakingInput || !Input || !Current)
			return;

		uint32_t NewPresses = Input->Down & ~Input->Previous;
		if (Input == Current->Player && Current->Box->InputDelay <= 0)
			KeptPresses |= NewPresses;
		Input->Previous |= Input->Down; // Nothing else sees a press this frame
	}

	bool TakePress(int32_t Input)
	{
		if (!(KeptPresses & (uint32_t)Input))
			return false;
		KeptPresses &= ~(uint32_t)Input;
		return true;
	}

	static void* MenuPlayer()
	{ // The first local player, the menus are his
		uint8_t* Info = MK12::GetLocalPlayerInfo ? (uint8_t*)MK12::GetLocalPlayerInfo(0) : nullptr;
		return Info ? *(void**)(Info + MK12::LocalPlayerInfoPlayerOffset) : nullptr;
	}

	void CloseAll()
	{
		InterlockedExchange(&bCloseAllRequested, 1);
	}

	MK12::FText ToGameText(const Text& text)
	{
		MK12::FText Out;
		if (!text.Namespace.empty())
		{
			MK12::FindText(&Out, text.Namespace.c_str(), text.Value.c_str());
			return Out;
		}

		int32_t Count = (int32_t)text.Value.size() + 1; // FString counts the terminator
		MK12::FString String = { (wchar_t*)text.Value.c_str(), Count, Count };
		MK12::TextFromString(&Out, &String); // Copies it
		return Out;
	}

	static int ButtonInput(int Index, int Count)
	{
		if (Index == 0)
			return ButtonInputs[0];
		if (Index == Count - 1)
			return ButtonInputs[MaxButtons - 1];
		return ButtonInputs[1];
	}

	static void Open(Request& Req)
	{
		Req.Box = new MK12::UIMessageBox;
		MK12::UIMessageBox* Box = Req.Box;

		MK12::FText Body = ToGameText(Req.Body);
		Req.Player = MenuPlayer();
		KeptPresses = 0;
		MK12::MessageBoxConstruct(Box, Req.Player, &Body, MESSAGE_BOX_STYLE);
		Body.Release();
		Box->bPausesGame = Req.bPausesGame;

		if (!Req.Title.IsEmpty() && MK12::MessageBoxSetTitle)
		{
			MK12::FText Title = ToGameText(Req.Title);
			MK12::MessageBoxSetTitle(Box, &Title);
			Title.Release();
		}

		int Count = (int)Req.Buttons.size();
		for (int i = 0; i < Count; i++)
		{
			MK12::FText Label = ToGameText(Req.Buttons[i]);
			MK12::FMessageBoxDelegate OnPick; // The game's own close, empty if not found
			if (MK12::MessageBoxCallback.IsReady())
				MK12::MessageBoxCallback.Fill(OnPick);
			int32_t IdCount = (int32_t)wcslen(BUTTON_ID) + 1;
			MK12::FString Id = { (wchar_t*)BUTTON_ID, IdCount, IdCount }; // The game copies it
			MK12::MessageBoxAddButton(Box, i, &Label, ButtonInput(i, Count), &OnPick, &Id);
			Label.Release();
		}

		MK12::MessageBoxShow(Box);
		if (SettingsMgr->ShouldLog(Log::Verbose))
			printf("GameMessageBox: Showing %ls\n", Req.Body.Value.c_str());
	}

	// Ends a request and adds its callback to Callbacks, they run after Tick is done with the queue
	static void DeleteRequest(Request* Req)
	{
		if (Req->Box)
		{
			MK12::MessageBoxDestruct(Req->Box);
			delete Req->Box;
		}
		delete Req;
	}

	static void Finish(Request* Req, int Picked, std::vector<Callback>& Callbacks)
	{
		if (!Req)
			return;
		if (!Advance(*Req, Phase::Done))
		{
			DeleteRequest(Req);
			return;
		}

		InterlockedDecrement(&PendingCount);
		if (Req->OnClose || Req->OnAnswer || Req->OnPick)
		{
			Callback Done = { Req->OnClose, Req->OnAnswer, Req->OnPick, Picked };
			Callbacks.push_back(Done);
		}
		DeleteRequest(Req);
	}

	static bool Contains(const std::vector<Handle>& Handles, Handle Id)
	{
		for (size_t i = 0; i < Handles.size(); i++)
		{
			if (Handles[i] == Id)
				return true;
		}
		return false;
	}

	void Tick()
	{
		if (!IsReady() || (!PendingCount && !bCloseAllRequested && !bDismissRequested))
			return;

		bool bSafe = GameState::CanShowUI();
		std::vector<Callback> Callbacks;

		if (InterlockedExchange(&bCloseAllRequested, 0))
		{
			std::vector<Request*> Dropped;
			AcquireSRWLockExclusive(&QueueLock);
			Dropped.swap(Queue);
			ReleaseSRWLockExclusive(&QueueLock);
			for (size_t i = 0; i < Dropped.size(); i++)
				Finish(Dropped[i], Closed, Callbacks);
			if (Current)
				MK12::MessageBoxClose(Current->Box, false, false);
			Finish(Current, Closed, Callbacks);
			Current = nullptr;
		}

		if (InterlockedExchange(&bDismissRequested, 0))
		{
			std::vector<Handle> Dismissed;
			std::vector<Request*> Dropped;
			AcquireSRWLockExclusive(&QueueLock);
			Dismissed.swap(DismissRequests);
			for (size_t i = 0; i < Queue.size();)
			{
				if (Contains(Dismissed, Queue[i]->Id))
				{
					Dropped.push_back(Queue[i]);
					Queue.erase(Queue.begin() + i);
				}
				else
					i++;
			}
			ReleaseSRWLockExclusive(&QueueLock);
			for (size_t i = 0; i < Dropped.size(); i++)
				Finish(Dropped[i], Closed, Callbacks);
			if (Current && Contains(Dismissed, Current->Id))
			{
				MK12::MessageBoxClose(Current->Box, false, false);
				Finish(Current, Closed, Callbacks);
				Current = nullptr;
			}
		}

		if (Current)
		{
			if (!bSafe)
			{ // Loading or online started while it was up
				MK12::MessageBoxClose(Current->Box, false, false);
				Finish(Current, Closed, Callbacks);
				Current = nullptr;
			}
			else if (Current->Box->IsClosed())
			{
				int Picked = Current->Box->PickedIndex;
				if (Picked < 0 || Picked >= (int)Current->Buttons.size())
					Picked = Closed;
				Finish(Current, Picked, Callbacks);
				Current = nullptr;
			}
		}

		if (!Current && bSafe)
		{
			Request* Next = nullptr;
			AcquireSRWLockExclusive(&QueueLock);
			if (!Queue.empty())
			{
				Next = Queue[0];
				Queue.erase(Queue.begin());
			}
			ReleaseSRWLockExclusive(&QueueLock);
			if (Next && Advance(*Next, Phase::Showing))
			{
				Open(*Next);
				Current = Next;
			}
			else if (Next)
				DeleteRequest(Next);
		}

		bTakingInput = Current && !Current->Buttons.empty();
		if (!bTakingInput)
			KeptPresses = 0;

		for (size_t i = 0; i < Callbacks.size(); i++)
		{
			Callback& Done = Callbacks[i];
			if (Done.OnClose)
				Done.OnClose();
			else if (Done.OnAnswer)
				Done.OnAnswer(Done.Picked == 0);
			else if (Done.OnPick)
				Done.OnPick(Done.Picked);
		}
	}
}
