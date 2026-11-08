#include "mkutils.h"
#include "GameMessageBox.h"
#include "GameState.h"
#include "mk12.h"
#include <atomic>
#include <cstdio>
#include <algorithm>
#include <deque>
#include <memory>
#include <mutex>

namespace GameMessageBox {
	constexpr char MESSAGE_BOX_STYLE = 2; // What almost every game message box uses
	static const wchar_t BUTTON_ID[] = L"MK12TTH"; // Tells our buttons' press checks apart from the screen's
	static const int32_t ButtonInputs[MaxButtons] = { MK12::MessageBoxInput::Confirm, MK12::MessageBoxInput::Alt, MK12::MessageBoxInput::Back };

	// A request's lifecycle. Only Advance changes it, and only along AllowedMoves
	enum class Phase { Queued, Showing, Done };

	struct Move { Phase From; Phase To; };
	static const Move AllowedMoves[] = {
		{ Phase::Queued,	Phase::Showing },	// Opened on screen
		{ Phase::Queued,	Phase::Done },		// Dropped before showing, or failed to open
		{ Phase::Showing,	Phase::Done },		// Picked, or closed
	};

	struct BoxDeleter {
		void operator()(MK12::UIMessageBox* Box) const
		{
			MK12::MessageBoxDestruct(Box);
			delete Box;
		}
	};

	struct Request {
		Text Body;
		Text Title;
		std::vector<Text> Buttons;
		std::function<void(int)> OnPick;
		Handle Id = NoHandle;
		bool bPausesGame = false;
		Phase State = Phase::Queued;
		std::unique_ptr<MK12::UIMessageBox, BoxDeleter> Box;
		void* Player = nullptr; // Whose presses pick its buttons
	};

	// Requests come from any thread, everything else is the game thread's (Tick)
	static std::mutex QueueLock;
	static std::deque<std::unique_ptr<Request>> Queue;
	static std::unique_ptr<Request> Current;
	static std::atomic<bool> bCloseAllRequested = false;
	static std::atomic<size_t> PendingCount = 0;
	static std::atomic<bool> bTakingInput = false;
	static std::atomic<Handle> NextHandle = 1;
	static std::vector<Handle> DismissRequests; // Guarded by QueueLock
	static std::atomic<bool> bDismissRequested = false;
	static uint32_t KeptPresses = 0; // Presses taken from the game for the box on screen, game thread only

	static bool Advance(Request& Req, Phase To)
	{
		for (const Move& move : AllowedMoves)
		{
			if (move.From == Req.State && move.To == To)
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
		MultiByteToWideChar(CP_UTF8, 0, Utf8.c_str(), (int)Utf8.size(), text.Value.data(), Size);
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

	static Handle Enqueue(const Text& Body, const std::vector<Text>& Buttons, std::function<void(int)> OnPick, const Text& Title, bool bPausesGame)
	{
		if (!IsReady())
			return NoHandle;
		if (Buttons.size() > MaxButtons)
		{
			printfError("GameMessageBox: Up to %d buttons, got %zu", MaxButtons, Buttons.size());
			return NoHandle;
		}

		auto Req = std::make_unique<Request>();
		Req->Body = Body;
		Req->Title = Title;
		Req->Buttons = Buttons;
		Req->OnPick = std::move(OnPick);
		Req->bPausesGame = bPausesGame;
		Req->Id = NextHandle++;
		Handle Id = Req->Id;

		std::lock_guard<std::mutex> Guard(QueueLock);
		Queue.push_back(std::move(Req));
		PendingCount++;
		return Id;
	}

	Handle Confirm(const Text& Body, std::function<void()> OnClose, const Text& Title, bool bPausesGame)
	{
		return Enqueue(Body, { Labels::OK() }, [OnClose](int) { if (OnClose) OnClose(); }, Title, bPausesGame);
	}

	Handle Ask(const Text& Body, std::function<void(bool)> OnAnswer, const Text& Title, const Text& Yes, const Text& No, bool bPausesGame)
	{
		return Enqueue(Body, { Yes, No }, [OnAnswer](int Picked) { if (OnAnswer) OnAnswer(Picked == 0); }, Title, bPausesGame);
	}

	Handle Choose(const Text& Body, const std::vector<Text>& Buttons, std::function<void(int)> OnPick, const Text& Title, bool bPausesGame)
	{
		if (Buttons.empty())
		{
			printfError("GameMessageBox: Choose needs at least 1 button, use Notice for none");
			return NoHandle;
		}
		return Enqueue(Body, Buttons, std::move(OnPick), Title, bPausesGame);
	}

	Handle Notice(const Text& Body, const Text& Title, bool bPausesGame)
	{
		return Enqueue(Body, {}, nullptr, Title, bPausesGame);
	}

	void Dismiss(Handle Box)
	{
		if (Box == NoHandle)
			return;
		std::lock_guard<std::mutex> Guard(QueueLock);
		DismissRequests.push_back(Box);
		bDismissRequested = true;
	}

	size_t Pending()
	{
		return PendingCount;
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
		Input->Previous |= Input->Down; // Taken: nothing else sees a press this frame
	}

	bool TakePress(int32_t Input)
	{
		if (!(KeptPresses & (uint32_t)Input))
			return false;
		KeptPresses &= ~(uint32_t)Input;
		return true;
	}

	// The first local player, the one the menus belong to
	static void* MenuPlayer()
	{
		uint8_t* Info = MK12::GetLocalPlayerInfo ? (uint8_t*)MK12::GetLocalPlayerInfo(0) : nullptr;
		return Info ? *(void**)(Info + MK12::LocalPlayerInfoPlayerOffset) : nullptr;
	}

	void CloseAll()
	{
		bCloseAllRequested = true;
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
		MK12::TextFromString(&Out, &String); // Copies the string
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
		Req.Box.reset(new MK12::UIMessageBox);
		MK12::UIMessageBox* Box = Req.Box.get();

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
			MK12::FMessageBoxDelegate OnPick; // The game's own close, empty if it wasn't found
			if (MK12::MessageBoxCallback.IsReady())
				MK12::MessageBoxCallback.Fill(OnPick);
			int32_t IdCount = (int32_t)wcslen(BUTTON_ID) + 1;
			MK12::FString Id = { (wchar_t*)BUTTON_ID, IdCount, IdCount }; // Copied by the game
			MK12::MessageBoxAddButton(Box, i, &Label, ButtonInput(i, Count), &OnPick, &Id);
			Label.Release();
		}

		MK12::MessageBoxShow(Box);
		if (SettingsMgr->ShouldLog(Log::Verbose))
			printf("GameMessageBox: Showing %ls\n", Req.Body.Value.c_str());
	}

	// Ends a request and hands back its callback, which runs after the lock is released
	static std::function<void()> Finish(std::unique_ptr<Request> Req, int Picked)
	{
		if (!Req || !Advance(*Req, Phase::Done))
			return nullptr;

		Req->Box.reset();
		PendingCount--;
		auto OnPick = std::move(Req->OnPick);
		if (!OnPick)
			return nullptr;
		return [OnPick, Picked]() { OnPick(Picked); };
	}

	void Tick()
	{
		if (!IsReady() || (!PendingCount && !bCloseAllRequested && !bDismissRequested))
			return;

		bool bSafe = GameState::CanShowUI();
		std::vector<std::function<void()>> Callbacks;
		auto Collect = [&Callbacks](std::function<void()> Callback) { if (Callback) Callbacks.push_back(std::move(Callback)); };

		if (bCloseAllRequested.exchange(false))
		{
			std::deque<std::unique_ptr<Request>> Dropped;
			{
				std::lock_guard<std::mutex> Guard(QueueLock);
				Dropped.swap(Queue);
			}
			for (auto& Req : Dropped)
				Collect(Finish(std::move(Req), Closed));
			if (Current)
				MK12::MessageBoxClose(Current->Box.get(), false, false);
			Collect(Finish(std::move(Current), Closed));
		}

		if (bDismissRequested.exchange(false))
		{
			std::vector<Handle> Dismissed;
			std::deque<std::unique_ptr<Request>> Dropped;
			{
				std::lock_guard<std::mutex> Guard(QueueLock);
				Dismissed.swap(DismissRequests);
				for (auto It = Queue.begin(); It != Queue.end();)
				{
					if (std::find(Dismissed.begin(), Dismissed.end(), (*It)->Id) != Dismissed.end())
					{
						Dropped.push_back(std::move(*It));
						It = Queue.erase(It);
					}
					else
						++It;
				}
			}
			for (auto& Req : Dropped)
				Collect(Finish(std::move(Req), Closed));
			if (Current && std::find(Dismissed.begin(), Dismissed.end(), Current->Id) != Dismissed.end())
			{
				MK12::MessageBoxClose(Current->Box.get(), false, false);
				Collect(Finish(std::move(Current), Closed));
			}
		}

		if (Current)
		{
			if (!bSafe)
			{ // Loading or online started while it was up
				MK12::MessageBoxClose(Current->Box.get(), false, false);
				Collect(Finish(std::move(Current), Closed));
			}
			else if (Current->Box->IsClosed())
			{
				int Picked = Current->Box->PickedIndex;
				if (Picked < 0 || Picked >= (int)Current->Buttons.size())
					Picked = Closed;
				Collect(Finish(std::move(Current), Picked));
			}
		}

		if (!Current && bSafe)
		{
			std::unique_ptr<Request> Next;
			{
				std::lock_guard<std::mutex> Guard(QueueLock);
				if (!Queue.empty())
				{
					Next = std::move(Queue.front());
					Queue.pop_front();
				}
			}
			if (Next && Advance(*Next, Phase::Showing))
			{
				Open(*Next);
				Current = std::move(Next);
			}
		}

		bTakingInput = Current && !Current->Buttons.empty();
		if (!bTakingInput)
			KeptPresses = 0;

		for (auto& Callback : Callbacks)
			Callback();
	}
}
