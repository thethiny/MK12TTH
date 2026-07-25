#include "mk12hook.h"
#include "GamePatchManager.h"


HookMetadata::ActiveMods			HookMetadata::ActiveModsMap;
HookMetadata::CheatsStruct			HookMetadata::sCheatsStruct;
HookMetadata::LibMapsStruct			HookMetadata::sLFS;
HookMetadata::UserKeysStruct		HookMetadata::sUserKeys;
HookMetadata::GameReadyState		HookMetadata::sGameState;
HookMetadata::SwapTable				HookMetadata::FSwapTable;
// Thread safety note: FSwapTable is only written at startup (AnnouncerSwap/StringSwaps) and read-only after.
// FloydCluesInfo and UserProfileInfo are written by NRS-to-UE proxies which are single-threaded within NRS's caller.
// If additional hooks are added that write to these from different call sites, add synchronization.
HookMetadata::FloydCluesInfo		HookMetadata::CurrentFloydInfo;
HookMetadata::ProfileInfo			HookMetadata::UserProfileInfo;



namespace MK12Hook::Proxies {

	void ReadFNameToWStrId(MK12::FName& Name, char* dest)
	{
		if (SettingsMgr->ShouldFlood(SettingsMgr->Floods.bFName))
		{
			printf("Proxy FNameToWStrId::Captured::");
			MK12::FNameFunc::Print(Name);
		}

		MK12::FName *current_fname = ReadFNameToWStr(Name, dest);

		RegisterHacks::MoveToRBX(MK12::FNameFunc::GetSize(*current_fname));
	}
	void ReadFNameToWStrNoId(MK12::FName& Name, char* dest)
	{
		if (SettingsMgr->ShouldFlood(SettingsMgr->Floods.bFName))
		{
			printf("Proxy FNameToWStrNoId::Captured::");
			MK12::FNameFunc::Print(Name);
		}

		MK12::FName *current_fname = ReadFNameToWStr(Name, dest);

		RegisterHacks::MoveToRBX((uint64_t)&current_fname->NameSize);
	}

	void ReadFNameToWStrCommon(MK12::FName& Name, char* dest)
	{
		if (SettingsMgr->ShouldFlood(SettingsMgr->Floods.bFName))
		{
			printf("Proxy FNameToWStrCommon::Captured::");
			MK12::FNameFunc::Print(Name);
		}

		MK12::FName* current_fname = ReadFNameToWStr(Name, dest);

		RegisterHacks::MoveToRBX((uint64_t)&current_fname->NameSize); // Needs testing
	}

	MK12::FName* ReadFNameToWStr(MK12::FName &Name, char* dest)
	{
		const char* name = MK12::FNameFunc::ToStr(Name);
		if (SettingsMgr->ShouldLog(Log::Debug) && containsCaseInsensitive(name, "skin007_pal004"))
		{
			printf("DEBUGME");
		}
		MK12::FName* swap = HookMetadata::FSwapTable.get(name);
		if (swap == nullptr)
		{
			MK12::FNameToWStr(Name, dest);
			return &Name;
		}

		if (SettingsMgr->ShouldLog(Log::Verbose))
		{
			printf("Swap To::");
			MK12::FNameFunc::Print(*swap);
		}
		MK12::FNameToWStr(*swap, dest);

		return swap;
	}

	wchar_t** OverrideGameEndpoint(MK12::JSONEndpointValue obj, wchar_t** EndpointAddress)
	{
		static wchar_t* cachedServerUrl = nullptr;
		static int cachedServerLen = 0;

		if (HookMetadata::ActiveModsMap["bGameEndpointSwap"] && !SettingsMgr->szServerUrl.empty())
		{
			if (!cachedServerUrl)
			{
				CPPython::string ServerUrl = SettingsMgr->szServerUrl;
				ServerUrl = ServerUrl.strip("/");

#pragma warning(push)
#pragma warning(disable: 4267)
				int StringLength = (ServerUrl.size() + 1) * 2;
				std::wstring wServerUrl = std::wstring(ServerUrl.begin(), ServerUrl.end());

				cachedServerUrl = new wchar_t[StringLength];
				memcpy(cachedServerUrl, wServerUrl.c_str(), StringLength);
				cachedServerLen = ServerUrl.size() + 1;
#pragma warning(pop)

				wprintf(L"Replacing endpoint with \"%s\"!\n", wServerUrl.c_str());
			}

			obj.ValuePointer = cachedServerUrl;
			obj.ValueLength = cachedServerLen;
			obj.ValueLength8BAligned = (obj.ValueLength + 7) & (~7);

			MK12::GetEndpointKeyValue(obj, EndpointAddress);
			return &obj.ValuePointer;
		}

		return MK12::GetEndpointKeyValue(obj, EndpointAddress);

	}

	MK12::TArray<uint32_t>* GenerateFloydCluesFromHashProxy(MK12::TArray<uint32_t>* ArrayLocation, uint64_t ShuffleSeed, uint64_t unk, uint64_t unk2)
	{ // Use hash to create 10 challenges

		printf("Floyd Shuffle Seed: %llX\n", (uint64_t)ShuffleSeed);
		MK12::TArray<uint32_t>* ReturnedArrayLocation = MK12::GenerateFloydCluesFromHash(ArrayLocation, ShuffleSeed, unk, unk2); // 8 bytes location and 4 bytes count

		if (ReturnedArrayLocation && ReturnedArrayLocation->Data) {

			// Store the data
			uint32_t newUserProfileHash = (uint32_t)(ShuffleSeed >> 32);
			uint32_t newFloydEncounters = (uint32_t)(ShuffleSeed & 0xFFFFFFFF);

			if (newFloydEncounters > HookMetadata::CurrentFloydInfo.FloydEncounters)
			{
				HookMetadata::CurrentFloydInfo.Clues.clear();

				for (uint8_t i = 0; i < ReturnedArrayLocation->ElementsCount; i++)
				{
					HookMetadata::CurrentFloydInfo.Clues.push_back((uint8_t)ReturnedArrayLocation->Data[i] + 1);
				}

				HookMetadata::CurrentFloydInfo.UserProfileHash = newUserProfileHash;
				HookMetadata::CurrentFloydInfo.FloydEncounters = newFloydEncounters;
			}

			printf("Floyd Challenges: ");
			for (int32_t i = 0; i < ReturnedArrayLocation->ElementsCount; ++i) {
				printf("%i ", (uint32_t)ReturnedArrayLocation->Data[i]+1);
			}
			printf("\n");
		}
		else {
			printfError("Couldn't Read Floyd Challenges!\n");
		}

		return ReturnedArrayLocation;
	}

	uint32_t CustomCityHashProxy(wchar_t** StringToHash)
	{ // Hash the floyd string
		if (StringToHash && *StringToHash) {
			printf("StringToHash: %ls\n", *StringToHash); // %ls for wchar_t*

			CPPython::string str(*StringToHash);
			std::vector<CPPython::string> parts = str.split("/");

			if (!parts.empty() && !HookMetadata::UserProfileInfo.OfflineProfileId)
			{
				std::vector<CPPython::string> pipeParts = parts[0].split("|");

				if (pipeParts.size() >= 2)
				{
					delete[] HookMetadata::UserProfileInfo.OfflineProfileId;

					size_t len = pipeParts[1].size() + 1;
					HookMetadata::UserProfileInfo.OfflineProfileId = new wchar_t[len];
					mbstowcs(HookMetadata::UserProfileInfo.OfflineProfileId, pipeParts[1].c_str(), len);

					if (SettingsMgr->ShouldLog(Log::Verbose))
						printf("OfflineProfileId extracted: %ls\n", HookMetadata::UserProfileInfo.OfflineProfileId);
				}
				else
				{
					if (SettingsMgr->ShouldLog(Log::Verbose))
						printf("OfflineProfileId not found. Single User Profile only.\n");
				}
			}
			if (parts.size() == 2 && !HookMetadata::UserProfileInfo.HydraId)
			{
				delete[] HookMetadata::UserProfileInfo.HydraId;

				size_t len = parts[1].size() + 1;
				HookMetadata::UserProfileInfo.HydraId = new wchar_t[len];
				mbstowcs(HookMetadata::UserProfileInfo.HydraId, parts[1].c_str(), len);
				if (SettingsMgr->ShouldLog(Log::Verbose))
					printf("HydraId extracted: %ls\n", HookMetadata::UserProfileInfo.HydraId);
			}
			else if (parts.size() >= 3 && !HookMetadata::UserProfileInfo.WBId)
			{
				delete[] HookMetadata::UserProfileInfo.HydraId;
				delete[] HookMetadata::UserProfileInfo.WBId;

				size_t len1 = parts[1].size() + 1;
				size_t len2 = parts[2].size() + 1;

				HookMetadata::UserProfileInfo.HydraId = new wchar_t[len1];
				HookMetadata::UserProfileInfo.WBId = new wchar_t[len2];

				mbstowcs(HookMetadata::UserProfileInfo.HydraId, parts[1].c_str(), len1);
				mbstowcs(HookMetadata::UserProfileInfo.WBId, parts[2].c_str(), len2);

				if (SettingsMgr->ShouldLog(Log::Verbose))
				{
					printf("HydraId extracted: %ls\n", HookMetadata::UserProfileInfo.HydraId);
					printf("WBId extracted: %ls\n", HookMetadata::UserProfileInfo.WBId);
				}

			}
			else {
				if (SettingsMgr->ShouldLog(Log::Verbose))
					printf("Offline Mode Floyd String Hash Detected!");
			}
		}
		else
		{
			printfError("StringToHash Is Empty!\n");
		}
		
		uint32_t HashResult = MK12::CustomCityHash(StringToHash);
		printf("HashResult: %X\n", (uint32_t)HashResult);

		return HashResult;
	}


	// Hooks a specific NRS call site that always passes exactly 3 wchar_t* args (Platform, PlatformId, SaveKey)
	wchar_t* MKWScanfProxyForCrypto(wchar_t* ResultString, const wchar_t* Format, ...)
	{
		va_list args;
		va_start(args, Format);

		wchar_t* arg0 = va_arg(args, wchar_t*);
		wchar_t* arg1 = va_arg(args, wchar_t*);
		wchar_t* arg2 = va_arg(args, wchar_t*);

		va_end(args);

		auto SafeStringCopy = [](const wchar_t* src) -> wchar_t* {
			if (!src) return nullptr;
			size_t len = wcslen(src) + 1;
			wchar_t* copy = new wchar_t[len];
			wcscpy(copy, src);
			return copy;
			};

		auto SafeWcsCmp = [](const wchar_t* a, const wchar_t* b) -> bool {
			if (!a && !b) return true;
			if (!a || !b) return false;
			return wcscmp(a, b) == 0;
			};

		if (!SafeWcsCmp(HookMetadata::UserProfileInfo.Platform, arg0) ||
			!SafeWcsCmp(HookMetadata::UserProfileInfo.PlatformId, arg1) ||
			!SafeWcsCmp(HookMetadata::UserProfileInfo.SaveKey, arg2))
		{
			delete[] HookMetadata::UserProfileInfo.Platform;
			delete[] HookMetadata::UserProfileInfo.PlatformId;
			delete[] HookMetadata::UserProfileInfo.SaveKey;

			HookMetadata::UserProfileInfo.Platform = SafeStringCopy(arg0);
			HookMetadata::UserProfileInfo.PlatformId = SafeStringCopy(arg1);
			HookMetadata::UserProfileInfo.SaveKey = SafeStringCopy(arg2);

			if (SettingsMgr->ShouldLog(Log::Verbose))
				printf("Stored Profile Info:\nPlatform: %ws\nPlatformId: %ws\nSaveKey: %ws\n", arg0, arg1, arg2);
		}

		// Now call real function with full argsCopy
		return MK12::MKWScanf(ResultString, Format, arg0, arg1, arg2);
	}



	MK12::FKlassicLadderSecretFightData* SetupSecretFightConditionsProxy(MK12::FKlassicLadderSecretFightData* mSecretFightData)
	{
 		MK12::UKlassicTowerSecretFightData* mKlassicTowerSecretFightInstance = (MK12::UKlassicTowerSecretFightData*)(((uint64_t)(mSecretFightData)) - MK12::KlassicTowerSecretFightDataOffset);
		if (SettingsMgr->ShouldLog(Log::Debug))
		{
			printf("SecretFightData @ %p\n", mSecretFightData);
			printf("KlassicTowerSecretFightData @ %p\n", mKlassicTowerSecretFightInstance);
		}

		MK12::FKlassicLadderSecretFightData* mSecretFightDataRet = MK12::SetupSecretFightConditions(mSecretFightData);

		if (!SettingsMgr->bSerializeSecretFights || !HookMetadata::ActiveModsMap["UNameTableGetter"])
			return mSecretFightData;

		if (mKlassicTowerSecretFightInstance)
		{
			MK12::FMissionBuildStep_KlassicTowerSecretFight& secretFightRules = mKlassicTowerSecretFightInstance->mSecretFightRulesBuildStep;

			printf("Floyd mDifficultyMaps: ");
			for (int i = 0; i < secretFightRules.mDifficultyMap.ElementsCount; i++) {
				printf("%02X ", secretFightRules.mDifficultyMap.Data[i]);
			}
			printf("\n");
		}


		char FightingStatCondition[]		= "LadderSecretFightCondition_FightingStatCondition";
		char MultiFightingStatCondition[]	= "LadderSecretFightCondition_MultiFightingStatCondition";
		char MoveTraceCondition[]			= "LadderSecretFightCondition_MoveTraceConditions";
		char ProfileStatCondition[]			= "LadderSecretFightCondition_ProfileStatCondition";

		// Start deserialization
		for (int i = 0; i < mSecretFightData->mConditions.ElementsCount; i++)
		{
			MK12::FKlassicLadderSecretFightCondition* CurrentCondition = MK12::TArrayHelpers::index<MK12::FKlassicLadderSecretFightCondition>(mSecretFightData->mConditions, i);
			MK12::FName* ScriptType = MK12::NameTableIndexToFName(&CurrentCondition->StructPtrType->ScriptClass);
			printf("Floyd Challenge %d - %s\n\tType %s\n", CurrentCondition->Index, MK12::EnumMaps::FloydChallengeMap[CurrentCondition->Index].c_str(), FNAME_STR(ScriptType));
			if (!CurrentCondition->enable)
			{
				printf("Disabled!");
				continue;
			}

			if (strncmp(ScriptType->Name, FightingStatCondition, MK12::FNameFunc::GetSize(*ScriptType)) == 0)
			{
				MK12::FLadderSecretFightCondition_FightingStatCondition* RetypedCondition = static_cast<MK12::FLadderSecretFightCondition_FightingStatCondition*>(CurrentCondition->StructPtrInstance);
				MK12::Remake::SecretFight::PrintTypedConditions(RetypedCondition);
			}
			else if (strncmp(ScriptType->Name, MultiFightingStatCondition, MK12::FNameFunc::GetSize(*ScriptType)) == 0)
			{
				MK12::FLadderSecretFightCondition_MultiFightingStatCondition* RetypedCondition = static_cast<MK12::FLadderSecretFightCondition_MultiFightingStatCondition*>(CurrentCondition->StructPtrInstance);
				MK12::Remake::SecretFight::PrintTypedConditions(RetypedCondition);
			}
			else if (strncmp(ScriptType->Name, MoveTraceCondition, MK12::FNameFunc::GetSize(*ScriptType)) == 0)
			{
				MK12::FLadderSecretFightCondition_MoveTraceConditions* RetypedCondition = static_cast<MK12::FLadderSecretFightCondition_MoveTraceConditions*>(CurrentCondition->StructPtrInstance);
				MK12::Remake::SecretFight::PrintTypedConditions(RetypedCondition);
			}
			else if (strncmp(ScriptType->Name, ProfileStatCondition, MK12::FNameFunc::GetSize(*ScriptType)) == 0)
			{
				MK12::FLadderSecretFightCondition_ProfileStatCondition* RetypedCondition = static_cast<MK12::FLadderSecretFightCondition_ProfileStatCondition*>(CurrentCondition->StructPtrInstance);
				MK12::Remake::SecretFight::PrintTypedConditions(RetypedCondition);
			}

			if (!CurrentCondition->CanCheckDuringFight)
			{
				printf("* Notification pops up after the match!\n");
			}
			printf("\n");
		}


		return mSecretFightDataRet;
	}
};

// TODO: Curl interception not yet implemented. Below code is scaffolding for future use.
std::map<int, const char*> CURL_MAP
{
	{46,	"CURLOPT_UPLOAD"},
	{47,	"CURLOPT_POST"},
	{10001,	"CURLOPT_WRITEDATA"},
	{10029,	"CURLOPT_HEADERDATA"},
	{10002,	"CURLOPT_URL"},
	{10004,	"CURLOPT_PROXY"},
	{10173,	"CURLOPT_USERNAME"},
	{10005,	"CURLOPT_USERPWD"},
	{10023,	"CURLOPT_HTTPHEADER"},
	{60,	"CURLOPT_POSTFIELDSIZE"},
	{20012,	"CURLOPT_READFUNCTION"},
	{10009,	"CURLOPT_READDATA"},
	{10010,	"CURLOPT_ERRORBUFFER"},
	{8,		"CURLPROTO_FTPS"},
	{10015,	"CURLOPT_POSTFIELDS"},
	{20011,	"CURLOPT_WRITEFUNCTION"},
	{10018,	"CURLOPT_USERAGENT"},
	{80,	"CURLOPT_HTTPGET"},
	{81,	"CURLOPT_SSL_VERIFYHOST"},
	{14,	"CURLOPT_INFILESIZE"},
	{64,	"CURLOPT_SSL_VERIFYPEER"},
	{99,	"CURLOPT_NOSIGNAL"},
	{41,	"CURLOPT_VERBOSE"},
	{42,	"CURLOPT_HEADER"},
	{43,	"CURLOPT_NOPROGRESS"},
	{10086,	"CURLOPT_SSLCERTTYPE"},
	{20079, "CURLOPT_HEADERFUNCTION"},
	{20108, "CURLOPT_SSL_CTX_FUNCTION"},
	{10065, "CURLOPT_CAINFO"},
	{10097, "CURLOPT_CAPATH"},

};

bool bFirstPost = true;

enum class ArgTypes {
	ARGTYPE_NONE = 0, // UNK
	ARGTYPE_STRING, // String Pointer
	ARGTYPE_AGBINARY, // Data
	ARGTYPE_FUNCTION, // Callback
	ARGTYPE_CANCEL, // Return 0
	ARGTYPE_SETZERO, // Arg3 becomes 0
	ARGTYPE_INT, // Integer
	ARGTYPE_STRUCT, // Struct Pointer
};

// TODO: Uses == pointer comparison instead of strcmp - fix when curl interception is implemented
ArgTypes GetArgType(const char* arg_type)
{
	if (arg_type == "CURLOPT_URL")
		return ArgTypes::ARGTYPE_STRING;
	if (arg_type == "CURLOPT_USERAGENT")
		return ArgTypes::ARGTYPE_STRING;
	if (arg_type == "CURLOPT_WRITEDATA")
		return ArgTypes::ARGTYPE_STRUCT;
	if (arg_type == "CURLOPT_USERNAME")
		return ArgTypes::ARGTYPE_STRUCT;
	if (arg_type == "CURLOPT_USERPWD")
		return ArgTypes::ARGTYPE_STRUCT;
	if (arg_type == "CURLOPT_HEADERDATA")
		return ArgTypes::ARGTYPE_AGBINARY;
	if (arg_type == "CURLOPT_READDATA")
		return ArgTypes::ARGTYPE_AGBINARY;
	if (arg_type == "CURLOPT_WRITEFUNCTION")
		return ArgTypes::ARGTYPE_FUNCTION;
	if (arg_type == "CURLOPT_READFUNCTION")
		return ArgTypes::ARGTYPE_FUNCTION;
	if (arg_type == "CURLOPT_SSL_VERIFYPEER")
		return ArgTypes::ARGTYPE_SETZERO;
	if (arg_type == "CURLOPT_SSL_VERIFYHOST")
		return ArgTypes::ARGTYPE_SETZERO;
	if (arg_type == "CURLOPT_INFILESIZE")
		return ArgTypes::ARGTYPE_INT;
	if (arg_type == "CURLOPT_POSTFIELDSIZE")
		return ArgTypes::ARGTYPE_INT;
	if (arg_type == "CURLOPT_CAPATH")
		return ArgTypes::ARGTYPE_STRING;
	if (arg_type == "CURLOPT_CAINFO")
		return ArgTypes::ARGTYPE_STRING;
	if (arg_type == "CURLOPT_HEADERFUNCTION")
		return ArgTypes::ARGTYPE_FUNCTION;
	if (arg_type == "CURLOPT_SSL_CTX_FUNCTION")
		return ArgTypes::ARGTYPE_FUNCTION;
	if (arg_type == "CURLOPT_SSLCERTTYPE")
		return ArgTypes::ARGTYPE_STRING;
	return ArgTypes::ARGTYPE_NONE;
}

// Hooks
namespace MK12Hook::Hooks {

	bool DisableSignatureCheck()
	{
		printf("\n==DisableSignatureCheck==\n");

		uint64_t patAddr = GamePatcher->ResolvePattern(SettingsMgr->pSigCheck, "SigCheck");
		if (!patAddr)
			return false;

		GamePatcher->PatchReturnAt(patAddr - 0x14, 5);
		printfSuccess("SigCheck Patched");

		return true;
	}

	bool DisableSignatureWarn()
	{
		printf("\n==DisableSignatureWarn==\n");

		uint64_t patAddr = GamePatcher->ResolvePattern(SettingsMgr->pSigWarn, "SigWarn");
		if (!patAddr)
			return false;

		GamePatcher->PatchConditionalToUnconditional(patAddr + 0xA);
		printfSuccess("SigWarn Patched");

		return true;
	}

	bool DisableChunkSigCheck()
	{
		printf("\n==DisablePakChunkSigCheck==\n");

		uint64_t patAddr = GamePatcher->ResolvePattern(SettingsMgr->pChunkSigCheck, "ChunkSigCheck");
		if (!patAddr)
			return false;

		uint64_t funcAddr = GamePatcher->ResolvePattern(SettingsMgr->pChunkSigCheckFunc, "ChunkSigCheckFunc");
		if (!funcAddr)
			return false;

		GamePatcher->RedirectCallTo(patAddr + 0xE, funcAddr);
		printfSuccess("PakChunkSigCheck Patched");

		return true;
	}

	bool DisableTOCSigCheck()
	{
		printf("\n==DisableTOCSigCheck==\n");

		uint64_t patAddr = GamePatcher->ResolvePattern(SettingsMgr->pTocCheck, "TocSigCheck");
		if (!patAddr)
			return false;

		GamePatcher->PatchConditionalToUnconditional(patAddr + 0x12);
		printfSuccess("TocSigCheck Patched");

		return true;
	}

	bool DisablePakTOCCheck()
	{
		printf("\n==DisablePakTOCCheck==\n");

		uint64_t patAddr = GamePatcher->ResolvePattern(SettingsMgr->pPakTocCheck, "PakTocCheck");
		if (!patAddr)
			return false;

		GamePatcher->PatchConditionalToUnconditional(patAddr + 0x12);
		printfSuccess("PakTocCheck Patched");

		return true;
	}

	bool FNameToStrWithIdLoader()
	{
		printf("\n==ProxyFPathIdLoader==\n");

		uint64_t patAddr = GamePatcher->ResolvePattern(SettingsMgr->pFPathLoadPat, "FPathLoad");
		if (!patAddr)
			return false;

		GamePatcher->ProxyCallAt(patAddr + 0xB, MK12Hook::Proxies::ReadFNameToWStrId, &MK12::FNameToWStr, PATCH_CALL);
		printfSuccess("FPathLoad Proxied");

		uint64_t func_start = patAddr - 0x6F;
		if (*(uint8_t*)func_start != 0x48)
		{
			printfError("FPathLoad Start not found!");
		}
		else
		{
			MK12::ReadFNameToWStrWithIdStart = (uint64_t*)func_start;
			if (SettingsMgr->ShouldLog(Log::Verbose))
				printf("FPathLoad Func Start is at: %p\n", MK12::ReadFNameToWStrWithIdStart);
		}

		return true;
	}

	bool FNameToStrNoIdLoader()
	{
		printf("\n==ProxyFPathNoIdLoader==\n");

		uint64_t patAddr = GamePatcher->ResolvePattern(SettingsMgr->pFPath2LoadPat, "FPath2Load");
		if (!patAddr)
			return false;

		GamePatcher->ProxyCallAt(patAddr + 0xB, MK12Hook::Proxies::ReadFNameToWStrNoId, &MK12::FNameToWStr, PATCH_CALL);
		printfSuccess("FPath2Load Proxied");

		uint64_t func_start = patAddr - 0x55;
		if (*(uint8_t*)func_start != 0x48)
		{
			printfError("FPath2Load Start not found!");
		}
		else
		{
			MK12::ReadFNameToWStrNoIdStart = (uint64_t*)func_start;
			if (SettingsMgr->ShouldLog(Log::Verbose))
				printf("FPath2Load Func Start is at: %p\n", MK12::ReadFNameToWStrNoIdStart);
		}

		return true;
	}

	bool FNameToStrCommonLoader()
	{
		printf("\n==ProxyFPathCommonLoader==\n");

		uint64_t patAddr = GamePatcher->ResolvePattern(SettingsMgr->pFPathCLoadPat, "FPathCLoad");
		if (!patAddr)
			return false;

		GamePatcher->ProxyCallAt(patAddr + 0x00, MK12Hook::Proxies::ReadFNameToWStrCommon, &MK12::FNameToWStr, PATCH_CALL);
		printfSuccess("FPathCLoad Proxied");

		uint64_t func_start = patAddr - 0x10;
		if (*(uint8_t*)func_start != 0x48)
		{
			printfError("FPathCLoad Start not found!");
		}
		else
		{
			MK12::ReadFNameToWStrCommonStart = (uint64_t*)func_start;
			if (SettingsMgr->ShouldLog(Log::Verbose))
				printf("FPathCLoad Func Start is at: %p\n", MK12::ReadFNameToWStrCommonStart);
		}

		return true;
	}

	bool UNameTableGetter()
	{
		printf("\n==UNameObjectGetter==\n");

		uint64_t patAddr = GamePatcher->ResolvePattern(SettingsMgr->pUNameObjGetPat, "UNameObjGet");
		if (!patAddr)
			return false;

		uint64_t* lpPattern = (uint64_t*)patAddr;

		uint64_t UNameMain	= ProcessPatch::ResolveDestination((uint64_t)lpPattern, 2, 7, 4) - 0x8;
		uint64_t UNameSub	= ProcessPatch::ResolveDestination((uint64_t)lpPattern + 0x12, 3, 7, 4);

		if (SettingsMgr->ShouldLog(Log::Verbose))
		{
			printf("UNameMain Object found at: %p\n", (uint64_t*)UNameMain);
			printf("UNameSub Object found at: %p\n", (uint64_t*)UNameSub);
		}

		// Find NameTable here for now until moving it to a different place
		MK12::UNameTable = (MK12::UNameTableStruct*)UNameSub;
		MK12::UMainNameTable = (MK12::UNameTableMainStruct*)UNameMain; // Can be UNameTable - 0x10

		printfSuccess("UName Objects Saved!\n");

		printf("\n==InitializeNameTableProxy==\n");

		uint64_t InitNameTableFuncAddr = (uint64_t)lpPattern + 0x22;
		if (*(uint8_t*)InitNameTableFuncAddr != 0xE8) // Call
		{
			printfError("InitializeNameTable Function not found!\n");
			return false;
		}
		if (SettingsMgr->ShouldLog(Log::Verbose))
		{
			printf("InitializeNameTable Function found at: %p\n", (uint64_t*)InitNameTableFuncAddr);
		}

		MK12::InitializeNameTable = (MK12::InitializeNameTableType*)ProcessPatch::ResolveDestination(InitNameTableFuncAddr);
		printfSuccess("InitializeNameTableFunction!\n");

		return true;
	}

	bool OverrideFNameToWStrFuncs()
	{

		if (!SettingsMgr->bUNameGetter)
		{
			printfWarning("Overriding FNameToWStr disabled: bUNameGetter is not enabled in ini!\n");
			return false;
		}
		if (!HookMetadata::ActiveModsMap["UNameTableGetter"])
		{
			printfError("Overriding FNameToWStr disabled: UNameTableGetter pattern failed!");
			return false;
		}

		if (MK12::ReadFNameToWStrWithIdStart == nullptr)
		{
			if (SettingsMgr->bFNameToStrHook)
			{
				if (HookMetadata::ActiveModsMap["bFPathIdLoader"])
				{
					printfYellow("Failed to override FNameToWStrId, but Proxy Loader is working.\n");
				}
				else
				{
					printfError("Failed to override FNameToWStrId and Proxy Loader is NOT working!");
				}
			}
		}
		else
		{
			GamePatcher->ReplaceFunctionWith((uint64_t)MK12::ReadFNameToWStrWithIdStart, MK12::Remake::FNameInfoToWStringWithId);
			if (SettingsMgr->bFNameToStrHook && HookMetadata::ActiveModsMap["bFPathIdLoader"])
				printfYellow("FNameToWStrId replacing Proxy Loader\n");
			else
				printfSuccess("Overrided FNameToWStrId skipping Proxy Loader");
		}

		if (MK12::ReadFNameToWStrNoIdStart == nullptr)
		{
			if (SettingsMgr->bFNameToStrHook)
			{
				if (HookMetadata::ActiveModsMap["bFPathNoIdLoader"])
				{
					printfYellow("Failed to override FNameToWStrNoId, but Proxy Loader is working.\n");
				}
				else
				{
					printfError("Failed to override FNameToWStrNoId and Proxy Loader is NOT working!");
				}
			}
		}
		else
		{
			GamePatcher->ReplaceFunctionWith((uint64_t)MK12::ReadFNameToWStrNoIdStart, MK12::Remake::FNameInfoToWStringNoId);
			if (SettingsMgr->bFNameToStrHook && HookMetadata::ActiveModsMap["bFPathNoIdLoader"])
				printfYellow("FNameToWStrNoId replacing Proxy Loader\n");
			else
				printfSuccess("Overrided FNameToWStrNoId skipping Proxy Loader");
		}

		if (MK12::ReadFNameToWStrCommonStart == nullptr)
		{
			if (SettingsMgr->bFNameToStrHook)
			{
				if (HookMetadata::ActiveModsMap["bFPathCommonLoader"])
				{
					printfYellow("Failed to override FNameToWStrCommon, but Proxy Loader is working.\n");
				}
				else
				{
					printfError("Failed to override FNameToWStrCommon and Proxy Loader is NOT working!");
				}
			}
		}
		else
		{
			GamePatcher->ReplaceFunctionWith((uint64_t)MK12::ReadFNameToWStrCommonStart, MK12::Remake::FNameInfoToWString);
			if (SettingsMgr->bFNameToStrHook && HookMetadata::ActiveModsMap["bFPathCommonLoader"])
				printfYellow("FNameToWStrCommon replacing Proxy Loader\n");
			else
				printfSuccess("Overrided FNameToWStrCommon skipping Proxy Loader");
		}

		bool anyOverride = MK12::ReadFNameToWStrWithIdStart || MK12::ReadFNameToWStrNoIdStart || MK12::ReadFNameToWStrCommonStart;
		if (anyOverride)
			printfSuccess("FNameToWStr Override Active");
		else
			printfWarning("FNameToWStr Override: No function starts found, falling back to Proxy Loaders");

		return true;
	}

	bool OverrideGameEndpointsData()
	{
		printf("\n==OverrideGameEndpointsData==\n");

		if (SettingsMgr->szServerUrl.empty())
		{
			printfWarning("Server Url is empty or not specified. Skipping!");
			return false;
		}

		return GamePatcher->ProxyByPattern(SettingsMgr->pEndpointLoader, "EndpointLoader", 0x00,
			MK12Hook::Proxies::OverrideGameEndpoint, &MK12::GetEndpointKeyValue, PATCH_CALL);
	}

	bool ProfileGetterHooks()
	{
		printf("\n==ProfileGetterHooks==\n");

		return GamePatcher->ProxyByPattern(SettingsMgr->pProfileGetter, "ProfileGetter", 30,
			MK12Hook::Proxies::MKWScanfProxyForCrypto, &MK12::MKWScanf, PATCH_CALL);
	}

	int FloydTrackingHooks()
	{
		printf("\n==FloydTrackingHooks==\n");

		int HooksCtr = 0;
		int ExpectedHooksCtr = 3;

		HooksCtr += GamePatcher->ProxyByPattern(SettingsMgr->pGetChallengesFromHash, "GetChallengesFromHash", 14,
			MK12Hook::Proxies::GenerateFloydCluesFromHashProxy, &MK12::GenerateFloydCluesFromHash, PATCH_CALL);

		HooksCtr += GamePatcher->ProxyByPattern(SettingsMgr->pGetFloydHashInputString, "GetFloydHashInputString", 11,
			MK12Hook::Proxies::CustomCityHashProxy, &MK12::CustomCityHash, PATCH_CALL);

		HooksCtr += GamePatcher->ProxyByPattern(SettingsMgr->pGetFloydHashInputString2, "GetFloydHashInputString2", 11,
			MK12Hook::Proxies::CustomCityHashProxy, &MK12::CustomCityHash, PATCH_CALL);

		if (HooksCtr == ExpectedHooksCtr)
			printfSuccess("Floyd Tracking On");
		else if (HooksCtr)
			printfWarning("Not all Floyd Trackers functioning!");
		else
			printfError("Floyd Tracking not working!");

		return HooksCtr;
	}

	bool ExtractFightMetadataFromSecretFightSetupStage()
	{
		printf("\n==ExtractFightMetadataFromSecretFightSetupStage==\n");

		uint64_t patAddr = GamePatcher->ResolvePattern(SettingsMgr->pSecretFightCondPat, "SecretFightConditionSetup");
		if (!patAddr)
			return false;

		MK12::KlassicTowerSecretFightDataOffset = 0x80;

		GamePatcher->ProxyCallAt(patAddr + 77, MK12Hook::Proxies::SetupSecretFightConditionsProxy, &MK12::SetupSecretFightConditions, PATCH_JUMP);
		printfSuccess("SecretFightConditionSetup Proxied");

		return true;
	}

};

namespace MK12Hook::Mods {
	static const char* AnnouncerSwapString = "/Game/WwiseAudio/Switches/SWITCHES_All/SWGP_Announcer/SWGP_Announcer-SWTC_ANNO_%s.SWGP_Announcer-SWTC_ANNO_%s";
	static const std::string* AllAnnouncerStrings[] = {
		&SettingsMgr->AnnouncerSwap.szDefault,
		&SettingsMgr->AnnouncerSwap.szGeras,
		&SettingsMgr->AnnouncerSwap.szJohnnyCage,
		&SettingsMgr->AnnouncerSwap.szLiuKang,
		&SettingsMgr->AnnouncerSwap.szOmniMan,
		&SettingsMgr->AnnouncerSwap.szShangTsung,
		&SettingsMgr->AnnouncerSwap.szSindel,
		&SettingsMgr->AnnouncerSwap.szSubZero
	};
	static const char* AllAnnouncerNames[] = {
		"Default",
		"Geras",
		"JohnnyCage",
		"LiuKang",
		"OmniMan",
		"ShangTsung",
		"Sindel",
		"SubZero"
	};
	int AnnouncerSwap()
	{
		printf("\n==Swapping Announcers==\n");
		if (!SettingsMgr->bFNameToStrHook)
		{
			printfError("No FPath Swapper has been loaded. Make sure bFPathLoader is enabled.");
			return false;
		}
		if (!HookMetadata::ActiveModsMap["bFPathIdLoader"])
		{
			printfError("No FPath Swapper has been loaded. FNameToStrWithIdLoader Patching was unsuccessful.");
			return false;
		}
	
		char CurAnnoString[256];
		char SwapAnnoString[256];
		int count = 0;
		for (int i = 0; i < 8; i++)
		{
			if (AllAnnouncerStrings[i]->empty())
				continue;

			if (strcmp(AllAnnouncerStrings[i]->c_str(), AllAnnouncerNames[i]) == 0) // No Swap
				continue;
			
			snprintf(CurAnnoString, sizeof(CurAnnoString), AnnouncerSwapString, AllAnnouncerNames[i], AllAnnouncerNames[i]);
			snprintf(SwapAnnoString, sizeof(SwapAnnoString), AnnouncerSwapString, AllAnnouncerStrings[i]->c_str(), AllAnnouncerStrings[i]->c_str());
			HookMetadata::FSwapTable.insert(CurAnnoString, SwapAnnoString);
			count++;
		}

		printfYellow("%d Announcers swapped.\n", count);
		int iDebugSwap = 0;

		//HookMetadata::FSwapTable.insert(Proxies::OriginalModifierName, Proxies::NewModifierName);
		//{
		//	const char* a1 = "/Game/DLC/REL_GOTY/Char/SubZero/Skin/007/Blueprint/BP_SubZero_Skin007_A_Char.BP_SubZero_Skin007_A_Char_C";
		//	const char* a2 = "/Game/Disk/Char/SubZero/Skin/002/Blueprint/BP_SubZero_Skin002_A_Char.BP_SubZero_Skin002_A_Char_C";
		//	//const char* a2 = "/Game/DLC/REL_T1000/Char/SubZero/Skin/009/Blueprint/BP_SubZero_Skin009_A_Char.BP_SubZero_Skin009_A_Char_C";
		//	HookMetadata::FSwapTable.insert(a1, a2);
		//	iDebugSwap++;
		//}
		//{
		//	const char* a1 = "/Game/DLC/REL_GOTY/Char/SubZero/Skin/007/Blueprint/BP_SubZero_Skin007_A_Char";
		//	const char* a2 = "/Game/Disk/Char/SubZero/Skin/002/Blueprint/BP_SubZero_Skin002_A_Char";
		//	HookMetadata::FSwapTable.insert(a1, a2);
		//	iDebugSwap++;
		//}
		/*{
			const char* a1 = "/Game/DLC/REL_T1000/Char/T1000/Skin/001/Palette/SetA/Blueprint/BP_T1000_Skin001_Pal001.BP_T1000_Skin001_Pal001_C";
			const char* a2 = "/Game/DLC/REL_GOTY/Char/SubZero/Skin/007/Palette/SetA/Blueprint/BP_SubZero_Skin007_Pal002.BP_SubZero_Skin007_Pal002_C";
			HookMetadata::FSwapTable.insert(a1, a2);
			iDebugSwap++;
		}
		{
			const char* a1 = "/Game/DLC/REL_T1000/Char/T1000/Skin/001/Palette/SetA/Blueprint/BP_T1000_Skin001_Pal001";
			const char* a2 = "/Game/DLC/REL_GOTY/Char/SubZero/Skin/007/Palette/SetA/Blueprint/BP_SubZero_Skin007_Pal002";
			HookMetadata::FSwapTable.insert(a1, a2);
			iDebugSwap++;
		}
		{
			const char* a1 = "/Game/DLC/REL_T1000/Char/T1000/Skin/001/Blueprint/BP_T1000_Skin001_A_Char";
			const char* a2 = "/Game/DLC/REL_GOTY/Char/SubZero/Skin/007/Blueprint/BP_SubZero_Skin007_A_Char";
			HookMetadata::FSwapTable.insert(a1, a2);
			iDebugSwap++;
		}
		{
			const char* a1 = "/Game/DLC/REL_T1000/Char/T1000/Skin/001/Blueprint/BP_T1000_Skin001_A_Char.BP_T1000_Skin001_A_Char_C";
			const char* a2 = "/Game/DLC/REL_GOTY/Char/SubZero/Skin/007/Blueprint/BP_SubZero_Skin007_A_Char.BP_SubZero_Skin007_A_Char_C";
			HookMetadata::FSwapTable.insert(a1, a2);
			iDebugSwap++;
		}
		{
			const char* a1 = "/Game/DLC/REL_T1000/Char/T1000/Template/Blueprint/BP_T1000_CharSelect_Anim.BP_T1000_CharSelect_Anim_C";
			const char* a2 = "/Game/Disk/Char/SubZero/Template/Blueprint/BP_SubZero_CharSelect_Anim.BP_SubZero_CharSelect_Anim_C";
			HookMetadata::FSwapTable.insert(a1, a2);
			iDebugSwap++;
		}
		{
			const char* a1 = "/Game/DLC/REL_T1000/Char/T1000/Template/Blueprint/BP_T1000_CharSelect_Anim";
			const char* a2 = "/Game/Disk/Char/SubZero/Template/Blueprint/BP_SubZero_CharSelect_Anim";
			HookMetadata::FSwapTable.insert(a1, a2);
			iDebugSwap++;
		}*/
		//printfYellow("DEBUG swapped %d.\n", iDebugSwap);


		
		return count;
	}
	int StringSwaps()
	{
		printf("\n==Swapping Strings==\n");
		if (!SettingsMgr->bFNameToStrHook)
		{
			printfError("No FPath Swapper has been loaded. Make sure bFPathLoader is enabled.");
			return -1;
		}
		if (!HookMetadata::ActiveModsMap["bFPathIdLoader"])
		{
			printfError("No FPath Swapper has been loaded. FNameToStrWithIdLoader Patching was unsuccessful.");
			return -1;
		}
		std::ifstream file("string_swaps.txt");

		CPPython::string line;
		int iSwapsAmount = 0;

		if (!file.is_open()) {
			printfError("Failed to open string_swaps.txt\n");
			return -1;
		}

		while (std::getline(file, line)) {
			CPPython::string stripped_line = line.strip();
			auto parts = stripped_line.split("->");

			if (parts.size() == 2) {
				CPPython::string a1 = parts[0].strip(" ");
				CPPython::string a2 = parts[1].strip(" ");
				HookMetadata::FSwapTable.insert(a1.c_str(), a2.c_str());
				std::cout << "Inserted: " << a1 << " -> " << a2 << std::endl;
				iSwapsAmount++;
			}
		}

		file.close();
		printfYellow("%d Strings swapped.\n", iSwapsAmount);
		return iSwapsAmount;
	}
}

namespace HookMetadata {
	HHOOK KeyboardProcHook = nullptr;
	HMODULE CurrentDllModule = NULL;
	HANDLE Console = NULL;
};