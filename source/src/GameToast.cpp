#include "mkutils.h"
#include "GameToast.h"
#include "GameState.h"
#include "mk12.h"

namespace GameToast {
	constexpr uint8_t TOAST_PRIORITY = 2; // Same as the game's social toasts

	struct Request {
		GameMessageBox::Text	Title;
		GameMessageBox::Text	Subtitle;
		bool					bLong;
	};

	// Shared with other threads
	static SRWLOCK				QueueLock	= SRWLOCK_INIT;
	static std::vector<Request>	Queue;

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
		Request Req = { Title, Subtitle, bLong };
		AcquireSRWLockExclusive(&QueueLock);
		Queue.push_back(Req);
		ReleaseSRWLockExclusive(&QueueLock);
	}

	void Tick()
	{
		if (!IsReady() || !GameState::CanShowUI())
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
			MK12::FMenuEntryCallback NoCallbackA, NoCallbackB, NoCallbackC;
			MK12::ToastEnqueue(Driver, &Title, &Subtitle, Req.bLong, DefaultDuration, TOAST_PRIORITY, &NoCallbackA, &NoCallbackB, &NoCallbackC);
		}
	}
}
