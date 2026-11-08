#include "mkutils.h"
#include "GameToast.h"
#include "GameState.h"
#include "mk12.h"
#include <deque>
#include <mutex>

namespace GameToast {
	constexpr uint8_t TOAST_PRIORITY = 2; // What the game's social toasts use

	struct Request {
		GameMessageBox::Text Title;
		GameMessageBox::Text Subtitle;
		bool bLong = false;
	};

	static std::mutex QueueLock;
	static std::deque<Request> Queue;

	bool IsReady()
	{
		return MK12::ToastDriver && MK12::ToastEnqueue && MK12::TextFromString;
	}

	bool IsUp()
	{
		return IsReady() && GameState::CanShowUI() && MK12::ToastDriver();
	}

	void Show(const GameMessageBox::Text& Title, const GameMessageBox::Text& Subtitle, bool bLong)
	{
		if (!IsReady())
			return;
		std::lock_guard<std::mutex> Guard(QueueLock);
		Queue.push_back({ Title, Subtitle, bLong });
	}

	void Tick()
	{
		if (!IsReady() || !GameState::CanShowUI())
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
			MK12::FMenuEntryCallback NoCallbackA, NoCallbackB, NoCallbackC;
			MK12::ToastEnqueue(Driver, &Title, &Subtitle, Req.bLong, DefaultDuration, TOAST_PRIORITY, &NoCallbackA, &NoCallbackB, &NoCallbackC);
		}
	}
}
