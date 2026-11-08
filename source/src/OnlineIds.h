#pragma once

// The player's online ids: Hydra (MK1 account), WB account and offline profile id. Read from the game's own online
// identity once it's signed in (main menu on), the same text the game hashes for Floyd. PROFILE GETTER owns them
namespace OnlineIds {
	// "PLATFORM:id/hydra/wb", with "|offline id" after the platform id when there is one. Keeps what's already read
	void Store(const wchar_t* Text);

	void Read(); // Game thread
	void Tick(); // Game thread, every frame: platform part when the main menu comes up, online ids once signed in (a minute at most)
	bool GaveUp(); // The last minute of reading ended without a Hydra id

	// The game's identity calls, found by the hooks
	void SetSubsystemGetter(void* Getter);
	bool IsReady();

	// Online means signed in to MK1's servers (Hydra), the game's own check. Offline the game still has the platform
	// account, and Floyd's challenges come from it alone
	void SetOnlineCheck(void* LocalProfiles, void* ProfileOnline);
	bool HasOnlineCheck();
	bool IsOnline(); // As of the last frame the game could show UI, any thread. False until then
}
