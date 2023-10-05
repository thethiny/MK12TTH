#pragma once
#include <cppython.h>

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

enum MOD_HOOK_STATUS
{
	DISABLED					= -1,
	SUCCESS						,
	NOT_SPECIFIED				,
	NOT_FOUND					,
};

namespace MK12 {

	// Structs
	/*struct IntroStruct {
		char PName[100]				{ 0 };
		char PName2[100]			{ 0 };
		char PChar[2]				{ 0 };
		char PChar2[2]				{ 0 };
		bool bEnabled				= false;
	};

	struct CharacterStruct {
		std::string name;
		uint8_t intros=0;
	};*/

	struct ActiveMods {
		//bool bAntiCheatEngine		= false;
		bool bAntiSigCheck			= false;
		bool bAntiChunkSigCheck		= false;
		bool bAntiSigWarn			= false;
		bool bAntiTocSigCheck		= false;
		//bool bModLoader				= false;
		//bool bCurl					= false;
	};

	/*struct CheatsStruct {
		uint64_t* lpMercy, * lpGround, * lpBrut, * lpBrutB, * lpMeteor, * lpDizzy, *lpFatality, *lpFatCombo, *lpNoBlock, *lpFatalBlow;
		bool bMercy = false, bGround = false, bBrut = false, bBrutB = false, bMeteor = false, bDizzy = false, bFatality = false, bFatCombo = false, bNoBlock = false, bFatalBlow = false;
	};*/

	struct LibFuncStruct {
		std::string FullName;
		std::string LibName;
		std::string ProcName;
		bool bIsValid = false;
	};

	struct LibMapsStruct {
		LibFuncStruct ModLoader;
		LibFuncStruct AntiCheatEngine;
		LibFuncStruct CurlSetOpt;
		LibFuncStruct CurlPerform;
	};

	struct HTTPHeaderInstance
	{
		char* GameVersion;
		uint64_t* OtherHeaderStuff;
		uint64_t PAD;
		uint32_t UNK;
		uint16_t UNK2[2];
		char RemainderOfHeader[4]; // it's a string

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

	struct UserKeysStruct
	{
		PyString SteamKeyBody		= ""; // Auth Token
		PyString AuthTokenBody		= ""; // Send to Access Token
		PyString AuthTokenResponse	= ""; // Access Token
	};

	struct GameReadyState
	{
		PyString szGameVersion	= "";
		bool bSteamKey			= false;
		bool bAuthToken			= false;
		bool bAccessToken		= false;
		bool bUsername			= false;
		bool bSteamID			= false;
		bool bGameVersion		= false;
		bool bHash				= false;
		bool bUnlock			= true;
	};

	
	// Vars
	//extern std::vector<CharacterStruct>			sCharacters;
	extern uint8_t								ulCharactersCount;
	extern uint64_t*							lpGameVersion;
	extern uint64_t*							lpGameVersionFull;
	extern std::vector<std::wstring>			vSwappedFiles;
	extern LibMap								IAT;
	extern std::map<uint64_t, HTTPPostStruct*>	CurlObjectMap;
	// StructVars
	//extern IntroStruct							sIntroStruct;
	//extern IntroStruct							sIntroStruct2;
	extern ActiveMods							sActiveMods;
	//extern CheatsStruct							sCheatsStruct;
	extern LibMapsStruct						sLFS;
	extern UserKeysStruct						sUserKeys;
	extern GameReadyState						sGameState;

	

	// Functions
	void DummyFunc();
	uint64_t TrueFunc();
	uint64_t FalseFunc();
	std::string GetGameVersion();
	std::string GetGameVersionFull();
	void PopulateCharList();
	//bool operator==(const CharacterStruct& s1, std::string s2);
	// LFS
	LibFuncStruct ParseLibFunc(CPPython::string);
	void ParseLibFunc(LibFuncStruct&);
	uint64_t* GetLibProcFromNT(const LibFuncStruct&);
	void PrintErrorProcNT(const LibFuncStruct& LFS, uint8_t bMode);
}