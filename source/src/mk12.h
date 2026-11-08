#pragma once
#include <cppython.h>
#include <map>
#include "mk12_enums.h"

//enum PLAYER_NUM
//{
//	INVALID_PLAYER_NUM			= 0xFFFFFFFF,
//	PLAYER1						= 0x0,
//	PLAYER2,
//	PLAYER3,
//	PLAYER4,
//	MAX_PLAYERS,
//	CPU_PLAYER					= 0x5,
//	NOT_CPU_PLAYER				= 0x6,
//	BACKGROUND_PLAYER			= 0x7,
//};

bool containsFloydCaseInsensitive(const wchar_t* str); // Debug
bool containsCaseInsensitive(const char* str, const char* compare);

namespace MK12 { // Namespace for game functions / structs

#pragma warning(push)
#pragma warning(disable: 4200)
	// Structs
	class FName {
	public:
		uint16_t	NameSize;
		char		Name[];

		uint16_t GetSize() const;
		char* GetName();
		void Print();
		char* ToStr();
		static FName* FromStr(const char* string);
	};

	struct FNameInfoStruct {
		uint16_t	NameOffset;
		uint16_t	NameTableId;
		uint32_t	DuplicateId;
	};

	struct UNameTableStruct {
		uint32_t	UNK;
		uint32_t	UNK2;
		uint32_t	TablesCount;
		uint32_t	ProbTablesSize;
		FName*		NameTableEntry[];
	};

#pragma warning(pop)

	struct UNameTableMainStruct {
		uint64_t			ProbablyUNameClassPtr;
		bool				IsInitialized;
		uint8_t				PAD[7];
		UNameTableStruct	UNameTable;
	};

	struct JSONEndpointValue {
		uint64_t*	UnkPointer;
		int32_t		UnkValue; // Usually is `2`
		wchar_t		IntChar[2]; // Either is of size 2, or is of size UnkValue, can't tell yet.
		wchar_t*	ValuePointer;
		int32_t		ValueLength;
		int32_t		ValueLength8BAligned;
	};



	class FString {
	public:
		wchar_t*	Data;
		int32_t		Count;
		int32_t		Max;

		const wchar_t* c_str() const;
		int32_t Len() const;
		bool IsEmpty() const;
	};

	template <typename T>
	class TArray {
	public:
		T* Data;
		int32_t ElementsCount;
		int32_t Size;
	};

	namespace TArrayHelpers { // Not added to the class because I don't know the correct implementation
		template <typename T>
		T* index(TArray<T> array, int32_t index) {
			int32_t CalcIndex = array.Size * index;
			return &array.Data[index]; // if this doesn't work then I should calculate index manually as index*Size;
		}

	}

	namespace CURL {

		class HTTPRequestWrapper
		{
		public:
			virtual ~HTTPRequestWrapper() = default;
			char _pad08[0x58];           // 0x08 - 0x5F
			void* CurlEasyHandle;        // 0x60
			void* CurlHeaderList;        // 0x68
			char _pad70[0x38];           // 0x70 - 0xA7
			void* ResponseBuffer;        // 0xA8
			char _padB0[0x08];           // 0xB0 - 0xB7
			void* ContentProvider;       // 0xB8

			bool GetRequestBody(char** out, int32_t* size);
			bool GetResponseBody(char** out, int32_t* size);
		};

	};


	extern uint8_t KlassicTowerSecretFightDataOffset;

	class FMissionBuildStep_InterruptForSecretFight
	{
	public:
		void* N00000224; //0x0008

		virtual void Function0();
	}; //Size: 0x0010

	class FMissionBuildStep_KlassicTowerSecretFight
	{
	public:
		TArray<uint8_t> mDifficultyMap; // 0x0008

		virtual void Function0(); 
	}; //Size: 0x0010

	class FMissionBuildStep_LadderClue
	{
	public:
		class ULadderCluePrefightInterrupt* mPrefightInterrupt; //0x0008
		float mChanceForHint; //0x0010
		int32_t mRequiredTowerCompletions; //0x0014
		int32_t mMatchCountFloor; //0x0018
		int32_t mMatchCountCeiling; //0x001C
		bool bEnable; //0x0020
		char pad_0021[7]; //0x0021

		virtual void Function0();
	}; //Size: 0x0028

	class ScriptStructSource // Custom
	{
	public:
		uint32_t unk1;
		uint32_t unk2;
		void* unk3;
		MK12::FNameInfoStruct ScriptClass;
		//idc about the rest

		virtual void Function0();
	};

	class FKlassicLadderSecretFightCondition
	{
	public:
		ScriptStructSource* StructPtrType; //0x0008
		class FLadderSecretFightConditionBase* StructPtrInstance; //0x0010
		char pad_0018[8]; //0x0018
		int32_t Index; //0x0020
		bool CanCheckDuringFight; //0x0024
		bool enable; //0x0025
		char pad_0026[2]; //0x0026

		virtual void Function0();
	}; //Size: 0x0028

	class FLadderSecretFightConditionBase
	{
	public:

		virtual void Function0();
	};

	struct FPrimaryAssetType
	{
		FNameInfoStruct Name;                                                             // 0x0000 (size: 0x8)
	};

	struct FPrimaryAssetId
	{
		FPrimaryAssetType PrimaryAssetType;                                               // 0x0000 (size: 0x8)
		FNameInfoStruct PrimaryAssetName;                                                 // 0x0008 (size: 0x8) // Should be FName but I done messed something up
	};

	struct FCharacterRequirementConditionHelper
	{
		FPrimaryAssetId Character;                                                        // 0x0000 (size: 0x10)
		FPrimaryAssetId Kameo;                                                            // 0x0010 (size: 0x10)
		FPrimaryAssetId OpponentCharacter;                                                // 0x0020 (size: 0x10)
		FPrimaryAssetId OpponentKameo;                                                    // 0x0030 (size: 0x10)
		bool enable;                                                                      // 0x0040 (size: 0x1)

	}; // Size: 0x44

	class FSecretFightMoveTraceCondition
	{
	public:
		Enums::EManualMoveTraceEvent mTraceID;                                              // 0x0000 (size: 0x1)
		Enums::EMoveTraceCondition mTraceCondition;                                         // 0x0001 (size: 0x1)
		Enums::EMoveTraceFlags mTraceFlags;                                                 // 0x0004 (size: 0x4)
		int32_t mRequiredCompletions;                                                       // 0x0008 (size: 0x4)
	};

	class FFightingStatCondition
	{
	public:
		FCharacterRequirementConditionHelper mCharacterRequirements;                      // 0x0000 (size: 0x44)
		Enums::EFighterStatCheckMode mFighterCheckMode;                                   // 0x0044 (size: 0x4)
		Enums::FGStatID mStatID;                                                          // 0x0048 (size: 0x4)
		Enums::EComparisonType mComparisonType;                                           // 0x004C (size: 0x4)
		int32_t mRequiredValue;                                                           // 0x0050 (size: 0x4)
		bool mCheckOpponentTeam;                                                          // 0x0054 (size: 0x1)
		Enums::EFightingStatRoundToCheck mRoundToCheck;                                   // 0x0058 (size: 0x4)
		bool mCompareAgainstOtherStat;                                                    // 0x005C (size: 0x1)
		Enums::FGStatID mComparisonStatID;                                                // 0x0060 (size: 0x4)
	};

	class FLadderSecretFightCondition_FightingStatCondition : public FLadderSecretFightConditionBase
	{
	public:
		FCharacterRequirementConditionHelper mCharacterRequirements;                      // 0x0008 (size: 0x44)
		Enums::EFighterStatCheckMode mFighterCheckMode;                                   // 0x004C (size: 0x4)
		Enums::FGStatID mStatID;                                                          // 0x0050 (size: 0x4)
		Enums::EComparisonType mComparisonType;                                           // 0x0054 (size: 0x4)
		int32_t mRequiredValue;                                                           // 0x0058 (size: 0x4)
		bool mCheckOpponentTeam;                                                          // 0x005C (size: 0x1)
		Enums::EFightingStatRoundToCheck mRoundToCheck;                                   // 0x0060 (size: 0x4)
		bool mCompareAgainstOtherStat;                                                    // 0x0064 (size: 0x1)
		Enums::FGStatID mComparisonStatID;                                                // 0x0068 (size: 0x4)
	};

	class FLadderSecretFightCondition_MultiFightingStatCondition : public FLadderSecretFightConditionBase
	{
	public:
		TArray<FFightingStatCondition> mConditions;
		Enums::EComparisonGroup mGroupType;

		virtual void Function0();
	};

	class FLadderSecretFightCondition_MoveTraceConditions : public FLadderSecretFightConditionBase
	{
	public:
		FCharacterRequirementConditionHelper mCharacterRequirements;                      // 0x0008 (size: 0x44)
		bool mCheckOpponentTeam;                                                          // 0x004C (size: 0x1)
		TArray<FSecretFightMoveTraceCondition> mConditions;                               // 0x0050 (size: 0x10)
		Enums::EComparisonGroup mGroupType;                                               // 0x0060 (size: 0x4)
		int32_t mRequiredCompletions;                                                     // 0x0064 (size: 0x4)

		virtual void Function0();
	};

	class FLadderSecretFightCondition_ProfileStatCondition : public FLadderSecretFightConditionBase
	{
	public:
		Enums::MKProfileStatID mStatID;                                                          // 0x0008 (size: 0x4)
		Enums::EComparisonType mComparisonType;                                                  // 0x000C (size: 0x4)
		int32_t mRequiredValue;                                                             // 0x0010 (size: 0x4)
		Enums::EComparisonType mPassesComparisonType;                                            // 0x0014 (size: 0x4)
		int32_t mRequiredPasses;                                                            // 0x0018 (size: 0x4)


		virtual void Function0();
	};

	class FKlassicLadderSecretFightData
	{
	public:
		bool mShowSecretFightUI; //0x0008 // Part of another struct but I'm not gonna use it so this is enough
		char pad_0009[7]; //0x0009
		int64_t N00000248; //0x0010
		int64_t N00000249; //0x0018
		char pad_0020[24]; //0x0020
		int64_t N0000024D; //0x0038
		int32_t N0000024E; //0x0040
		int32_t N000002D8; //0x0044
		int32_t N0000024F; //0x0048
		int32_t N000002DB; //0x004C
		char pad_0050[8]; //0x0050
		int64_t N00000251; //0x0058
		int32_t N00000252; //0x0060
		char pad_0064[4]; //0x0064
		//class TArray<FKlassicLadderSecretFightCondition> mConditions; //0x0068
		TArray<FKlassicLadderSecretFightCondition> mConditions;
		void* mMissionClass; //0x0078
		void* mLootRewards; //0x0080

		virtual void Function0();
	};

	class UKlassicTowerSecretFightData
	{
	public:
		void* Ignore1;
		void* Ignore2;
		void* Ignore3;
		void* Ignore4;
		void* Ignore5;
		FMissionBuildStep_LadderClue mLadderClueBuildStep; // 0x30
		FMissionBuildStep_KlassicTowerSecretFight mSecretFightRulesBuildStep; // 0x58
		FMissionBuildStep_InterruptForSecretFight mSecretFightInterruptBuildStep; // 0x60
		FKlassicLadderSecretFightData mSecretFightData; // 0x70 = KlassicTowerSecretFightDataOffset

		virtual void Function0();
	}; // Total Size: 0xF8

	// Floyd (Taco) Tower window, built by the game's schedule builder
	class FTacoTowerEntry
	{
	public:
		FString LadderName; // 0x00 Taco_Tower_CharacterTrialLadder_1
		FString SplineName; // 0x10 MapMode_MeshSpline_BP_C_x, the Living Towers slot
		char Unknown[0x20]; // 0x20 Tower class path
		int64_t StartTicks; // 0x40 FDateTime ticks (UTC)
		int64_t EndTicks; // 0x48
	}; // Total Size: 0x50

	// Owner of the schedule passed to the game's schedule update, only the list is read
	class UTacoTowerSchedule
	{
	public:
		char Unknown[0x20];
		TArray<FTacoTowerEntry> Entries; // 0x20 Today's and yesterday's windows
		int64_t CachedDayTicks; // 0x30 Rebuilds when the UTC day changes
	};

	// Shared reference counter behind FText
	struct FSharedRefController
	{
		void** VTable; // 0x00 [0] destroys the object, [1] deletes the counter
		volatile long SharedRefs; // 0x08
		volatile long WeakRefs; // 0x0C
	};

	// Game text. Holds a shared reference, so every FText we fill must be released
	class FText
	{
	public:
		void* TextData = nullptr; // 0x00
		FSharedRefController* Controller = nullptr; // 0x08
		uint32_t Flags = 0; // 0x10

		void Release();
	}; // Total Size: 0x18

	// Callback run when a message box button is picked. All zeros means none, the box just closes
	struct alignas(16) FMessageBoxDelegate
	{
		uint64_t Storage[8] = {};
	}; // Total Size: 0x40

	// One player's buttons. A press is Down and not Previous; whoever reads it first in a frame marks it in Previous,
	// so later readers that frame don't see it. Menus read presses from this directly
	struct FPlayerInput
	{
		uint32_t Down; // 0x00 MessageBoxInput style bits, held this frame
		char Unknown04[0x44 - 0x04];
		uint32_t Previous; // 0x44 Held last frame, plus presses already taken this frame
		char Unknown48[0xD8 - 0x48];
	}; // Total Size: 0xD8

	// The game's standard button callback (closes the box with its sound), filled like the game fills it
	struct FMessageBoxCallbackParts
	{
		void* Invoker = nullptr; // Storage[0]
		void* VTable = nullptr; // Storage[4], the inline storage's vtable
		void* Function = nullptr; // Storage[5]

		bool IsReady() const { return Invoker && VTable && Function; }
		void Fill(FMessageBoxDelegate& Delegate) const
		{
			Delegate.Storage[0] = (uint64_t)Invoker;
			Delegate.Storage[4] = (uint64_t)VTable;
			Delegate.Storage[5] = (uint64_t)Function;
		}
	};

	// Input that picks a message box button, mouse clicks work on any of them
	namespace MessageBoxInput {
		constexpr int32_t Confirm = 32; // A / Cross / Enter
		constexpr int32_t Alt = 64; // X / Square
		constexpr int32_t Back = 128; // B / Circle / Escape
	}

	// The game's standard message box. Only the fields we read are named
	class alignas(16) UIMessageBox
	{
	public:
		static constexpr int32_t MaxButtons = 4;
		static constexpr int32_t StateDone = 2;

		char Unknown0[0x84];
		int32_t InputDelay; // 0x84 Frames it still ignores presses after showing
		char Unknown88[0x8A - 0x88];
		bool bActive; // 0x8A Set while it's on screen
		char Unknown8B[0x8D - 0x8B];
		bool bPausesGame; // 0x8D Pauses the whole game while it's up (meant for matches), set before showing
		char Unknown8E[0x94 - 0x8E];
		int32_t State; // 0x94 StateDone once it's closed
		char Unknown98[0x218 - 0x98];
		int32_t PickedInput; // 0x218 MessageBoxInput of the picked button
		char Unknown21C[0x260 - 0x21C];
		int32_t PickedIndex; // 0x260 -1 until a button is picked
		char Unknown264[0x4D0 - 0x264];

		bool IsClosed() const { return State == StateDone || !bActive; }
	}; // Total Size: 0x4D0

	// Row kinds of the settings screen
	namespace SettingsOptionType {
		constexpr int32_t Category = 1; // Header row
		constexpr int32_t List = 2; // Left / right through a list of values
		constexpr int32_t Slider = 3;
		constexpr int32_t Link = 4;
	}

	// One row of the settings screen. Plain C++ objects the screen keeps raw pointers to, never freed
	class FSettingsOption
	{
	public:
		static constexpr size_t Size = 0xD0; // Category rows, list rows are bigger

		void** VTable; // 0x00
		char Unknown08[0x8]; // 0x08
		const wchar_t* Table; // 0x10 Text table of the title
		const wchar_t* Key; // 0x18 Title key, or the title itself when bLocalizedTitle is false
		int32_t Id; // 0x20 Unique within its page
		int32_t Type; // 0x24 SettingsOptionType
		char Unknown28[0x18]; // 0x28 Change callback, kept from the copied row
		char Callbacks[0x80]; // 0x40 Two optional callbacks, all zeros means none
		bool bLocalizedTitle; // 0xC0
		char UnknownC1[0xF]; // 0xC1
	}; // Total Size: 0xD0

	// One value of a list row
	struct FSettingsListValue
	{
		const wchar_t* Table; // 0x00
		const wchar_t* Key; // 0x08 Shown as is when the value isn't localized
		const wchar_t* Raw; // 0x10
		int32_t Value; // 0x18 What Get / Set / Default use
		bool bLocalized; // 0x1C
	}; // Total Size: 0x20

	// A list row. Get / Set / Default are (function, context) pairs the screen calls with the context first
	class FSettingsListOption
	{
	public:
		typedef int32_t (__fastcall GetType)(void* Context, int64_t* Player);
		typedef void (__fastcall SetType)(void* Context, int32_t* Value, int64_t* Player);
		typedef int32_t (__fastcall DefaultType)(void* Context);
		static constexpr size_t Size = 0x110;

		char Base[0xC8]; // 0x00 FSettingsOption without its tail
		TArray<FSettingsListValue> Values; // 0xC8
		bool bLocalizedValues; // 0xD8
		bool bSkipLast; // 0xD9 Hides the last value when cycling
		char UnknownDA[0x6]; // 0xDA
		GetType* Get; // 0xE0
		void* GetContext; // 0xE8
		SetType* Set; // 0xF0
		void* SetContext; // 0xF8
		DefaultType* Default; // 0x100
		void* DefaultContext; // 0x108
	}; // Total Size: 0x110

	// A link row: no value, Confirm calls Activate with its context and the player (like REMAP KONTROLS)
	class FSettingsLinkOption
	{
	public:
		typedef bool (__fastcall ActivateType)(void* Context, void* Player); // False closes the screen, ours always return true
		static constexpr size_t Size = 0xE0;

		char Base[0xC8]; // 0x00 FSettingsOption without its tail
		ActivateType* Activate; // 0xC8
		void* Context; // 0xD0
		char UnknownD8[0x8]; // 0xD8
	}; // Total Size: 0xE0

	// One page of the settings screen, a tab shows the page with the same index
	struct FSettingsPage
	{
		FSettingsOption** Options; // 0x00
		int32_t Count; // 0x08
		int32_t Unused; // 0x0C
	}; // Total Size: 0x10

	// One entry of a game menu (a settings tab label is one)
	class alignas(16) FMenuEntry
	{
	public:
		FString Key; // 0x00 Text key, or the label itself when bLocalized is false
		FString Description; // 0x10 Shown as is under the menu when bLocalized is false (game memory, the game frees it)
		char Unknown20[0x30 - 0x20];
		void* OnPick; // 0x30 Callable run when the entry is picked, its vtable slot 10 runs it
		int32_t OnPickStorage; // 0x38
		char Unknown3C[0x58 - 0x3C];
		uint32_t FlagsA; // 0x58
		uint32_t FlagsB; // 0x5C
		char Unknown60[0xD4 - 0x60];
		bool bLocalized; // 0xD4
		char UnknownD5[0xE0 - 0xD5];
	}; // Total Size: 0xE0

	// Callback when a menu entry is picked, all zeros means none
	struct FMenuEntryCallback
	{
		void* Instance = nullptr;
		int32_t bAllocated = 0;
	}; // Total Size: 0x10

	// One item of a menu bar (settings tabs, main menu entries), owns a list of FMenuEntry. Groups hold several
	struct alignas(8) FSettingsTab
	{
		TArray<FMenuEntry> Entries; // 0x00
		char Unknown10[0x10]; // 0x10
	}; // Total Size: 0x20

	// One entry of the in-fight pause menu: its label and the panel it opens (KONTROLS, MOVE LIST...). Moved around
	// bit for bit, the menu owns the holder and deletes it through its vtable
	struct alignas(8) FPauseEntry
	{
		FSettingsTab Tab; // 0x00
		void* PanelHolder; // 0x20 Keeps the panel object alive, the object is at +0x10
	}; // Total Size: 0x28
	constexpr uint32_t PanelHolderObjectOffset = 0x10;
	constexpr size_t PanelHolderSize = 0x18;
	constexpr uint32_t PauseEntriesOffset = 0x240; // In the pause menu, the list of FPauseEntry
	constexpr uint32_t PauseMenuListOffset = 0x98; // In the pause menu, its entry list (PauseSelectedIndex reads it)

	// The settings screen, only the fields we use are named
	class USettingsScreen
	{
	public:
		char Unknown0[0x3A4];
		int32_t FocusedRow; // 0x3A4 Index in the page showing
		char Unknown3A8[0x439 - 0x3A8];
		bool bOpen; // 0x439 Set while the screen is up
		char Unknown43A[0x6A9 - 0x43A];
		bool bNoRowFocusA; // 0x6A9 The game's page switch only gives the rows focus while these three are clear
		bool bNoRowFocusB; // 0x6AA
		char Unknown6AB[0x6AC - 0x6AB];
		int32_t CurrentPage; // 0x6AC The page (and tab) on screen
		char Unknown6B0[0x6B1 - 0x6B0];
		bool bNoRowFocusC; // 0x6B1
		char Unknown6B2[0x6F8 - 0x6B2];
		TArray<FSettingsPage> Pages; // 0x6F8
		TArray<FString> PageDescriptions; // 0x708 Text keys in the Settings table, one per page
	};


	// Vars
	extern uint64_t*									lpGameVersion;
	extern uint64_t*									lpGameVersionFull;
	extern UNameTableStruct*							UNameTable;
	extern UNameTableMainStruct*						UMainNameTable;
	extern uint8_t*									lpEngineExitRequested; // The main loop ends once it's set
	extern uint64_t*									lpGamePhaseOwner; // Pointer to the object holding the game phase byte
	constexpr uint32_t								GamePhaseOffset = 0xA98;

	// Functions
	std::string GetGameVersion();
	std::string GetGameVersionFull();

	FName* NameTableIndexToFName(FNameInfoStruct*);
	FName* NameTableIndexToFName(uint16_t NameTableId, uint16_t NameOffset);

	// Game Functions
	// FString
	typedef			void							(__fastcall FNameToWStrType)					(FName&, char*);
	typedef			__int64							(__fastcall	InitializeNameTableType)			(UNameTableStruct&);
	// JSONEndpoint
	typedef			wchar_t**						(__fastcall	GetEndpointKeyValueType)			(JSONEndpointValue&, wchar_t**);
	// FightMetadata
	typedef			FKlassicLadderSecretFightData*	(__fastcall	SetupSecretFightConditionsType)		(FKlassicLadderSecretFightData*);
	typedef			TArray<uint32_t>*				(__fastcall GenerateFloydCluesFromHashType)		(TArray<uint32_t>*, uint64_t, uint64_t, uint64_t);
	typedef         uint32_t						(__fastcall CustomCityHashType)					(wchar_t**);
	typedef			__int64							(__fastcall UpdateTacoScheduleType)				(UTacoTowerSchedule*, int64_t NowTicks);
	typedef			wchar_t*						(__fastcall MKWScanfType)						(wchar_t* ResultString, const wchar_t* Format, ...);
	// Version
	typedef			FString*						(__fastcall GetNRSClientVersionType)			();
	typedef			const wchar_t*					(__fastcall GetBuildVersionType)				();
	typedef			const wchar_t*					(__fastcall GetBuildDateType)					();
	typedef			int								(__fastcall GetChangelistType)					();
	// Curl
	typedef			__int64							(__fastcall CurlSetOptType)						(__int64 handle, __int64 option, __int64 value);
	typedef			__int64							(__fastcall CurlMultiAddHandleType)				(__int64 multiHandle, __int64 easyHandle);
	typedef			__int64							(__fastcall CurlMultiInfoReadType)				(__int64 multiHandle, __int64* msgsInQueue);
	// Text
	typedef			FText*							(__fastcall TextFromStringType)					(FText* Out, FString* String);
	typedef			FText*							(__fastcall FindTextType)						(FText* Out, const wchar_t* Namespace, const wchar_t* Key);
	// Message Box
	typedef			UIMessageBox*					(__fastcall MessageBoxConstructType)			(UIMessageBox* Box, void* Player, FText* Body, char Style);
	typedef			void							(__fastcall MessageBoxSetTitleType)				(UIMessageBox* Box, FText* Title);
	typedef			void							(__fastcall MessageBoxAddButtonType)			(UIMessageBox* Box, int32_t Index, FText* Label, int32_t Input, FMessageBoxDelegate* OnPick, FString* Id);
	typedef			void							(__fastcall MessageBoxShowType)					(UIMessageBox* Box);
	typedef			void							(__fastcall MessageBoxCloseType)				(UIMessageBox* Box, bool bWait, bool bUnknown); // bWait must stay false off the game's own fibers
	typedef			void							(__fastcall MessageBoxDestructType)				(UIMessageBox* Box);
	typedef			void*							(__fastcall GetLocalPlayerInfoType)				(int32_t Index); // Its player sits at +0x38
	typedef			void*							(__fastcall ToastDriverType)						(); // Null unless the main menu is the top mode
	// bWithButtons: ACCEPT (A) and DECLINE (B) prompts, the press runs that delegate; C runs when it times out unpressed.
	// Takes over and releases both texts and the three delegates
	typedef			void							(__fastcall ToastEnqueueType)					(void* Driver, FText* Title, FText* Subtitle, bool bWithButtons, int32_t DurationFrames, uint8_t Priority,
		FMenuEntryCallback* CallbackA, FMenuEntryCallback* CallbackB, FMenuEntryCallback* CallbackC);
	typedef			void*							(__fastcall ReallocType)						(void* Memory, size_t Size, uint32_t Alignment); // The game's allocator, size 0 frees
	typedef			void							(__fastcall PlayerInputUpdateType)				(FPlayerInput* Input, void* Sample); // Once per frame per player, before anything reads presses
	typedef			bool							(__fastcall PlayerInputPressedType)				(void* Player, int32_t Input, FString* Id, uint32_t Flags); // Screens read one player's presses through it
	// Game State
	typedef			__int64							(__fastcall FrameTickType)						(); // Called once per frame inside the engine's frame, on the game thread
	typedef			void*							(__fastcall GetModeManagerType)					(void* WorldContext); // Null before the game has a world
	typedef			bool							(__fastcall IsModeOnStackType)					(void* ModeManager, void* ModeClass);
	typedef			void*							(__fastcall GameModeOnlineClassType)				();
	// Settings Screen
	typedef			__int64							(__fastcall SettingsBuildTabsType)				(USettingsScreen* Screen, bool bOnline); // Runs right after the pages are added
	typedef			void							(__fastcall SettingsCopyTabsType)				(void* Out, FSettingsTab* Tabs, int32_t Count, int32_t ExtraA, int32_t ExtraB);
	typedef			FMenuEntry*						(__fastcall MenuEntryConstructType)				(FMenuEntry* Entry, const wchar_t* Key, FMenuEntryCallback* OnPick);
	typedef			FSettingsTab*					(__fastcall SettingsTabConstructType)				(FSettingsTab* Tab, FMenuEntry* Entry, int32_t Order);
	typedef			void							(__fastcall SettingsTabDestructType)				(FSettingsTab* Tab);
	typedef			void							(__fastcall GrowArrayType)						(void* Array); // TArray of 16 byte elements, after its count was bumped
	typedef			void							(__fastcall StringReserveType)					(FString* String, int32_t Count); // Game memory for Count characters, Count stays as is
	typedef			void							(__fastcall SettingsRunType)						(void* Screen, void* Player, int64_t Mode); // Builds the settings screen and runs it until the player backs out
	typedef			FString*						(__fastcall SettingsVersionTextType)				(); // The version shown at the bottom left of the settings screen
	typedef			void							(__fastcall MemoryFreeType)						(void* Memory); // Frees the game's own allocations
	typedef			void							(__fastcall SettingsScreenStepType)				(USettingsScreen* Screen); // Steps of the game's page switch, see SwitchPage
	typedef			bool							(__fastcall SettingsFocusRowsType)				(USettingsScreen* Screen, bool bFirst); // Page switch: the rows take the keyboard and controller again
	typedef			__int64							(__fastcall FooterBuildType)					(USettingsScreen* Screen, TArray<FText>* Labels, void* FocusedRow, bool bNarrate, TArray<uint8_t>* Bits); // The footer prompts, Bits holds each one's input bit number
	typedef			FText*							(__fastcall PromptTextType)						(FText* Out, uint32_t InputMask, FText* Label); // "<key> LABEL", the key as the player has it bound
	typedef			void							(__fastcall SettingsPageSetupType)				(USettingsScreen* Screen, void* Player, int64_t Mode); // Fills the pages for a mode (5 main menu settings, 2 pause KONTROLS)
	typedef			void							(__fastcall PauseCollectEntriesType)			(void* Source, void* Player, TArray<FPauseEntry>* Out); // Fills the pause menu's entries, sorted
	typedef			void							(__fastcall GrowPauseEntriesType)				(TArray<FPauseEntry>* Array, int32_t OldCount); // After its count was bumped
	typedef			void*							(__fastcall GameAllocType)						(size_t Size);
	typedef			void*							(__fastcall PanelHolderConstructType)			(void* Memory, void* Object);
	typedef			int32_t							(__fastcall PauseSelectedIndexType)				(void* MenuList, int32_t Player); // The highlighted entry
	typedef			void							(__fastcall PauseCloseType)						(void* Pause); // The pause menu closing: its panels shut, its entries go
	typedef			uint8_t*						(__fastcall FloydSetLevelType)					(uint8_t* AI, uint8_t Level); // Floyd's fight: the AI level of each of his fighters (AI + 1)
	// Main menu, Living Towers
	typedef			bool							(__fastcall IsModeOnTopType)					(void* ModeManager, void* ModeClass); // The attract mode idle timer restarts while it's false
	typedef			char							(__fastcall TowersSetupType)					(void* TacoSchedule); // Sets up the towers as Living Towers loads, the schedule sits in the map mode
	typedef			int32_t							(__fastcall GetFloydVictoriesType)				(void* Profile); // Profile stat 9005, the game has no Floyd Tower below 1
	typedef			void*							(__fastcall GetStatsContainerType)				(void* Profile);
	typedef			uint32_t						(__fastcall GetProfileStatType)					(void* Stats, uint8_t ScopeMask, int32_t StatId); // Scope 2, ids from MKProfileStatID
	typedef			uint64_t						(__fastcall GetProfileStatBitsType)				(void* Stats, uint8_t ScopeMask, int32_t StatId); // Scope 8, the stats that are bit sets
	typedef			uint32_t						(__fastcall FloydProfileHashType)				(void** Profile); // Hash of the profile's online ids, Floyd's challenges are picked from it
	// Window focus
	typedef			void							(__fastcall FocusLostStepType)					(); // First step of the game's focus lost handler
	typedef			void							(__fastcall FocusLostPauseType)					(void* Mode); // Pauses with the "paused" message
	typedef			bool							(__fastcall BootScreenWaitType)					(int32_t FirstBootFrames, int32_t Frames, int32_t Counter); // True once a logo or legal screen waited long enough
	typedef			bool							(__fastcall PlayMovieType)						(const wchar_t* Name, int32_t A, int32_t B, int32_t C, int32_t D, void* Out); // False: it didn't start
	typedef			bool							(__fastcall CommandLineParamType)				(const wchar_t* CommandLine, const wchar_t* Param); // FParse::Param
	// Engine config
	typedef			void							(__fastcall InitializeConfigSystemType)			(); // Loads the engine's ini files into its config
	typedef			bool							(__fastcall LoadFileToStringType)				(FString* Out, const wchar_t* FileName, uint8_t VerifyFlags, uint32_t ReadFlags); // Through the game's file system, paks included
	// Announcer
	typedef			__int64							(__fastcall AnnouncerRerollType)					(void* AudioSettings); // Re-rolls a random announcer, ladder fights
	typedef			__int64							(__fastcall AnnouncerSetterType)					(void* AudioSettings, FString* ItemId); // The player's setting, only owned announcers
	typedef			void							(__fastcall ApplyAnnouncerType)					(uint64_t Key, uint64_t* ItemName, bool bUnknown); // Key 0 is the game's own
	typedef			uint64_t*						(__fastcall EngineNameConstructType)				(uint64_t* Out, const wchar_t* Text, int32_t FindType); // An engine name from text, FindType 1 adds it
	typedef			void*							(__fastcall GetCurrentMissionType)				(void* WorldContext);
	typedef			void*							(__fastcall GetTeamType)							(void* GameInfo, int32_t Team);
	typedef			void*							(__fastcall GetTeamDefinitionsType)				(void* Team);
	typedef			void*							(__fastcall MapModeGetProfileType)				(void* MapMode, int32_t Index, bool bLocal);
	// Paks
	typedef			__int64							(__fastcall PakMountType)						(void* PakPlatformFile, const wchar_t* PakFilename, uint32_t PakOrder, const wchar_t* MountPath, bool bLoadIndex); // Mounts the pak and its IoStore container
	// ReCreated
	namespace Remake {
		uint64_t	__fastcall	FNameInfoToWString(FNameInfoStruct* FNameInfo, char* Destination); // Common between 1 & 2
		uint64_t	__fastcall	FNameObjectToWStringCommon(FName* Name, char* Destination); // Common between all
		uint64_t	__fastcall	FNameInfoToWStringNoId(FNameInfoStruct* FNameInfo, char* Destination); // 1
		uint64_t	__fastcall	FNameInfoToWStringWithId(FNameInfoStruct* FNameInfo, char* Destination); // 2
		uint64_t	__fastcall	FNameObjectToWString(FName* Name, char* Destination); // 3 // Used for Proxy
		bool		__fastcall	GetArgBoolByNameWrapper(uint64_t* thisPtr, wchar_t* ArgName);
		bool		__fastcall	GetArgBoolByName(uint64_t *thisPtr, wchar_t* ArgName);

		namespace SecretFight {
			void PrintCharacterRequirements(FCharacterRequirementConditionHelper*);
			void PrintCharTypeRequirement(Enums::EFighterStatCheckMode&);
			void PrintComparisonType(Enums::EComparisonType&);
			void PrintRoundType(Enums::EFightingStatRoundToCheck&);

			void PrintFGStatID(Enums::FGStatID&);

			void PrintTypedConditions(FLadderSecretFightCondition_FightingStatCondition*);
			void PrintTypedConditions(FLadderSecretFightCondition_MultiFightingStatCondition*);
			void PrintTypedConditions(FLadderSecretFightCondition_ProfileStatCondition*);
			void PrintTypedConditions(FLadderSecretFightCondition_MoveTraceConditions*);
			//void PrintTypedConditions(FLadderSecretFightCondition_LadderLevelCondition*);
		}
	}

	// externs
	extern FNameToWStrType*								FNameToWStr;
	extern InitializeNameTableType*						InitializeNameTable;
	extern uint64_t*									ReadFNameToWStrNoIdStart;
	extern uint64_t*									ReadFNameToWStrWithIdStart;
	extern uint64_t*									ReadFNameToWStrCommonStart;

	extern GetEndpointKeyValueType*						GetEndpointKeyValue;
	extern SetupSecretFightConditionsType*				SetupSecretFightConditions;
	extern GenerateFloydCluesFromHashType*				GenerateFloydCluesFromHash;
	extern CustomCityHashType*							CustomCityHash;
	extern UpdateTacoScheduleType*						UpdateTacoSchedule;
	extern MKWScanfType*								MKWScanf;
	extern GetNRSClientVersionType*						GetNRSClientVersion;
	extern GetBuildVersionType*							GetBuildVersion;
	extern GetBuildDateType*							GetBuildDate;
	extern GetChangelistType*							GetChangelist;
	extern CurlSetOptType*								CurlSetOpt;
	extern CurlMultiAddHandleType*						CurlMultiAddHandle;
	extern CurlMultiInfoReadType*						CurlMultiInfoRead;
	extern TextFromStringType*							TextFromString;
	extern FindTextType*								FindText;
	extern MessageBoxConstructType*						MessageBoxConstruct;
	extern MessageBoxSetTitleType*						MessageBoxSetTitle;
	extern MessageBoxAddButtonType*						MessageBoxAddButton;
	extern MessageBoxShowType*							MessageBoxShow;
	extern MessageBoxCloseType*							MessageBoxClose;
	extern MessageBoxDestructType*						MessageBoxDestruct;
	extern PlayerInputPressedType*						PlayerInputPressed;
	extern PlayerInputUpdateType*						PlayerInputUpdate;
	extern ToastDriverType*								ToastDriver;
	extern ToastEnqueueType*							ToastEnqueue;
	extern ReallocType*									Realloc;
	extern GetLocalPlayerInfoType*						GetLocalPlayerInfo;
	extern FMessageBoxCallbackParts						MessageBoxCallback;
	constexpr uint32_t								LocalPlayerInfoPlayerOffset = 0x38;
	extern FrameTickType*								FrameTick;
	extern GetModeManagerType*							GetModeManager;
	extern IsModeOnStackType*							IsModeOnStack;
	extern GameModeOnlineClassType*						GameModeOnlineClass;
	extern SettingsBuildTabsType*						SettingsBuildTabs;
	extern SettingsCopyTabsType*						SettingsCopyTabs;
	extern MenuEntryConstructType*						MenuEntryConstruct;
	extern SettingsTabConstructType*					SettingsTabConstruct;
	extern SettingsTabDestructType*						SettingsTabDestruct;
	extern GrowArrayType*								GrowArray;
	extern StringReserveType*							StringReserve;
	extern MemoryFreeType*								MemoryFree;
	extern SettingsScreenStepType*						SettingsUnfocusRow;
	extern SettingsScreenStepType*						SettingsShowPage;
	extern SettingsScreenStepType*						SettingsRefreshFooter;
	extern SettingsFocusRowsType*						SettingsFocusRows;
	extern FooterBuildType*								FooterBuild;
	extern PromptTextType*								PromptText;
	extern SettingsPageSetupType*						PausePanelPageSetup; // The call that sets up the pause menu's KONTROLS panel
	extern PauseCollectEntriesType*						PauseCollectEntries;
	extern GrowPauseEntriesType*						GrowPauseEntries;
	extern GameAllocType*								GameAlloc;
	extern PanelHolderConstructType*					PanelHolderConstruct;
	extern PauseSelectedIndexType*						PauseSelectedIndex;
	extern PauseCloseType*								PauseClose;
	extern FloydSetLevelType*							FloydSetLevel;
	// One of the game's own settings rows, found by its title key and kind. They are static objects, there from startup
	FSettingsOption*									FindStaticOption(const wchar_t* Key, int32_t Type);
	extern SettingsVersionTextType*						SettingsVersionText;
	extern SettingsRunType*								SettingsRun;
	extern SettingsCopyTabsType*						MainMenuCopyTabs; // Same copy, the call that builds the main menu's entries
	extern PakMountType*								PakMount;
	extern IsModeOnTopType*								IsModeOnTop;
	extern TowersSetupType*								TowersSetup;
	extern GetFloydVictoriesType*						GetFloydVictories;
	extern GetStatsContainerType*						GetStatsContainer;
	extern GetProfileStatType*							GetProfileStat;
	extern GetProfileStatBitsType*						GetProfileStatBits;
	extern FloydProfileHashType*						FloydProfileHash;
	extern FocusLostStepType*							FocusLostStep;
	extern FocusLostPauseType*							FocusLostPause;
	extern BootScreenWaitType*							BootScreenWait;
	extern PlayMovieType*								PlayMovie;
	extern CommandLineParamType*						CommandLineParam;
	extern InitializeConfigSystemType*					InitializeConfigSystem;
	extern LoadFileToStringType*						LoadFileToString;
	extern AnnouncerRerollType*							AnnouncerReroll;
	extern AnnouncerSetterType*							AnnouncerSetter;
	extern ApplyAnnouncerType*							ApplyAnnouncer;
	extern EngineNameConstructType*						EngineNameConstruct;
	extern GetCurrentMissionType*						GetCurrentMission;
	extern GetTeamType*									GetTeam;
	extern GetTeamDefinitionsType*						GetTeamDefinitions;
	constexpr uint8_t								ProfileStatScope = 2;
	constexpr uint8_t								ProfileStatBitsScope = 8;
	constexpr uint32_t								StatSetSlot = 0x60 / 8; // Virtual, on a stat object: sets its value
	// Writes a stat in this PC's copy of the profile only. The game's own setter also marks the stat changed, which sends
	// it to the server after the next match; this one doesn't, so the server copy only changes when the game writes it
	bool											SetProfileStatLocal(void* Stats, uint8_t ScopeMask, int32_t StatId, uint64_t Value);
	constexpr uint32_t								MapModeTacoScheduleOffset = 0x368;
	constexpr uint32_t								MapModeGetProfileSlot = 0x2D0 / 8; // Virtual
	void*											MapModeGetProfile(void* MapMode); // The local player's profile, null without one
	extern FindTextType*								FindTextPlatform; // FindText that tries the platform's own wording first

}