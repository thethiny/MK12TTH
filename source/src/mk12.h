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

	// Structs
	struct FName {
		uint16_t	NameSize;
		char		Name[];
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


	namespace FNameFunc {
		inline uint16_t	GetSize(FName& F);
		inline char*	GetName(FName& F);
		void			Print(FName& F);
		char*			ToStr(FName& F);
		FName*			FromStr(const char* string);
	}

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

		struct HTTPHeaderInstance
		{
			char*		GameVersion;
			uint64_t*	OtherHeaderStuff;
			uint64_t	PAD;
			uint32_t	UNK;
			uint16_t	UNK2[2];
			char		RemainderOfHeader[4]; // it's a string

		};

		struct HTTPHeaderContainer
		{
			uint64_t HeaderSize;
			HTTPHeaderInstance* Instance;
		};

		struct HTTPPostStruct
		{
			HTTPHeaderContainer* HeaderContainer; //0
			char* ResponseBody; //8
			int32_t ResponseBodySize; //10
			int32_t UnknownSize; //14
			char* ResponseStatus; //18
			wchar_t* Unknown; //20
			char* Body; //28
			int32_t BodySize; //30
			int32_t Unknown2; //30
			uint64_t Unknown3;
			uint64_t Unknown4;
			char* HTTPEndpoint;
			uint32_t HTTPEndpointSize;
			uint32_t Unknown2Copy;
			uint64_t Ignore[18];
			char* Unk;
			uint64_t UnkSize;
			char* Platform;
			uint64_t PlatformSize;
			char* Unk2;
			uint64_t Unk2Size;
			char* Platform2;
			uint64_t Platform2Size;
			uint64_t Ignore3[4];
			char* MainEndpoint;
			uint64_t Ignore4;
			char* Endpoint;
			int32_t EndpointSize;
		};

		struct HTTPResponseStruct
		{
			uint64_t* ClassPointer;
			char* ResponseBody;
			uint32_t Sizes[2];
			char* ResponseStatus;
			wchar_t* Unknown;
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

	
	// Vars
	extern uint8_t										ulCharactersCount;
	extern uint64_t*									lpGameVersion;
	extern uint64_t*									lpGameVersionFull;
	extern std::vector<std::wstring>					vSwappedFiles;
	extern std::map<uint64_t, CURL::HTTPPostStruct*>	CurlObjectMap;
	extern UNameTableStruct*							UNameTable;
	extern UNameTableMainStruct*						UMainNameTable;

	// Functions
	std::string GetGameVersion();
	std::string GetGameVersionFull();

	FName* NameTableIndexToFName(FNameInfoStruct*);
	FName* NameTableIndexToFName(uint16_t NameTableId, uint16_t NameOffset);

	// Game Functions
	// FString
	typedef			__int64							(__fastcall ReadFStringType)					(__int64, __int64);
	typedef			void							(__fastcall FNameToWStrType)					(FName&, char*);
	typedef			__int64							(__fastcall	InitializeNameTableType)			(UNameTableStruct&);
	// JSONEndpoint
	typedef			wchar_t**						(__fastcall	GetEndpointKeyValueType)			(JSONEndpointValue&, wchar_t**);
	// FightMetadata
	typedef			FKlassicLadderSecretFightData*	(__fastcall	SetupSecretFightConditionsType)		(FKlassicLadderSecretFightData*);
	typedef			TArray<uint32_t>*				(__fastcall GenerateFloydCluesFromHashType)		(TArray<uint32_t>*, uint64_t, uint64_t, uint64_t);
	typedef         uint32_t						(__fastcall CustomCityHashType)					(wchar_t**);
	typedef			wchar_t*						(__fastcall MKWScanfType)						(wchar_t* ResultString, const wchar_t* Format, ...);
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
	extern ReadFStringType*								ReadFString;
	extern FNameToWStrType*								FNameToWStr;
	extern InitializeNameTableType*						InitializeNameTable;
	extern uint64_t*									ReadFNameToWStrNoIdStart;
	extern uint64_t*									ReadFNameToWStrWithIdStart;
	extern uint64_t*									ReadFNameToWStrCommonStart;

	extern GetEndpointKeyValueType*						GetEndpointKeyValue;
	extern SetupSecretFightConditionsType*				SetupSecretFightConditions;
	extern GenerateFloydCluesFromHashType*				GenerateFloydCluesFromHash;
	extern CustomCityHashType*							CustomCityHash;
	extern MKWScanfType*								MKWScanf;

}