#include "mkutils.h"
#include "AttractMode.h"
#include "mk12.h"
#include <Windows.h>
#include <climits>
#include <cstdlib>
#include <cstring>

namespace AttractMode {
	constexpr int32_t FRAMES_PER_SECOND = 60; // Both timers count one per frame, the menus run at 60

	static uint32_t*		Limit		= nullptr;
	static volatile int32_t	Seconds		= GameSeconds;
	static volatile LONG	bRestart	= 0; // After a change the timer starts at 0, a shorter delay doesn't fire right away

	// The title screen: the attract script's numbers for its wait, in this order: the wait (frames), whether to get
	// the demo ready before it ends (1), and the frame that does it. The game's own: 1800, 1, 1200
	namespace Title {
		constexpr const char*	SCRIPT_NAME		= "Attract.mko";
		constexpr int32_t		MAX_CONSTANTS	= 0x10000; // More is not a script's list

		static bool						bReady			= false;
		static uint32_t					ScriptHash		= 0;
		// The loaded script we last looked at, and where its wait sits (-1: not in it)
		static MK12::FScriptRecord*		Record			= nullptr;
		static MK12::FScriptConstant*	Data			= nullptr;
		static int32_t					Index			= -1;
		static int64_t					Lead			= 0; // How many frames before the wait ends the game gets the demo ready
		static int64_t					WrittenWait		= 0;
		static int64_t					WrittenReady	= 0;
		static int32_t					WrittenSeconds	= -1;
		static int32_t					RetryIn			= 0; // Frames until the next search while the wait isn't found

		bool IsNumber(const MK12::FScriptConstant& Constant)
		{
			return Constant.Kind != MK12::SCRIPT_CONSTANT_REFERENCE;
		}

		bool IsGameWait(const MK12::FScriptConstant* At)
		{ // The game's own values, as the script ships them
			return IsNumber(At[0]) && At[0].Value == GameSeconds * FRAMES_PER_SECOND
				&& IsNumber(At[1]) && At[1].Value == 1
				&& IsNumber(At[2]) && At[2].Value > 0 && At[2].Value < At[0].Value;
		}

		int32_t FindWait(const MK12::TArray<MK12::FScriptConstant>& List)
		{ // Where the wait sits in this script's list: the only place with the game's values, else -1
			if (!List.Data || List.ElementsCount < 3 || List.ElementsCount > List.Size || List.Size > MAX_CONSTANTS)
				return -1;
			int32_t Found = -1;
			for (int32_t i = 0; i + 2 < List.ElementsCount; i++)
			{
				if (!IsGameWait(List.Data + i))
					continue;
				if (Found != -1)
					return -1; // Twice: not sure which one
				Found = i;
			}
			return Found;
		}

		bool StillOurs(const MK12::TArray<MK12::FScriptConstant>& List)
		{
			return List.Data == Data && Index >= 0 && Index + 2 < List.ElementsCount
				&& Data[Index].Value == WrittenWait && Data[Index + 2].Value == WrittenReady;
		}

		void Write(int32_t Delay)
		{
			int64_t Wait = Delay == Never ? INT_MAX : (int64_t)Delay * FRAMES_PER_SECOND;
			int64_t Ready = Delay == Never ? INT_MAX : (Wait > Lead ? Wait - Lead : 0);
			Data[Index].Value = Wait;
			Data[Index + 2].Value = Ready;
			WrittenWait = Wait;
			WrittenReady = Ready;
			WrittenSeconds = Delay;
			if (SettingsMgr->ShouldLog(Log::Debug))
			{
				if (Delay == Never)
					printf("Attract Mode: the title screen never starts the demo\n");
				else
					printf("Attract Mode: the title screen starts the demo after %d seconds\n", Delay);
			}
		}

		void LogList(const MK12::FScriptRecord* Loaded, const MK12::TArray<MK12::FScriptConstant>& List)
		{ // What the loaded script holds, for the dev log: once per list the search failed on
			if (!SettingsMgr->ShouldLogDev())
				return;
			printf("Attract Mode: script at %p, constants %p, %d of %d\n", Loaded, List.Data, List.ElementsCount, List.Size);
			const uint64_t* Raw = (const uint64_t*)Loaded;
			for (int32_t i = 0xA0 / 8; i < 0x100 / 8; i++)
				printf("  +0x%02X %016llX\n", i * 8, (unsigned long long)Raw[i]);
			if (!List.Data || List.ElementsCount <= 0 || List.ElementsCount > MAX_CONSTANTS)
				return;
			for (int32_t i = 0; i < List.ElementsCount && i < 64; i++)
				printf("  [%d] %lld kind 0x%X\n", i, (long long)List.Data[i].Value, List.Data[i].Kind);
		}

		void Tick()
		{
			MK12::FScriptRecord* Loaded = MK12::FindScript(ScriptHash);
			if (!Loaded)
			{
				Record = nullptr;
				return;
			}
			const MK12::TArray<MK12::FScriptConstant>& List = Loaded->Constants;
			int32_t Delay = Seconds;
			if (Loaded == Record && StillOurs(List))
			{
				if (Delay != WrittenSeconds)
					Write(Delay);
				return;
			}
			// Not found yet in this script: it may still be loading its constants, look again now and then
			bool bSame = Loaded == Record && Index == -1 && List.Data == Data;
			if (bSame && --RetryIn > 0)
				return;
			RetryIn = 30;

			// Newly loaded (or loaded again): the game's own values are back
			bool bFirstLook = !bSame;
			Record = Loaded;
			Data = List.Data;
			Index = FindWait(List);
			if (Index == -1)
			{
				if (bFirstLook)
				{
					if (SettingsMgr->ShouldLog(Log::Debug))
						printf("Attract Mode: the title screen's timer isn't in the attract script yet, looking again\n");
					LogList(Loaded, List);
				}
				return;
			}
			Lead = Data[Index].Value - Data[Index + 2].Value;
			Write(Delay);
		}
	}

	bool WriteLimit(uint32_t Frames)
	{
		DWORD Old;
		if (!Limit || !VirtualProtect(Limit, sizeof(*Limit), PAGE_EXECUTE_READWRITE, &Old))
			return false;
		*Limit = Frames;
		VirtualProtect(Limit, sizeof(*Limit), Old, &Old);
		return true;
	}

	bool Init(uint64_t LimitAddress)
	{
		Limit = (uint32_t*)LimitAddress;
		if (!Limit || *Limit != GameSeconds * FRAMES_PER_SECOND) // Not the timer we know
		{
			Limit = nullptr;
			return false;
		}
		return true;
	}

	bool IsReady()
	{
		return Limit != nullptr;
	}

	void InitTitle()
	{
		Title::ScriptHash = MK12::ScriptNameHash(Title::SCRIPT_NAME);
		Title::bReady = MK12::FindScript != nullptr;
	}

	bool IsTitleReady()
	{
		return Title::bReady;
	}

	int32_t FromIni(const char* Value)
	{
		if (!Value || !*Value || !_stricmp(Value, "Game"))
			return GameSeconds;
		if (!_stricmp(Value, "Never"))
			return Never;
		int32_t Parsed = atoi(Value);
		return Parsed > 0 ? Parsed : GameSeconds;
	}

	int32_t Delay()
	{
		return Seconds;
	}

	void SetDelay(int32_t NewSeconds)
	{
		if (!IsReady() || NewSeconds < 0)
			return;
		if (NewSeconds != Never && !WriteLimit((uint32_t)NewSeconds * FRAMES_PER_SECOND))
			return;
		Seconds = NewSeconds;
		InterlockedExchange(&bRestart, 1);
	}

	bool ShouldRestartTimer()
	{
		return Seconds == Never || InterlockedExchange(&bRestart, 0);
	}

	void Tick()
	{
		if (IsReady() && Title::bReady)
			Title::Tick();
	}
}
