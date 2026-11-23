#pragma once

// The player's online ids: Hydra (MK1 account), WB account and offline profile id, from the game's online identity once it's
// signed in. The same text the game hashes for Floyd
namespace OnlineIds {
	// "PLATFORM:id/hydra/wb", with "|offline id" after the platform id when there is one. Keeps what's already read
	void Store(const wchar_t* Text);

	// Game thread
	void Read();
	void Tick(); // Platform part when the main menu comes up, online ids once signed in (a minute at most)
	bool GaveUp(); // The last minute of reading found no Hydra id

	void SetSubsystemGetter(void* Getter);
	bool IsReady();

	// Online = signed in to MK1's servers (Hydra). Offline the game still has the platform account, Floyd's challenges
	// come from it alone
	void SetOnlineCheck(void* LocalProfiles, void* ProfileOnline);
	bool HasOnlineCheck();

	bool IsOnline(); // As of the last frame the game could show UI, any thread
}
