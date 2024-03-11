#pragma once
#include <cppython.h>
#include <map>

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
	typedef			__int64		(__fastcall ReadFStringType)		(__int64, __int64);
	typedef			void		(__fastcall FNameToWStrType)		(FName&, char*);
	typedef			__int64		(__fastcall	InitializeNameTableType)(UNameTableStruct&);
	// JSONEndpoint
	typedef			wchar_t**	(__fastcall	GetEndpointKeyValueType)(JSONEndpointValue, wchar_t*&);
	// ReCreated
	namespace Remake {
		uint64_t	__fastcall	FNameInfoToWString(FNameInfoStruct* FNameInfo, char* Destination); // Common between 1 & 2
		uint64_t	__fastcall	FNameObjectToWStringCommon(FName* Name, char* Destination); // Common between all
		uint64_t	__fastcall	FNameInfoToWStringNoId(FNameInfoStruct* FNameInfo, char* Destination); // 1
		uint64_t	__fastcall	FNameInfoToWStringWithId(FNameInfoStruct* FNameInfo, char* Destination); // 2
		uint64_t	__fastcall	FNameObjectToWString(FName* Name, char* Destination); // 3 // Used for Proxy
		bool		__fastcall	GetArgBoolByNameWrapper(uint64_t* thisPtr, wchar_t* ArgName);
		bool		__fastcall	GetArgBoolByName(uint64_t *thisPtr, wchar_t* ArgName);
	}

	// externs
	extern ReadFStringType*								ReadFString;
	extern FNameToWStrType*								FNameToWStr;
	extern InitializeNameTableType*						InitializeNameTable;
	extern uint64_t*									ReadFNameToWStrNoIdStart;
	extern uint64_t*									ReadFNameToWStrWithIdStart;
	extern uint64_t*									ReadFNameToWStrCommonStart;

	extern GetEndpointKeyValueType*						GetEndpointKeyValue;
}