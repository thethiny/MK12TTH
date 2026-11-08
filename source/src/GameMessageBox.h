#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace MK12 { class FString; class FText; struct FPlayerInput; }

// The game's own message box popups, shown on demand. Requests can come from any thread,
// they wait in a queue and show one at a time from the game thread (Tick), never during
// loading screens or online modes. Callbacks run on the game thread
namespace GameMessageBox {
	constexpr int MaxButtons = 3; // Each button needs its own input, the game has 3 (Confirm, Alt, Back)
	constexpr int Closed = -1; // Picked index when it closed without a pick (blocked, dismissed or closed by the game)

	// Names a queued or showing box so it can be dismissed. NoHandle means it wasn't queued
	using Handle = uint64_t;
	constexpr Handle NoHandle = 0;

	// Plain text, or a key into the game's text tables so it shows in the player's language
	struct Text {
		std::wstring Value; // The text, or the key when Namespace is set
		std::wstring Namespace;

		Text() = default;
		Text(const wchar_t* Plain) : Value(Plain ? Plain : L"") {}
		Text(std::wstring Plain) : Value(std::move(Plain)) {}

		static Text Plain(const std::string& Utf8);
		static Text Key(const wchar_t* Namespace, const wchar_t* Key);
		bool IsEmpty() const { return Value.empty(); }
	};

	// The game's own button labels
	namespace Labels {
		Text OK();
		Text Yes();
		Text No();
		Text Cancel();
		Text Confirm();
		Text Back();
	}

	// bPausesGame: pauses the whole game while it's up, like the game does in matches. Off by default.
	// While a box with buttons is up the screen behind it stops taking button presses, notices leave it alone

	// One OK button
	Handle Confirm(const Text& Body, std::function<void()> OnClose = nullptr, const Text& Title = {}, bool bPausesGame = false);
	// Two buttons, Yes on Confirm and No on Back. Gets false when it closes without a pick
	Handle Ask(const Text& Body, std::function<void(bool Yes)> OnAnswer, const Text& Title = {},
		const Text& Yes = Labels::Yes(), const Text& No = Labels::No(), bool bPausesGame = false);
	// 1 to MaxButtons buttons. The first is on Confirm, the last on Back, the middle on Alt. Gets Closed without a pick
	Handle Choose(const Text& Body, const std::vector<Text>& Buttons, std::function<void(int Picked)> OnPick, const Text& Title = {}, bool bPausesGame = false);
	// No buttons, stays up until Dismiss ("Loading Floyd..."). Boxes queued after it wait for it
	Handle Notice(const Text& Body, const Text& Title = {}, bool bPausesGame = false);
	// Closes it if it's showing, drops it if it's queued. Its callback gets Closed. Any thread
	void Dismiss(Handle Box);

	bool IsReady(); // The game's message box functions were found
	MK12::FText ToGameText(const Text& text); // Game text for a Text, the caller releases it (or hands it to the game)
	size_t Pending(); // Queued and showing
	bool IsTakingInput(); // One of ours with buttons is on screen, the screens behind it must not take button presses
	bool IsOwnButton(const MK12::FString* Id); // The press check is for one of our buttons
	// While one of ours with buttons is up, every new press is taken from the game (so the screen behind can't use it)
	// and kept for our box. Called by the input hooks, game thread only
	void OnInputUpdate(MK12::FPlayerInput* Input);
	bool TakePress(int32_t Input);
	void CloseAll(); // Drops the queue and closes the one on screen, callbacks get Closed

	// Game thread only, called every frame by the hook
	void Tick();
}
