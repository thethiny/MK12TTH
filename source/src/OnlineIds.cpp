#include "mkutils.h"
#include "OnlineIds.h"
#include "Features.h"
#include "GameState.h"
#include "mk12.h"
#include "mk12hook.h"
#include "eSettingsManager.h"
#include <Windows.h>
#include <string>
#include <cstring>
#include <vector>

namespace OnlineIds {
	constexpr ULONGLONG	RETRY_DELAY			= 3000; // Between reads while the game signs in
	constexpr ULONGLONG	READ_WINDOW			= 60000; // The game stops trying to sign in about then, so do we
	constexpr size_t	IDENTITY_SLOT		= 0x80 / 8; // Online subsystem: its identity interface
	constexpr size_t	PLAYER_ID_SLOT		= 0xF0 / 8; // Identity: the unique id of a local player
	constexpr size_t	TO_STRING_SLOT		= 0x30 / 8; // Unique id: as text
	constexpr int32_t	LOCAL_PLAYER		= 0;
	constexpr ULONGLONG	AUTH_RETRY			= 3000; // Between reads while the access token isn't there yet
	constexpr ULONGLONG	AUTH_REFRESH		= 30000; // Between reads once it is: signing in again gives a new one
	constexpr size_t	AUTH_TEXT			= 4096; // Room for a ticket or token, they are far shorter
	// The signed in player's object, as the game lays it out
	constexpr size_t	PLAYER_TICKET		= 0x138; // The platform ticket, as engine text
	constexpr size_t	PLAYER_CLIENT		= 0x2B0; // The connection to MK1's servers, there once signed in
	constexpr size_t	PLAYER_CREDENTIAL	= 0x378; // What the game signs in with, holds the ticket again
	constexpr size_t	CREDENTIAL_TICKET	= 0x18; // As a standard string

	// The engine's shared pointer: the object and its reference counter
	struct SharedRef {
		void* Object = nullptr;
		void* Counter = nullptr;
	};

	typedef void*			(__fastcall SubsystemGetterType)(); // The game's online subsystem
	typedef void			(__fastcall GetIdentityType)(void* Subsystem, SharedRef* Out);
	typedef void			(__fastcall GetPlayerIdType)(void* Identity, SharedRef* Out, int32_t LocalPlayer);
	typedef MK12::FString*	(__fastcall ToStringType)(void* Id, MK12::FString* Out);
	typedef MK12::TArray<void*>* (__fastcall GetLocalProfilesType)(); // Every local player's profile
	typedef bool			(__fastcall IsProfileOnlineType)(void* Profile); // Signed in to MK1's servers (Hydra)
	typedef MK12::FString*	(__fastcall AuthTokenType)(void* Unused, MK12::FString* Out, int32_t LocalPlayer); // The access token
	typedef void*			(__fastcall AuthRegistryType)(); // The signed in players, by local player
	typedef SharedRef*		(__fastcall AuthPlayerType)(void* Registry, SharedRef* Out, int32_t LocalPlayer);

	static SubsystemGetterType* GetSubsystem = nullptr;
	static GetLocalProfilesType* GetLocalProfiles = nullptr;
	static IsProfileOnlineType* IsProfileOnline = nullptr;
	static AuthTokenType* GetAuthToken = nullptr;
	static AuthRegistryType* GetAuthRegistry = nullptr;
	static AuthPlayerType* GetAuthPlayer = nullptr;
	static ULONGLONG LastTry = 0;
	static ULONGLONG LastAuthTry = 0;
	static ULONGLONG WindowStart = 0; // Reading since the game went online, 0 when not reading
	static uint32_t SeenMainMenus = 0;
	static bool bWasOnline = false;
	static bool bGaveUp = false;

	template <typename T>
	static T Virtual(void* Object, size_t Slot)
	{
		return (T)(*(void***)Object)[Slot];
	}

	// Same as the game: the last shared reference destroys the object, the last weak one the counter
	static void Release(SharedRef& Ref)
	{
		if (!Ref.Counter)
			return;
		volatile LONG* Counts = (volatile LONG*)((char*)Ref.Counter + 8); // Shared, then weak
		if (InterlockedDecrement(&Counts[0]) == 0)
		{
			Virtual<void(__fastcall*)(void*)>(Ref.Counter, 0)(Ref.Counter);
			if (InterlockedDecrement(&Counts[1]) == 0)
				Virtual<void(__fastcall*)(void*, int)>(Ref.Counter, 1)(Ref.Counter, 1);
		}
		Ref = {};
	}

	static std::vector<std::wstring> Split(const std::wstring& Text, wchar_t Separator)
	{
		std::vector<std::wstring> Parts;
		size_t Start = 0;
		while (true)
		{
			size_t End = Text.find(Separator, Start);
			Parts.push_back(Text.substr(Start, End == std::wstring::npos ? std::wstring::npos : End - Start));
			if (End == std::wstring::npos)
				return Parts;
			Start = End + 1;
		}
	}

	static void Keep(wchar_t*& Field, const std::wstring& Value, const char* Name)
	{
		if (Field || Value.empty())
			return;
		Field = new wchar_t[Value.size() + 1];
		wcscpy_s(Field, Value.size() + 1, Value.c_str());
		if (SettingsMgr->ShouldLogProfile())
			printf("%s extracted: %ls\n", Name, Field);
	}

	void Store(const wchar_t* Text)
	{
		if (!Text || !*Text)
			return;
		auto& Info = HookMetadata::UserProfileInfo;
		std::vector<std::wstring> Parts = Split(Text, L'/');
		std::vector<std::wstring> Platform = Split(Parts[0], L'|');
		// The platform's own account is there even when the game couldn't sign in online
		size_t Colon = Platform[0].find(L':');
		if (Colon != std::wstring::npos)
		{
			Keep(Info.Platform, Platform[0].substr(0, Colon), "Platform");
			Keep(Info.PlatformId, Platform[0].substr(Colon + 1), "PlatformId");
		}
		// The offline profile id after "|" is the same account, used when the platform part has no id
		if (Platform.size() >= 2)
			Keep(Info.PlatformId, Platform[1], "PlatformId");
		if (Parts.size() >= 2)
			Keep(Info.HydraId, Parts[1], "HydraId");
		if (Parts.size() >= 3)
			Keep(Info.WBId, Parts[2], "WBId");
	}

	void SetSubsystemGetter(void* Getter)
	{
		GetSubsystem = (SubsystemGetterType*)Getter;
	}

	bool IsReady()
	{
		return GetSubsystem != nullptr;
	}

	void Read()
	{
		void* Subsystem = GetSubsystem ? GetSubsystem() : nullptr;
		if (!Subsystem)
			return;
		SharedRef Identity, Id;
		Virtual<GetIdentityType*>(Subsystem, IDENTITY_SLOT)(Subsystem, &Identity);
		if (Identity.Object)
			Virtual<GetPlayerIdType*>(Identity.Object, PLAYER_ID_SLOT)(Identity.Object, &Id, LOCAL_PLAYER);
		if (Id.Object)
		{
			// Like the game: the returned string is taken over and freed, so is the one passed in
			MK12::FString Out = { nullptr, 0, 0 };
			MK12::FString* Result = Virtual<ToStringType*>(Id.Object, TO_STRING_SLOT)(Id.Object, &Out);
			std::wstring Text = Result && Result->Data ? Result->Data : L"";
			if (Result && Result != &Out && Result->Data && MK12::MemoryFree)
			{
				MK12::MemoryFree(Result->Data);
				*Result = { nullptr, 0, 0 };
			}
			if (Out.Data && MK12::MemoryFree)
				MK12::MemoryFree(Out.Data);
			if (SettingsMgr->ShouldLogProfile() && !Text.empty())
				printf("Online Id: %ls\n", Text.c_str());
			Store(Text.c_str());
		}
		Release(Id);
		Release(Identity);
	}

	void SetAuthReaders(void* TokenGetter, void* RegistryGetter, void* PlayerLookup)
	{
		GetAuthToken = (AuthTokenType*)TokenGetter;
		GetAuthRegistry = (AuthRegistryType*)RegistryGetter;
		GetAuthPlayer = (AuthPlayerType*)PlayerLookup;
	}

	// Engine text to UTF-8, false when it's empty or doesn't fit
	static bool Narrow(const wchar_t* Text, char* Out, size_t OutSize)
	{
		if (!Text || !*Text)
			return false;
		return WideCharToMultiByte(CP_UTF8, 0, Text, -1, Out, (int)OutSize, nullptr, nullptr) > 1;
	}

	// Reads the keys with the game's own calls. Plain data only, so a bad read ends here and not in the game:
	// bit 0 is the token, bit 1 the ticket
	static unsigned ReadAuthRaw(char* Token, size_t TokenSize, char* Ticket, size_t TicketSize)
	{
		unsigned Got = 0;
		__try
		{
			if (GetAuthToken)
			{
				MK12::FString Out = { nullptr, 0, 0 };
				GetAuthToken(nullptr, &Out, LOCAL_PLAYER);
				if (Narrow(Out.Data, Token, TokenSize))
					Got |= 1;
				if (Out.Data && MK12::MemoryFree)
					MK12::MemoryFree(Out.Data);
			}
			if (GetAuthRegistry && GetAuthPlayer)
			{
				SharedRef Ref;
				GetAuthPlayer(GetAuthRegistry(), &Ref, LOCAL_PLAYER);
				char* Player = (char*)Ref.Object;
				// Signed in only when the connection is there, before that the player holds nothing of ours
				if (Player && *(void**)(Player + PLAYER_CLIENT))
				{
					if (Narrow(((MK12::FString*)(Player + PLAYER_TICKET))->Data, Ticket, TicketSize))
						Got |= 2;
					else if (char* Credential = *(char**)(Player + PLAYER_CREDENTIAL))
					{
						// A standard string: the text in place when it's short, else where the pointer says
						char* String = Credential + CREDENTIAL_TICKET;
						size_t Size = *(size_t*)(String + 0x10);
						size_t Capacity = *(size_t*)(String + 0x18);
						const char* Text = Capacity >= 16 ? *(char**)String : String;
						if (Text && Size > 0 && Size < TicketSize)
						{
							memcpy(Ticket, Text, Size);
							Ticket[Size] = 0;
							Got |= 2;
						}
					}
				}
				Release(Ref);
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			Got = 0;
		}
		return Got;
	}

	// The platform the game runs on. The Epic build ships and loads Epic's online library, the Steam build doesn't
	static const char* PlatformName()
	{
		return GetModuleHandleW(L"EOSSDK-Win64-Shipping.dll") ? "epic" : "steam";
	}

	// Fills the keys other features show and use. Game thread. Signing in again gives a new token, so it's read again
	static void ReadAuth()
	{
		if (!GetAuthToken && !(GetAuthRegistry && GetAuthPlayer))
			return;
		static char Token[AUTH_TEXT];
		static char Ticket[AUTH_TEXT];
		Token[0] = Ticket[0] = 0;
		unsigned Got = ReadAuthRaw(Token, sizeof(Token), Ticket, sizeof(Ticket));
		auto& Keys = HookMetadata::sUserKeys;
		if ((Got & 1) && strcmp(Keys.AccessToken.c_str(), Token) != 0)
		{
			Keys.AccessToken = Token;
			if (SettingsMgr->ShouldLogProfile())
				printf("Access token read (%zu chars)\n", strlen(Token));
			if (SettingsMgr->ShouldLog(Log::Verbose))
				printf("Access Token: %s\n", Token);
		}
		if ((Got & 2) && strcmp(Keys.PlatformTicket.c_str(), Ticket) != 0)
		{
			Keys.PlatformTicket = Ticket;
			if (SettingsMgr->ShouldLogProfile())
				printf("Platform ticket read (%zu chars)\n", strlen(Ticket));
			if (SettingsMgr->ShouldLog(Log::Verbose))
				printf("Platform Ticket: %s\n", Ticket);
		}
		if (Got && Keys.Platform.empty())
		{
			Keys.Platform = PlatformName();
			if (SettingsMgr->ShouldLogProfile())
				printf("Platform: %s\n", Keys.Platform.c_str());
		}
	}

	static bool AskOnline();

	// While signed in: until the token is there, then now and then
	static void TickAuth(ULONGLONG Now)
	{
		ULONGLONG Wait = HookMetadata::sUserKeys.AccessToken.empty() ? AUTH_RETRY : AUTH_REFRESH;
		if (Now - LastAuthTry < Wait)
			return;
		if (HasOnlineCheck() && !AskOnline())
			return;
		LastAuthTry = Now;
		ReadAuth();
	}

	bool GaveUp()
	{
		return bGaveUp;
	}

	void SetOnlineCheck(void* LocalProfiles, void* ProfileOnline)
	{
		GetLocalProfiles = (GetLocalProfilesType*)LocalProfiles;
		IsProfileOnline = (IsProfileOnlineType*)ProfileOnline;
	}

	// The game's own check: any local profile signed in to MK1's servers. Game thread once the game is up (Tick):
	// at startup its profiles don't exist yet
	static bool AskOnline()
	{
		if (!GetLocalProfiles || !IsProfileOnline)
			return false;
		MK12::TArray<void*>* Profiles = GetLocalProfiles();
		for (int32_t i = 0; Profiles && i < Profiles->ElementsCount; i++)
		{
			if (Profiles->Data[i] && IsProfileOnline(Profiles->Data[i]))
				return true;
		}
		return false;
	}

	void* LocalProfile()
	{
		if (!GetLocalProfiles)
			return nullptr;
		MK12::TArray<void*>* Profiles = GetLocalProfiles();
		return Profiles && Profiles->ElementsCount > 0 ? Profiles->Data[0] : nullptr;
	}

	bool IsOnline()
	{
		return bWasOnline;
	}

	bool HasOnlineCheck()
	{
		return GetLocalProfiles && IsProfileOnline;
	}

	// The platform part once each time the main menu comes up. The online ids once the game signs in (or, without
	// the game's check, from the main menu on), for a minute at most
	void Tick()
	{
		if (!Features::IsActive(Features::Id::ProfileGetter) || !GameState::CanShowUI())
			return;
		ULONGLONG Now = GetTickCount64();
		TickAuth(Now);
		if (!GetSubsystem)
			return;
		bool bHaveIds = HookMetadata::UserProfileInfo.HydraId != nullptr;
		uint32_t MainMenus = GameState::MainMenuEntries();
		if (MainMenus != SeenMainMenus)
		{
			SeenMainMenus = MainMenus;
			if (!bHaveIds)
			{
				Read();
				if (!HasOnlineCheck())
					WindowStart = Now;
			}
		}
		bool bOnline = AskOnline();
		if (bOnline && !bWasOnline && !bHaveIds)
		{
			WindowStart = Now;
			LastTry = 0;
			bGaveUp = false;
		}
		bWasOnline = bOnline;
		if (!WindowStart || bHaveIds)
		{
			WindowStart = 0;
			return;
		}
		if (Now - WindowStart > READ_WINDOW)
		{
			WindowStart = 0;
			bGaveUp = true;
			if (SettingsMgr->ShouldLogProfile())
				printf("Online Id: not signed in online, keeping what the platform gave\n");
			return;
		}
		if (Now - LastTry < RETRY_DELAY)
			return;
		LastTry = Now;
		Read();
	}
}
