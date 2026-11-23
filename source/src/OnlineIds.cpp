#include "mkutils.h"
#include "OnlineIds.h"
#include "Features.h"
#include "GameState.h"
#include "mk12.h"
#include "mk12hook.h"
#include "eSettingsManager.h"
#include <Windows.h>
#include <string>
#include <vector>

namespace OnlineIds {
	constexpr ULONGLONG	RETRY_DELAY			= 3000; // Between reads while the game signs in
	constexpr ULONGLONG	READ_WINDOW			= 60000; // The game stops trying to sign in about then, so do we
	constexpr size_t	IDENTITY_SLOT		= 0x80 / 8; // Online subsystem: its identity interface
	constexpr size_t	PLAYER_ID_SLOT		= 0xF0 / 8; // Identity: the unique id of a local player
	constexpr size_t	TO_STRING_SLOT		= 0x30 / 8; // Unique id: as text
	constexpr int32_t	LOCAL_PLAYER		= 0;

	// The engine's shared pointer
	struct SharedRef {
		void* Object	= nullptr;
		void* Counter	= nullptr;
	};

	typedef void*			(__fastcall SubsystemGetterType)(); // The game's online subsystem
	typedef void			(__fastcall GetIdentityType)(void* Subsystem, SharedRef* Out);
	typedef void			(__fastcall GetPlayerIdType)(void* Identity, SharedRef* Out, int32_t LocalPlayer);
	typedef MK12::FString*	(__fastcall ToStringType)(void* Id, MK12::FString* Out);
	typedef MK12::TArray<void*>* (__fastcall GetLocalProfilesType)(); // Every local player's profile
	typedef bool			(__fastcall IsProfileOnlineType)(void* Profile); // Signed in to MK1's servers (Hydra)

	typedef void			(__fastcall DestroyType)(void* Counter);
	typedef void			(__fastcall DeleteType)(void* Counter, int Flags);

	static SubsystemGetterType*		GetSubsystem		= nullptr;
	static GetLocalProfilesType*	GetLocalProfiles	= nullptr;
	static IsProfileOnlineType*		IsProfileOnline		= nullptr;
	static ULONGLONG				LastTry				= 0;
	static ULONGLONG				WindowStart			= 0; // Reading since the game went online, 0 when not reading
	static uint32_t					SeenMainMenus		= 0;
	static bool						bWasOnline			= false;
	static bool						bGaveUp				= false;

	static void* Virtual(void* Object, size_t Slot)
	{
		return (*(void***)Object)[Slot];
	}

	static void Release(SharedRef& Ref)
	{ // Like the game: the last shared reference destroys the object, the last weak one the counter
		if (!Ref.Counter)
			return;
		volatile LONG* Counts = (volatile LONG*)((char*)Ref.Counter + 8); // Shared, then weak
		if (InterlockedDecrement(&Counts[0]) == 0)
		{
			((DestroyType*)Virtual(Ref.Counter, 0))(Ref.Counter);
			if (InterlockedDecrement(&Counts[1]) == 0)
				((DeleteType*)Virtual(Ref.Counter, 1))(Ref.Counter, 1);
		}
		Ref.Object = nullptr;
		Ref.Counter = nullptr;
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
		HookMetadata::ProfileInfo& Info = HookMetadata::UserProfileInfo;
		std::vector<std::wstring> Parts = Split(Text, L'/');
		std::vector<std::wstring> Platform = Split(Parts[0], L'|');
		if (Platform.size() >= 2)
			Keep(Info.OfflineProfileId, Platform[1], "OfflineProfileId");
		// The platform account is there even when the game couldn't sign in online
		size_t Colon = Platform[0].find(L':');
		if (Colon != std::wstring::npos)
		{
			Keep(Info.Platform, Platform[0].substr(0, Colon), "Platform");
			Keep(Info.PlatformId, Platform[0].substr(Colon + 1), "PlatformId");
		}
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
		((GetIdentityType*)Virtual(Subsystem, IDENTITY_SLOT))(Subsystem, &Identity);
		if (Identity.Object)
			((GetPlayerIdType*)Virtual(Identity.Object, PLAYER_ID_SLOT))(Identity.Object, &Id, LOCAL_PLAYER);
		if (Id.Object)
		{
			// Like the game: the returned string and the one passed in are both freed
			MK12::FString Out = { nullptr, 0, 0 };
			MK12::FString* Result = ((ToStringType*)Virtual(Id.Object, TO_STRING_SLOT))(Id.Object, &Out);
			std::wstring Text = Result && Result->Data ? Result->Data : L"";
			if (Result && Result != &Out && Result->Data && MK12::MemoryFree)
			{
				MK12::MemoryFree(Result->Data);
				MK12::FString Empty = { nullptr, 0, 0 };
				*Result = Empty;
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

	bool GaveUp()
	{
		return bGaveUp;
	}

	void SetOnlineCheck(void* LocalProfiles, void* ProfileOnline)
	{
		GetLocalProfiles = (GetLocalProfilesType*)LocalProfiles;
		IsProfileOnline = (IsProfileOnlineType*)ProfileOnline;
	}

	static bool AskOnline()
	{ // Any local profile signed in to MK1's servers. Game thread once the game is up, no profiles at startup
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

	void Tick()
	{ // Platform part each time the main menu comes up, online ids once signed in (without the game's check, from the main menu on)
		if (!GetSubsystem || !Features::IsActive(Features::Id::ProfileGetter) || !GameState::CanShowUI())
			return;
		ULONGLONG Now = GetTickCount64();
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
