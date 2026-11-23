#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace MK12 { class FString; class FText; struct FPlayerInput; }

// The game's own message boxes. Any thread can ask for one, they queue and show one at a time on the game thread (Tick),
// never while loading or online. Callbacks run on the game thread
namespace GameMessageBox {
	constexpr int MaxButtons = 3; // The game has 3 inputs for them: Confirm, Alt, Back
	constexpr int Closed = -1; // Closed without a pick

	typedef uint64_t Handle;
	constexpr Handle NoHandle = 0;

	typedef void (CloseCallback)();
	typedef void (AnswerCallback)(bool bYes);
	typedef void (PickCallback)(int Picked);

	// Plain text, or a key in the game's text tables (shows in the player's language)
	struct Text {
		std::wstring Value; // The key when Namespace is set
		std::wstring Namespace;

		Text() {}
		Text(const wchar_t* Plain) : Value(Plain ? Plain : L"") {}
		Text(const std::wstring& Plain) : Value(Plain) {}

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

	// bPausesGame: pauses the game while it's up, like in matches
	// While a box with buttons is up the screen behind it gets no presses
	Handle Confirm(const Text& Body, CloseCallback* OnClose = nullptr, const Text& Title = Text(), bool bPausesGame = false);
	// Yes on Confirm, No on Back. False when closed without a pick
	Handle Ask(const Text& Body, AnswerCallback* OnAnswer, const Text& Title = Text(),
		const Text& Yes = Labels::Yes(), const Text& No = Labels::No(), bool bPausesGame = false);
	// First button on Confirm, last on Back, middle on Alt
	Handle Choose(const Text& Body, const std::vector<Text>& Buttons, PickCallback* OnPick, const Text& Title = Text(), bool bPausesGame = false);
	// No buttons, stays up until Dismiss. Boxes after it wait
	Handle Notice(const Text& Body, const Text& Title = Text(), bool bPausesGame = false);
	void Dismiss(Handle Box); // Any thread, its callback gets Closed

	bool IsReady();
	MK12::FText ToGameText(const Text& text); // Caller releases it (or hands it to the game)
	size_t Pending();
	bool IsTakingInput(); // One of ours with buttons is on screen
	bool IsOwnButton(const MK12::FString* Id);
	// Input hooks, game thread: while ours is up every new press is taken from the game and kept for our box
	void OnInputUpdate(MK12::FPlayerInput* Input);
	bool TakePress(int32_t Input);
	void CloseAll();

	void Tick(); // Game thread, every frame
}
