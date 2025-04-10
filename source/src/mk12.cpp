#include "mkutils.h"
#include "mk12.h"


uint64_t*							MK12::lpGameVersionFull			= nullptr;
uint64_t*							MK12::lpGameVersion				= nullptr;
uint8_t								MK12::ulCharactersCount;
std::vector<std::wstring>			MK12::vSwappedFiles;

MK12::UNameTableStruct*				MK12::UNameTable;
MK12::UNameTableMainStruct*			MK12::UMainNameTable;

// Game Functions
MK12::ReadFStringType*					MK12::ReadFString;
MK12::FNameToWStrType*					MK12::FNameToWStr;
MK12::InitializeNameTableType*			MK12::InitializeNameTable;
uint64_t*								MK12::ReadFNameToWStrNoIdStart		= nullptr;
uint64_t*								MK12::ReadFNameToWStrWithIdStart	= nullptr;
uint64_t*								MK12::ReadFNameToWStrCommonStart	= nullptr;

MK12::GetEndpointKeyValueType*			MK12::GetEndpointKeyValue			= nullptr;

MK12::SetupSecretFightConditionsType*	MK12::SetupSecretFightConditions	= nullptr;
// Floyd
MK12::GenerateFloydCluesFromHashType*	MK12::GenerateFloydCluesFromHash	= nullptr;
MK12::CustomCityHashType*				MK12::CustomCityHash				= nullptr;
MK12::MKWScanfType*						MK12::MKWScanf						= nullptr;


uint8_t									MK12::KlassicTowerSecretFightDataOffset   = 0;

bool containsFloydCaseInsensitive(const wchar_t* str) {
	std::wstring wstr(str);
	//std::wstring target = L"floyd";
	std::wstring target = L"BP_SubZero_Skin007_Pal004.BP_SubZero_Skin007_Pal004_C";

	std::transform(wstr.begin(), wstr.end(), wstr.begin(), ::towlower);

	return wstr.find(target) != std::wstring::npos;
}

bool containsCaseInsensitive(const char* str, const char* compare) {
	std::string s(str);
	std::string target = compare;

	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });

	return s.find(target) != std::string::npos;
}

MK12::FName* MK12::NameTableIndexToFName(uint16_t NameTableId, uint16_t NameOffset)
{
	uint32_t NameOffsetC = NameOffset * 2;
	uint64_t NameTableStartLoc = (uint64_t)UNameTable->NameTableEntry[NameTableId];
	return (FName*)(NameTableStartLoc + NameOffsetC);
}

MK12::FName* MK12::NameTableIndexToFName(MK12::FNameInfoStruct* s)
{
	return NameTableIndexToFName(s->NameTableId, s->NameOffset);
}

uint64_t __fastcall	MK12::Remake::FNameInfoToWString(MK12::FNameInfoStruct* FNameInfo, char* Destination)
{
	if (!MK12::UMainNameTable->IsInitialized)
	{
		MK12::UNameTable = (MK12::UNameTableStruct*)MK12::InitializeNameTable(*MK12::UNameTable);
		MK12::UMainNameTable->IsInitialized = 1;
	}

	MK12::FName* Name = MK12::NameTableIndexToFName(FNameInfo);
	return FNameObjectToWStringCommon(Name, Destination);
}

uint64_t __fastcall MK12::Remake::FNameObjectToWString(MK12::FName* Name, char* Destination) // This is the function that we jmp to
{
	uint64_t NameSize = FNameObjectToWStringCommon(Name, Destination);
	printf("FNameObjectToWString::%p = %ls\n", (void*)Name, (wchar_t*)Destination);
	return NameSize;
}

uint64_t __fastcall MK12::Remake::FNameObjectToWStringCommon(MK12::FName* Name, char* Destination)
{
	// Swap should be here in this function
	uint64_t NameSize = MK12::FNameFunc::GetSize(*Name);
	MK12::FNameToWStr(*Name, Destination);
	*(wchar_t*)&Destination[2 * NameSize] = L'\0';

	return NameSize;
}

uint64_t __fastcall	MK12::Remake::FNameInfoToWStringNoId(MK12::FNameInfoStruct* FNameInfo, char* Destination)
{
	uint64_t NameSize = FNameInfoToWString(FNameInfo, Destination);
	//if (containsFloydCaseInsensitive((wchar_t*)Destination))
	printf("FNameInfoToWStringNoId::%p = %ls\n", (void*)FNameInfo, (wchar_t*)Destination);
	return NameSize;
}

uint64_t __fastcall	MK12::Remake::FNameInfoToWStringWithId(MK12::FNameInfoStruct* FNameInfo, char* Destination)
{
	uint64_t NameSize = FNameInfoToWString(FNameInfo, Destination);
	
	uint64_t DupId = FNameInfo->DuplicateId;
	//if (containsFloydCaseInsensitive((wchar_t*)Destination))
	printf("FNameInfoToWStringWithId::%p = %ls dup %lld\n", (void*)FNameInfo, (wchar_t*)Destination, DupId);

	if (!DupId)
		return NameSize;

	wchar_t buff[16];
	__int32 addSize = swprintf(buff, L"_%d", DupId - 1);
	char* PtrToEndOfStr = &Destination[2 * NameSize];
	NameSize += addSize;
	memcpy(PtrToEndOfStr, buff, 2 * (__int64)addSize);
	*(wchar_t*)&Destination[2 * NameSize] = L'\0';

	return NameSize;
}

bool __fastcall MK12::Remake::GetArgBoolByNameWrapper(uint64_t* thisPtr, wchar_t* ArgName)
{
	bool value = GetArgBoolByName(thisPtr, ArgName);
	std::wcout << L"Loader " << ArgName << L" = " << value << L"\n";
	return value;
}

bool __fastcall MK12::Remake::GetArgBoolByName(uint64_t *thisPtr, wchar_t* ArgName)
{
	typedef uint32_t(__fastcall* internalfunction)(uint64_t, uint64_t, uint64_t);
	auto function = (internalfunction)0x142228650;
	
	if (*thisPtr)
	{
		auto v4 = thisPtr;
		if (ArgName)
		{
			while (v4)
			{
				auto v5 = 0;
				auto v6 = -1i64;
				auto v7 = *ArgName - 32;
				if ((uint16_t)(*ArgName - 97) > 0x19u)
					v7 = *ArgName;
				do
					++v6;
				while (ArgName[v6]);
				auto v8 = *v4;
				auto v9 = v6 - 1;
				++v4;
				auto v10 = 0;
				if (!v8)
					break;
				while (true)
				{
					auto v11 = v8 - 32;
					if ((uint16_t)(v8 - 97) > 0x19u)
						v11 = v8;
					if (!v10 && !v5 && v11 == v7 && !(unsigned int)function((__int64)v4, (uint64_t)((uint16_t*)ArgName + 1), v9))
					{
						break;
					}
					v5 = (uint16_t)(v11 - 65) <= 0x19u || (uint16_t)(v11 - 48) <= 9u;
					if (v11 == 34)
						v10 = v10 == 0;
					v8 = *v4;
					++v4;
					if (!v8)
						return 0;
				}
				auto v13 = (uint64_t)(v4 - 1);
				if (v4 == (uint64_t*)2)
					break;
				if (v13 > (uint64_t)thisPtr
					&& !((*(v4 - 2) - 45) & 0xFFFD)
					&& (thisPtr > v4 - 3 || (unsigned int)iswspace((uint16_t) * (v4 - 3)))
					)
				{
					auto v14 = -1i64;
					do
						++v14;
					while (ArgName[v14]);
					auto v15 = (uint16_t*)(v13 + 2i64 * (int)v14);
					if (!v15)
						return 1;
					auto v16 = *v15;
					if (!(__int16)v16 || (unsigned int)iswspace(v16))
						return 1;
				}
			}
		}
	}
	return 0;
}

std::string MK12::GetGameVersion()
{
	if (!lpGameVersion)
	{
		return "";
	}
	if (!*lpGameVersion)
	{
		return "";
	}
	return reinterpret_cast<char*>(*lpGameVersion);
}

std::string MK12::GetGameVersionFull()
{
	if (lpGameVersionFull == NULL)
	{
		return "";
	}
	if (!*lpGameVersionFull)
	{
		return "";
	}
	uint64_t* ptr = (uint64_t*)(uint64_t(*lpGameVersionFull) + 0x30);
	if (!*ptr)
	{
		return "";
	}
	return reinterpret_cast<char*>(*ptr);
};


namespace MK12::FNameFunc {
	inline uint16_t GetSize(FName& F)
	{
		return F.NameSize >> 6;
	};
	inline char* GetName(FName& F)
	{
		return F.Name;
	}
	void Print(FName& F)
	{
		uint16_t s = GetSize(F);
		fwrite(F.Name, 1, s, stdout);
		printf("\n");
	}
	char* ToStr(FName& F)
	{
		uint16_t s = GetSize(F);
		char* name = new char[s + 1];
		strncpy(name, F.Name, s);
		name[s] = '\0'; // Null terminate
		return name;
	}
	FName* FromStr(const char* string)
	{
		uint16_t length = (uint16_t)strlen(string);
		FName* f = (FName*)(new uint8_t[length + 2]);
		f->NameSize = (length << 6) + 0xC; // IDK where C comes from
		strncpy(f->Name, string, length);

		return f;
	}
}

namespace MK12::Remake::SecretFight {
	void PrintCharacterRequirements(FCharacterRequirementConditionHelper *mCharacterRequirements)
	{
		if (!mCharacterRequirements->enable)
			return;
		
		MK12::FName* Char1Type = MK12::NameTableIndexToFName(&mCharacterRequirements->Character.PrimaryAssetType.Name);
		MK12::FName* Char1Name = MK12::NameTableIndexToFName(&mCharacterRequirements->Character.PrimaryAssetName);
		MK12::FName* Kameo1Type = MK12::NameTableIndexToFName(&mCharacterRequirements->Kameo.PrimaryAssetType.Name);
		MK12::FName* Kameo1Name = MK12::NameTableIndexToFName(&mCharacterRequirements->Kameo.PrimaryAssetName);

		MK12::FName* Char2Type = MK12::NameTableIndexToFName(&mCharacterRequirements->OpponentCharacter.PrimaryAssetType.Name);
		MK12::FName* Char2Name = MK12::NameTableIndexToFName(&mCharacterRequirements->OpponentCharacter.PrimaryAssetName);
		MK12::FName* Kameo2Type = MK12::NameTableIndexToFName(&mCharacterRequirements->OpponentKameo.PrimaryAssetType.Name);
		MK12::FName* Kameo2Name = MK12::NameTableIndexToFName(&mCharacterRequirements->OpponentKameo.PrimaryAssetName);

		printf("Character Requirements:\n");
		if (strncmp(Char1Type->Name, "None", 4))
			printf("\tPlayer %s %s\n", FNAME_STR(Char1Type), FNAME_STR(Char1Name));
		if (strncmp(Kameo1Type->Name, "None", 4))
			printf("\tPlayer %s %s\n", FNAME_STR(Kameo1Type), FNAME_STR(Kameo1Name));
		if (strncmp(Char2Type->Name, "None", 4))
			printf("\tEnemy %s %s\n", FNAME_STR(Char2Type), FNAME_STR(Char2Name));
		if (strncmp(Kameo2Type->Name, "None", 4))
			printf("\tEnemy %s %s\n", FNAME_STR(Kameo2Type), FNAME_STR(Kameo2Name));
		
	}

	void PrintCharTypeRequirement(Enums::EFighterStatCheckMode& mFighterCheckMode)
	{
		switch (mFighterCheckMode)
		{
		case MK12::Enums::EFighterStatCheckMode::MainFighterAndKameo:
			printf("Char & Kameo\n"); break;
		case MK12::Enums::EFighterStatCheckMode::MainFighterOnly:
			printf("Char\n"); break;
		case MK12::Enums::EFighterStatCheckMode::KameoOnly:
			printf("Kameo\n"); break;
		}
	}

	void PrintComparisonType(Enums::EComparisonType& mComparisonType)
	{
		switch (mComparisonType)
		{
		case MK12::Enums::EComparisonType::Equal:
			printf(" = "); break;
		case MK12::Enums::EComparisonType::GreaterThan:
			printf(" > "); break;
		case MK12::Enums::EComparisonType::LessThan:
			printf(" < "); break;
		case MK12::Enums::EComparisonType::GreaterThanOrEqual:
			printf(" >= "); break;
		case MK12::Enums::EComparisonType::LessThanOrEqual:
			printf(" <= "); break;
		}
	}

	void PrintRoundType(Enums::EFightingStatRoundToCheck& mRoundToCheck)
	{
		printf("On ");
		switch (mRoundToCheck)
		{
		case MK12::Enums::EFightingStatRoundToCheck::All: // By the end of the match
			printf("Total"); break;
		case MK12::Enums::EFightingStatRoundToCheck::Any: // Any
			printf("Any"); break;
		case MK12::Enums::EFightingStatRoundToCheck::Last: // Final Round
			printf("The Final"); break;
		case MK12::Enums::EFightingStatRoundToCheck::LowestValueRound: // Round where you do the least
			printf("Where You Did The Least"); break;
		case MK12::Enums::EFightingStatRoundToCheck::HighestValueRound: // Round where you do the max
			printf("Where You Did The Most"); break;
		case MK12::Enums::EFightingStatRoundToCheck::LowestChangeBetweenRounds: // ?
			printf("LowestPossibleChange"); break;
		case MK12::Enums::EFightingStatRoundToCheck::HighestChangeBetweenRounds: // ?
			printf("HighestPossibleChange"); break;
		}
		printf(" Round\n");
	}

	void PrintTypedConditions(FLadderSecretFightCondition_FightingStatCondition* RetypedCondition)
	{
		PrintCharacterRequirements(&RetypedCondition->mCharacterRequirements);

		printf("Tracking action from ");
		if (RetypedCondition->mCheckOpponentTeam)
		{
			printf("Enemy ");
		}
		
		PrintCharTypeRequirement(RetypedCondition->mFighterCheckMode);
		printf("Condition: ");
		PrintFGStatID(RetypedCondition->mStatID);
		PrintComparisonType(RetypedCondition->mComparisonType);

		if (RetypedCondition->mCompareAgainstOtherStat)
		{
			PrintFGStatID(RetypedCondition->mComparisonStatID);
			printf(" by ");
		}
		printf("%i ", RetypedCondition->mRequiredValue);

		PrintRoundType(RetypedCondition->mRoundToCheck);
	}

	void PrintTypedConditions(FLadderSecretFightCondition_MultiFightingStatCondition* RetypedCondition)
	{
		int32_t elements = RetypedCondition->mConditions.ElementsCount;
		bool all = false;

		switch (RetypedCondition->mGroupType)
		{
		case MK12::Enums::EComparisonGroup::All:
			all = true; break;
		case MK12::Enums::EComparisonGroup::Any:
			all = false; ; break;
		}

		for (int i = 0; i < elements; i++)
		{
			MK12::FFightingStatCondition* CurrentElement = MK12::TArrayHelpers::index<MK12::FFightingStatCondition>(RetypedCondition->mConditions, i);
			MK12::FLadderSecretFightCondition_FightingStatCondition* RecastElement = reinterpret_cast<MK12::FLadderSecretFightCondition_FightingStatCondition*>((uint64_t)(CurrentElement) - 0x8);

			PrintTypedConditions(RecastElement);
			if (i < elements-1)
			{
				if (all)
					printf("\t\t == AND == \n");
				else
					printf("\t\t == OR == \n");
			}
		}
	}

	void PrintTypedConditions(FLadderSecretFightCondition_MoveTraceConditions* RetypedCondition)
	{
		PrintCharacterRequirements(&RetypedCondition->mCharacterRequirements);

		if (RetypedCondition->mCheckOpponentTeam)
		{
			printf("Must be performed by enemy team");
		}

		int32_t elements = RetypedCondition->mConditions.ElementsCount;
		bool all = false;

		switch (RetypedCondition->mGroupType)
		{
		case MK12::Enums::EComparisonGroup::All:
			all = true; break;
		case MK12::Enums::EComparisonGroup::Any:
			all = false; ; break;
		}

		for (int i = 0; i < elements; i++)
		{
			MK12::FSecretFightMoveTraceCondition* CurrentElement = MK12::TArrayHelpers::index<MK12::FSecretFightMoveTraceCondition>(RetypedCondition->mConditions, i);
			printf("Perform %s as a ", EnumMaps::EManualMoveTraceEventMap[CurrentElement->mTraceID].c_str());

			switch (CurrentElement->mTraceFlags)
			{
			case Enums::EMoveTraceFlags::None:
				printf("normal move "); break;
			case Enums::EMoveTraceFlags::KameoMove:
				printf("Kameo move "); break;
			case Enums::EMoveTraceFlags::TagPartnerMove:
				printf("Tag Team move "); break;
			case Enums::EMoveTraceFlags::OpponentMorphMove:
				printf("Shang Tsung move "); break;
			case Enums::EMoveTraceFlags::OpponentStolenMove:
				printf("Shujinko move "); break;
			}

			printf("and ");
			switch (CurrentElement->mTraceCondition)
			{
			case Enums::EMoveTraceCondition::None:
			case Enums::EMoveTraceCondition::MoveStarted:
				printf("that's it "); break;
			case Enums::EMoveTraceCondition::MoveHit:
				printf("Hit your enemy "); break;
			case Enums::EMoveTraceCondition::MoveBlocked:
				printf("Hit a blocking enemy "); break;
			case Enums::EMoveTraceCondition::MovePerfectBlocked:
				printf("Hit a perfect block "); break;
			case Enums::EMoveTraceCondition::MoveUpBlocked:
				printf("Hit an Up Block "); break;
			case Enums::EMoveTraceCondition::MoveMissed:
				printf("Miss your enemy "); break;
			case Enums::EMoveTraceCondition::ManualTrace:
				printf("Trace it (idk what that means) "); break;
			}

			printf("%d times.\n", CurrentElement->mRequiredCompletions);

			if (i < elements - 1)
			{
				if (all)
					printf("\t\t == AND == \n");
				else
					printf("\t\t == OR == \n");
			}

		}

		printf("And repeat a total of %d times.\n", RetypedCondition->mRequiredCompletions);
	}

	void PrintTypedConditions(FLadderSecretFightCondition_ProfileStatCondition* RetypedCondition)
	{
		printf("Achieve Career Progression %s", EnumMaps::MKProfileStatIDMap[RetypedCondition->mStatID].c_str());
		PrintComparisonType(RetypedCondition->mComparisonType);
		printf("%d Times ", RetypedCondition->mRequiredValue);
		if (RetypedCondition->mRequiredPasses > 1)
		{
			printf("And repeat the challenge with");
			PrintComparisonType(RetypedCondition->mPassesComparisonType);
			printf("%d different characters", RetypedCondition->mRequiredPasses);
		}
		printf("\n");
	}

	void PrintFGStatID(Enums::FGStatID& enumkey)
	{
		printf("%s", EnumMaps::FGStatIDMap[enumkey].c_str());
	}

}



namespace MK12::EnumMaps {
	std::unordered_map<MK12::Enums::FGStatID, std::string> FGStatIDMap = {
		{MK12::Enums::FGStatID::INVALID, "INVALID"},
		{MK12::Enums::FGStatID::TotalFlawlessRounds, "TotalFlawlessRounds"},
		{MK12::Enums::FGStatID::TotalAcceptableRounds, "TotalAcceptableRounds"},
		{MK12::Enums::FGStatID::RoundStartTime, "RoundStartTime"},
		{MK12::Enums::FGStatID::RoundEndTime, "RoundEndTime"},
		{MK12::Enums::FGStatID::TotalRoundsWonWithSuperMoves, "TotalRoundsWonWithSuperMoves"},
		{MK12::Enums::FGStatID::TotalRoundsWonWithThrows, "TotalRoundsWonWithThrows"},
		{MK12::Enums::FGStatID::TotalRoundsWonTimeout, "TotalRoundsWonTimeout"},
		{MK12::Enums::FGStatID::TotalRoundsWonDanger, "TotalRoundsWonDanger"},
		{MK12::Enums::FGStatID::TotalSuperMoveMatchWins, "TotalSuperMoveMatchWins"},
		{MK12::Enums::FGStatID::TotalQuitalityMatchWins, "TotalQuitalityMatchWins"},
		{MK12::Enums::FGStatID::TotalMatchWins, "TotalMatchWins"},
		{MK12::Enums::FGStatID::PlayerEndHealth, "PlayerEndHealth"},
		{MK12::Enums::FGStatID::DamageTaken, "DamageTaken"},
		{MK12::Enums::FGStatID::TotalBloodSpilt, "TotalBloodSpilt"},
		{MK12::Enums::FGStatID::TotalDamagePerformed, "TotalDamagePerformed"},
		{MK12::Enums::FGStatID::TotalBlockDamagePerformed, "TotalBlockDamagePerformed"},
		{MK12::Enums::FGStatID::TotalFirstHits, "TotalFirstHits"},
		{MK12::Enums::FGStatID::LongestCombo, "LongestCombo"},
		{MK12::Enums::FGStatID::LongestAirCombo, "LongestAirCombo"},
		{MK12::Enums::FGStatID::MaxComboDamage, "MaxComboDamage"},
		{MK12::Enums::FGStatID::TotalCombos, "TotalCombos"},
		{MK12::Enums::FGStatID::Total2HitCombos, "Total2HitCombos"},
		{MK12::Enums::FGStatID::Total3HitCombos, "Total3HitCombos"},
		{MK12::Enums::FGStatID::Total4HitCombos, "Total4HitCombos"},
		{MK12::Enums::FGStatID::Total5HitCombos, "Total5HitCombos"},
		{MK12::Enums::FGStatID::Total6HitCombos, "Total6HitCombos"},
		{MK12::Enums::FGStatID::Total12HitCombos, "Total12HitCombos"},
		{MK12::Enums::FGStatID::Total1HitAirCombos, "Total1HitAirCombos"},
		{MK12::Enums::FGStatID::Total2HitAirCombos, "Total2HitAirCombos"},
		{MK12::Enums::FGStatID::Total3HitAirCombos, "Total3HitAirCombos"},
		{MK12::Enums::FGStatID::Total4HitAirCombos, "Total4HitAirCombos"},
		{MK12::Enums::FGStatID::Total5HitAirCombos, "Total5HitAirCombos"},
		{MK12::Enums::FGStatID::Total6HitAirCombos, "Total6HitAirCombos"},
		{MK12::Enums::FGStatID::TotalComboDamageTier1, "TotalComboDamageTier1"},
		{MK12::Enums::FGStatID::TotalComboDamageTier2, "TotalComboDamageTier2"},
		{MK12::Enums::FGStatID::TotalComboDamageTier3, "TotalComboDamageTier3"},
		{MK12::Enums::FGStatID::TotalBlocks, "TotalBlocks"},
		{MK12::Enums::FGStatID::TotalPerfectBlocks, "TotalPerfectBlocks"},
		{MK12::Enums::FGStatID::TotalParryAttempted, "TotalParryAttempted"},
		{MK12::Enums::FGStatID::TotalParryConnected, "TotalParryConnected"},
		{MK12::Enums::FGStatID::TotalBlockedLowAttacks, "TotalBlockedLowAttacks"},
		{MK12::Enums::FGStatID::TotalBlockedMidAttacks, "TotalBlockedMidAttacks"},
		{MK12::Enums::FGStatID::TotalBlockedHighAttacks, "TotalBlockedHighAttacks"},
		{MK12::Enums::FGStatID::TotalBlockedOverheadAttacks, "TotalBlockedOverheadAttacks"},
		{MK12::Enums::FGStatID::TotalPunishes, "TotalPunishes"},
		{MK12::Enums::FGStatID::TotalReversals, "TotalReversals"},
		{MK12::Enums::FGStatID::TotalKomboBreakers, "TotalKomboBreakers"},
		{MK12::Enums::FGStatID::TotalThrowsAttempted, "TotalThrowsAttempted"},
		{MK12::Enums::FGStatID::ThrowsConnected, "ThrowsConnected"},
		{MK12::Enums::FGStatID::TotalThrowEscapes, "TotalThrowEscapes"},
		{MK12::Enums::FGStatID::KameoThrows, "KameoThrows"},
		{MK12::Enums::FGStatID::LastHitLanded, "LastHitLanded"},
		{MK12::Enums::FGStatID::TotalSupermoves, "TotalSupermoves"},
		{MK12::Enums::FGStatID::TotalSupermovesConnected, "TotalSupermovesConnected"},
		{MK12::Enums::FGStatID::TotalSupermeterUsed, "TotalSupermeterUsed"},
		{MK12::Enums::FGStatID::TotalSupermeterSegmentsUsed, "TotalSupermeterSegmentsUsed"},
		{MK12::Enums::FGStatID::TotalKameoSupermovesConnected, "TotalKameoSupermovesConnected"},
		{MK12::Enums::FGStatID::TotalKameoSupermovesUsed, "TotalKameoSupermovesUsed"},
		{MK12::Enums::FGStatID::TotalFatalities, "TotalFatalities"},
		{MK12::Enums::FGStatID::TotalKameoFatalities, "TotalKameoFatalities"},
		{MK12::Enums::FGStatID::TotalBrutalities, "TotalBrutalities"},
		{MK12::Enums::FGStatID::TotalKameoBrutalities, "TotalKameoBrutalities"},
		{MK12::Enums::FGStatID::TotalDeepDishes, "TotalDeepDishes"},
		{MK12::Enums::FGStatID::TotalMercies, "TotalMercies"},
		{MK12::Enums::FGStatID::TotalJumps, "TotalJumps"},
		{MK12::Enums::FGStatID::TotalJumpUps, "TotalJumpUps"},
		{MK12::Enums::FGStatID::TotalJumpAttacks, "TotalJumpAttacks"},
		{MK12::Enums::FGStatID::TotalJumpAttacksConnected, "TotalJumpAttacksConnected"},
		{MK12::Enums::FGStatID::TotalJumpForwardAttacksConnected, "TotalJumpForwardAttacksConnected"},
		{MK12::Enums::FGStatID::TotalGroundAttacks, "TotalGroundAttacks"},
		{MK12::Enums::FGStatID::TotalDucks, "TotalDucks"},
		{MK12::Enums::FGStatID::TotalDuckAttacks, "TotalDuckAttacks"},
		{MK12::Enums::FGStatID::TotalDuckAttacksConnected, "TotalDuckAttacksConnected"},
		{MK12::Enums::FGStatID::TotalPunches, "TotalPunches"},
		{MK12::Enums::FGStatID::TotalFrontPunches, "TotalFrontPunches"},
		{MK12::Enums::FGStatID::TotalFrontPunchesConnected, "TotalFrontPunchesConnected"},
		{MK12::Enums::FGStatID::TotalBackPunches, "TotalBackPunches"},
		{MK12::Enums::FGStatID::TotalBackPunchesConnected, "TotalBackPunchesConnected"},
		{MK12::Enums::FGStatID::TotalJumpPunches, "TotalJumpPunches"},
		{MK12::Enums::FGStatID::TotalJumpPunchesConnected, "TotalJumpPunchesConnected"},
		{MK12::Enums::FGStatID::TotalDuckPunchesConnected, "TotalDuckPunchesConnected"},
		{MK12::Enums::FGStatID::TotalKicks, "TotalKicks"},
		{MK12::Enums::FGStatID::TotalFrontKicks, "TotalFrontKicks"},
		{MK12::Enums::FGStatID::TotalFrontKicksConnected, "TotalFrontKicksConnected"},
		{MK12::Enums::FGStatID::TotalBackKicks, "TotalBackKicks"},
		{MK12::Enums::FGStatID::TotalBackKicksConnected, "TotalBackKicksConnected"},
		{MK12::Enums::FGStatID::TotalJumpKicks, "TotalJumpKicks"},
		{MK12::Enums::FGStatID::TotalJumpKicksConnected, "TotalJumpKicksConnected"},
		{MK12::Enums::FGStatID::TotalDuckKicksConnected, "TotalDuckKicksConnected"},
		{MK12::Enums::FGStatID::TotalSpecialsUsed, "TotalSpecialsUsed"},
		{MK12::Enums::FGStatID::TotalSpecialMovesConnected, "TotalSpecialMovesConnected"},
		{MK12::Enums::FGStatID::TotalEnhancedSpecialMovesUsed, "TotalEnhancedSpecialMovesUsed"},
		{MK12::Enums::FGStatID::TotalEnhancedSpecialMovesConnected, "TotalEnhancedSpecialMovesConnected"},
		{MK12::Enums::FGStatID::TotalUppercuts, "TotalUppercuts"},
		{MK12::Enums::FGStatID::TotalUppercutsConnected, "TotalUppercutsConnected"},
		{MK12::Enums::FGStatID::TotalSweeps, "TotalSweeps"},
		{MK12::Enums::FGStatID::TotalSweepsConnected, "TotalSweepsConnected"},
		{MK12::Enums::FGStatID::TotalKnockDownAttacksConnected, "TotalKnockDownAttacksConnected"},
		{MK12::Enums::FGStatID::TotalLowAttacks, "TotalLowAttacks"},
		{MK12::Enums::FGStatID::TotalLowAttacksConnected, "TotalLowAttacksConnected"},
		{MK12::Enums::FGStatID::TotalMidAttacksConnected, "TotalMidAttacksConnected"},
		{MK12::Enums::FGStatID::TotalHighAttacksConnected, "TotalHighAttacksConnected"},
		{MK12::Enums::FGStatID::TotalOverheadAttacksConnected, "TotalOverheadAttacksConnected"},
		{MK12::Enums::FGStatID::TotalProjectilesFired, "TotalProjectilesFired"},
		{MK12::Enums::FGStatID::TotalProjectilesConnected, "TotalProjectilesConnected"},
		{MK12::Enums::FGStatID::TotalFlipStances, "TotalFlipStances"},
		{MK12::Enums::FGStatID::TotalTaunts, "TotalTaunts"},
		{MK12::Enums::FGStatID::TotalButtonsPressed, "TotalButtonsPressed"},
		{MK12::Enums::FGStatID::TotalObjectsDestroyed, "TotalObjectsDestroyed"},
		{MK12::Enums::FGStatID::TotalTalismansUsed, "TotalTalismansUsed"},
		{MK12::Enums::FGStatID::TotalScore, "TotalScore"},
		{MK12::Enums::FGStatID::CompleteEasyTower, "CompleteEasyTower"},
		{MK12::Enums::FGStatID::CompleteMediumTower, "CompleteMediumTower"},
		{MK12::Enums::FGStatID::CompleteHardTower, "CompleteHardTower"},
		{MK12::Enums::FGStatID::FGStatID_MAX, "FGStatID_Max"},
	};

	std::unordered_map<int, std::string> FloydChallengeMap = {
		{1, "Total Disrespect"},
		{2, "Jumping Gets You Nowhere"},
		{3, "Klean Sweep"},
		{4, "Get Over Here Already"},
		{5, "Flipping Out"},
		{6, "Up & Away"},
		{7, "No Elder God"},
		{8, "I Make The Rules"},
		{9, "No Luna"},
		{10, "Fire & Ice"},
		{11, "Ice & Fire"},
		{12, "Perfect Kouple"},
		{13, "Get The Horns"},
		{14, "Hip Hop 4 Ever"},
		{15, "Yeet!!!"},
		{16, "This Is Where You Fall Down"},
		{17, "Timed Out"},
		{18, "You Suck"},
		{19, "I'm Down Too"},
		{20, "Fists Of Fury"},
		{21, "Kicking It"},
		{22, "Sans Jade"},
		{23, "Losing Is Winning"},
		{24, "Keep Kalm & Finish"},
		{25, "Demonic Duo"},
		{26, "Frosty!!!"},
		{27, "Toasty!!!"},
		{28, "Ka Ballin"},
		{29, "Hat Trick"},
		{30, "Fatal Finish"},
		{31, "You Finish Yet???"},
		{32, "Inner Beast"},
		{33, "Shaolin Monks"},
		{34, "Door Buster"},
		{35, "Climb The Pyramid"},
		{36, "Challenge Accepted"},
		{37, "Quest Keeper"}
	};

	std::unordered_map<Enums::EManualMoveTraceEvent, std::string> EManualMoveTraceEventMap = {
		{ Enums::EManualMoveTraceEvent::None, "None" },
		{ Enums::EManualMoveTraceEvent::DebugTrace, "DebugTrace" },
		{ Enums::EManualMoveTraceEvent::DashForward, "DashForward" },
		{ Enums::EManualMoveTraceEvent::DashBackward, "DashBackward" },
		{ Enums::EManualMoveTraceEvent::JumpUpward, "JumpUpward" },
		{ Enums::EManualMoveTraceEvent::JumpForward, "JumpForward" },
		{ Enums::EManualMoveTraceEvent::JumpBackward, "JumpBackward" },
		{ Enums::EManualMoveTraceEvent::JumpLand, "JumpLand" },
		{ Enums::EManualMoveTraceEvent::Duck, "Duck" },
		{ Enums::EManualMoveTraceEvent::ThrowBackward, "ThrowBackward" },
		{ Enums::EManualMoveTraceEvent::ThrowForward, "ThrowForward" },
		{ Enums::EManualMoveTraceEvent::ThrowBackwardNoBreak, "ThrowBackwardNoBreak" },
		{ Enums::EManualMoveTraceEvent::ThrowForwardNoBreak, "ThrowForwardNoBreak" },
		{ Enums::EManualMoveTraceEvent::ThrowForwardShove, "ThrowForwardShove" },
		{ Enums::EManualMoveTraceEvent::ThrowEscape, "ThrowEscape" },
		{ Enums::EManualMoveTraceEvent::DelayedWakeup, "DelayedWakeup" },
		{ Enums::EManualMoveTraceEvent::DelayedWakeupEnd, "DelayedWakeupEnd" },
		{ Enums::EManualMoveTraceEvent::KameoBreaker, "KameoBreaker" },
		{ Enums::EManualMoveTraceEvent::JumpCancelEX, "JumpCancelEX" },
		{ Enums::EManualMoveTraceEvent::AnyBlock, "AnyBlock" },
		{ Enums::EManualMoveTraceEvent::NormalBlock, "NormalBlock" },
		{ Enums::EManualMoveTraceEvent::PerfectBlock, "PerfectBlock" },
		{ Enums::EManualMoveTraceEvent::BlockWhilePerfectBlockBuff, "BlockWhilePerfectBlockBuff" },
		{ Enums::EManualMoveTraceEvent::UpBlock, "UpBlock" },
		{ Enums::EManualMoveTraceEvent::BlockReactionExit, "BlockReactionExit" },
		{ Enums::EManualMoveTraceEvent::UpBlockStarted, "UpBlockStarted" },
		{ Enums::EManualMoveTraceEvent::BlockLow, "BlockLow" },
		{ Enums::EManualMoveTraceEvent::BlockMid, "BlockMid" },
		{ Enums::EManualMoveTraceEvent::BlockHigh, "BlockHigh" },
		{ Enums::EManualMoveTraceEvent::BlockOverhead, "BlockOverhead" },
		{ Enums::EManualMoveTraceEvent::Reversal, "Reversal" },
		{ Enums::EManualMoveTraceEvent::WakeupAttack, "WakeupAttack" },
		{ Enums::EManualMoveTraceEvent::Getup, "Getup" },
		{ Enums::EManualMoveTraceEvent::Counter, "Counter" },
		{ Enums::EManualMoveTraceEvent::Punish, "Punish" },
		{ Enums::EManualMoveTraceEvent::LowHealth, "LowHealth" },
		{ Enums::EManualMoveTraceEvent::OneSuperMeter, "OneSuperMeter" },
		{ Enums::EManualMoveTraceEvent::OnHitConnected, "OnHitConnected" },
		{ Enums::EManualMoveTraceEvent::FaceButtonPauseRelease, "FaceButtonPauseRelease" },
		{ Enums::EManualMoveTraceEvent::TooCloseWatcher, "TooCloseWatcher" },
		{ Enums::EManualMoveTraceEvent::TooFarWatcher, "TooFarWatcher" },
		{ Enums::EManualMoveTraceEvent::CornerWatcher, "CornerWatcher" },
		{ Enums::EManualMoveTraceEvent::DuckHighAttack, "DuckHighAttack" },
		{ Enums::EManualMoveTraceEvent::DuckUnderFireballStart, "DuckUnderFireballStart" },
		{ Enums::EManualMoveTraceEvent::DuckUnderFireball, "DuckUnderFireball" },
		{ Enums::EManualMoveTraceEvent::JumpOverFireballStart, "JumpOverFireballStart" },
		{ Enums::EManualMoveTraceEvent::JumpOverFireball, "JumpOverFireball" },
		{ Enums::EManualMoveTraceEvent::JumpOverLowFireballStart, "JumpOverLowFireballStart" },
		{ Enums::EManualMoveTraceEvent::JumpOverLowFireball, "JumpOverLowFireball" },
		{ Enums::EManualMoveTraceEvent::P1AttackStarted, "P1AttackStarted" },
		{ Enums::EManualMoveTraceEvent::P1KameoActive, "P1KameoActive" },
		{ Enums::EManualMoveTraceEvent::DelayBlockTrace, "DelayBlockTrace" },
		{ Enums::EManualMoveTraceEvent::ElementalBallSharedHitCallback, "ElementalBallSharedHitCallback" },
		{ Enums::EManualMoveTraceEvent::GenericTalismanAttackStarted, "GenericTalismanAttackStarted" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_Intro_1A, "FrameDataColor_Intro_1A" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_Intro_1B, "FrameDataColor_Intro_1B" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_Intro_2A, "FrameDataColor_Intro_2A" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_Intro_2B, "FrameDataColor_Intro_2B" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_Intro_3A, "FrameDataColor_Intro_3A" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_Intro_3B, "FrameDataColor_Intro_3B" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_Intro_4, "FrameDataColor_Intro_4" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_StartupFast_1, "FrameDataColor_StartupFast_1" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_StartupFast_2, "FrameDataColor_StartupFast_2" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_StartupFast_3, "FrameDataColor_StartupFast_3" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_StartupSlow_1, "FrameDataColor_StartupSlow_1" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_StartupSlow_2, "FrameDataColor_StartupSlow_2" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_StartupSlow_3, "FrameDataColor_StartupSlow_3" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_ActiveFast_1, "FrameDataColor_ActiveFast_1" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_ActiveFast_2, "FrameDataColor_ActiveFast_2" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_ActiveFast_3, "FrameDataColor_ActiveFast_3" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_ActiveSlow_1, "FrameDataColor_ActiveSlow_1" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_ActiveSlow_2, "FrameDataColor_ActiveSlow_2" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_ActiveSlow_3, "FrameDataColor_ActiveSlow_3" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_ActiveSlow_3B, "FrameDataColor_ActiveSlow_3B" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_RecoverFast_1, "FrameDataColor_RecoverFast_1" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_RecoverFast_2, "FrameDataColor_RecoverFast_2" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_RecoverFast_3, "FrameDataColor_RecoverFast_3" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_RecoverSlow_1, "FrameDataColor_RecoverSlow_1" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_RecoverSlow_2, "FrameDataColor_RecoverSlow_2" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_RecoverSlow_3, "FrameDataColor_RecoverSlow_3" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_HitAdv_1, "FrameDataColor_HitAdv_1" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_HitAdv_2, "FrameDataColor_HitAdv_2" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_HitAdv_3, "FrameDataColor_HitAdv_3" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_HitAdv_4, "FrameDataColor_HitAdv_4" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_HitAdv_5, "FrameDataColor_HitAdv_5" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_HitAdv_6, "FrameDataColor_HitAdv_6" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_HitAdv_7, "FrameDataColor_HitAdv_7" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_HitAdv_8, "FrameDataColor_HitAdv_8" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_HitAdv_9, "FrameDataColor_HitAdv_9" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_BlockAdv_1, "FrameDataColor_BlockAdv_1" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_BlockAdv_2, "FrameDataColor_BlockAdv_2" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_BlockAdv_3, "FrameDataColor_BlockAdv_3" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_BlockAdv_4, "FrameDataColor_BlockAdv_4" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_BlockAdv_5, "FrameDataColor_BlockAdv_5" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_BlockAdv_6, "FrameDataColor_BlockAdv_6" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_BlockAdv_7, "FrameDataColor_BlockAdv_7" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_BlockAdv_8, "FrameDataColor_BlockAdv_8" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_BlockAdv_9, "FrameDataColor_BlockAdv_9" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_Punish_1, "FrameDataColor_Punish_1" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_Victim_Done, "FrameDataColor_Victim_Done" },
		{ Enums::EManualMoveTraceEvent::FrameDataColor_Attacker_Done, "FrameDataColor_Attacker_Done" },
		{ Enums::EManualMoveTraceEvent::CWHints_ComboWindowTicksDone, "CWHints_ComboWindowTicksDone" },
		{ Enums::EManualMoveTraceEvent::CWHints_ComboStarted, "CWHints_ComboStarted" },
		{ Enums::EManualMoveTraceEvent::CWHints_ComboHit, "CWHints_ComboHit" },
		{ Enums::EManualMoveTraceEvent::TutorialDemoPlaybackStart, "TutorialDemoPlaybackStart" },
		{ Enums::EManualMoveTraceEvent::AshrahBlockHellWave, "AshrahBlockHellWave" },
		{ Enums::EManualMoveTraceEvent::AshrahGhostSliceTeleport, "AshrahGhostSliceTeleport" },
		{ Enums::EManualMoveTraceEvent::AshrahGhostSliceTeleportAir, "AshrahGhostSliceTeleportAir" },
		{ Enums::EManualMoveTraceEvent::BarakaAirBladeSpinEnderHit, "BarakaAirBladeSpinEnderHit" },
		{ Enums::EManualMoveTraceEvent::BarakaStabStabAntiAirHit, "BarakaStabStabAntiAirHit" },
		{ Enums::EManualMoveTraceEvent::CageDown1Done, "CageDown1Done" },
		{ Enums::EManualMoveTraceEvent::CageDown3Done, "CageDown3Done" },
		{ Enums::EManualMoveTraceEvent::CageShadowKickHypeBuild, "CageShadowKickHypeBuild" },
		{ Enums::EManualMoveTraceEvent::CageNutPunchHypeBuild, "CageNutPunchHypeBuild" },
		{ Enums::EManualMoveTraceEvent::CageRisingElbowHypeBuild, "CageRisingElbowHypeBuild" },
		{ Enums::EManualMoveTraceEvent::HavikChestProjectileEXHit, "HavikChestProjectileEXHit" },
		{ Enums::EManualMoveTraceEvent::KenshiSoulSeperateStart, "KenshiSoulSeperateStart" },
		{ Enums::EManualMoveTraceEvent::KenshiSpiritAttack1Hit, "KenshiSpiritAttack1Hit" },
		{ Enums::EManualMoveTraceEvent::KenshiSpiritAttack2_1stHit, "KenshiSpiritAttack2_1stHit" },
		{ Enums::EManualMoveTraceEvent::KenshiSpiritAttack2_2ndHit, "KenshiSpiritAttack2_2ndHit" },
		{ Enums::EManualMoveTraceEvent::KenshiSpiritAttack3Hit, "KenshiSpiritAttack3Hit" },
		{ Enums::EManualMoveTraceEvent::KenshiSpiritAttack4_1stHit, "KenshiSpiritAttack4_1stHit" },
		{ Enums::EManualMoveTraceEvent::KenshiSpiritAttack4_2ndHit, "KenshiSpiritAttack4_2ndHit" },
		{ Enums::EManualMoveTraceEvent::KenshiSpiritAttack4_3rdHit, "KenshiSpiritAttack4_3rdHit" },
		{ Enums::EManualMoveTraceEvent::KenshiSpiritAttack1Block, "KenshiSpiritAttack1Block" },
		{ Enums::EManualMoveTraceEvent::KenshiSpiritAttack2_1stBlock, "KenshiSpiritAttack2_1stBlock" },
		{ Enums::EManualMoveTraceEvent::KenshiSpiritAttack2_2ndBlock, "KenshiSpiritAttack2_2ndBlock" },
		{ Enums::EManualMoveTraceEvent::KenshiSpiritAttack3Block, "KenshiSpiritAttack3Block" },
		{ Enums::EManualMoveTraceEvent::KenshiSpiritAttack4_1stBlock, "KenshiSpiritAttack4_1stBlock" },
		{ Enums::EManualMoveTraceEvent::KenshiSpiritAttack4_2ndBlock, "KenshiSpiritAttack4_2ndBlock" },
		{ Enums::EManualMoveTraceEvent::KenshiSpiritAttack4_3rdBlock, "KenshiSpiritAttack4_3rdBlock" },
		{ Enums::EManualMoveTraceEvent::KitanaVortexTrapHit, "KitanaVortexTrapHit" },
		{ Enums::EManualMoveTraceEvent::LiMeiLanternPopHit, "LiMeiLanternPopHit" },
		{ Enums::EManualMoveTraceEvent::LiMeiTowards3Cancel, "LiMeiTowards3Cancel" },
		{ Enums::EManualMoveTraceEvent::LiuKangFireballBreakout, "LiuKangFireballBreakout" },
		{ Enums::EManualMoveTraceEvent::ReikoParryEXHit, "ReikoParryEXHit" },
		{ Enums::EManualMoveTraceEvent::ReptileForceBallSlow, "ReptileForceBallSlow" },
		{ Enums::EManualMoveTraceEvent::ReptileForceBallMid, "ReptileForceBallMid" },
		{ Enums::EManualMoveTraceEvent::ReptileForceBallFast, "ReptileForceBallFast" },
		{ Enums::EManualMoveTraceEvent::ReptileForceBallEXSlow, "ReptileForceBallEXSlow" },
		{ Enums::EManualMoveTraceEvent::ReptileForceBallEXMid, "ReptileForceBallEXMid" },
		{ Enums::EManualMoveTraceEvent::ReptileForceBallEXFast, "ReptileForceBallEXFast" },
		{ Enums::EManualMoveTraceEvent::ShangAgeMorph, "ShangAgeMorph" },
		{ Enums::EManualMoveTraceEvent::ShangSkullStraightHit, "ShangSkullStraightHit" },
		{ Enums::EManualMoveTraceEvent::ShangSkull3DHit, "ShangSkull3DHit" },
		{ Enums::EManualMoveTraceEvent::ShangSkullVertHit, "ShangSkullVertHit" },
		{ Enums::EManualMoveTraceEvent::SubzeroMultiIceCloneEX, "SubzeroMultiIceCloneEX" },
		{ Enums::EManualMoveTraceEvent::SubzeroMultiIceCloneEXHit, "SubzeroMultiIceCloneEXHit" },
		{ Enums::EManualMoveTraceEvent::TanyaExtraDashCancel, "TanyaExtraDashCancel" },
		{ Enums::EManualMoveTraceEvent::TanyaParryHit, "TanyaParryHit" },
		{ Enums::EManualMoveTraceEvent::TanyaParryEXHit, "TanyaParryEXHit" },
		{ Enums::EManualMoveTraceEvent::TanyaPerfectParryHit, "TanyaPerfectParryHit" },
		{ Enums::EManualMoveTraceEvent::TanyaRiposteHit, "TanyaRiposteHit" },
		{ Enums::EManualMoveTraceEvent::TanyaEvadeFromParry, "TanyaEvadeFromParry" },
		{ Enums::EManualMoveTraceEvent::TanyaEvadeFromParryEX, "TanyaEvadeFromParryEX" },
		{ Enums::EManualMoveTraceEvent::CyraxSelfDestructHit, "CyraxSelfDestructHit" },
		{ Enums::EManualMoveTraceEvent::FrostIceRushHit, "FrostIceRushHit" },
		{ Enums::EManualMoveTraceEvent::KanoEyeLaserHit, "KanoEyeLaserHit" },
		{ Enums::EManualMoveTraceEvent::SonyaRingCharge, "SonyaRingCharge" },
		{ Enums::EManualMoveTraceEvent::SonyaRingHit, "SonyaRingHit" },
		{ Enums::EManualMoveTraceEvent::ErmacTeleportHit, "ErmacTeleportHit" },
		{ Enums::EManualMoveTraceEvent::ErmacTeleportGroundToAir, "ErmacTeleportGroundToAir" },
		{ Enums::EManualMoveTraceEvent::ErmacTeleportAirToGround, "ErmacTeleportAirToGround" },
		{ Enums::EManualMoveTraceEvent::ShujinkoAUp, "ShujinkoAUp" },
		{ Enums::EManualMoveTraceEvent::ShujinkoADown, "ShujinkoADown" },
		{ Enums::EManualMoveTraceEvent::ShujinkoBUp, "ShujinkoBUp" },
		{ Enums::EManualMoveTraceEvent::ShujinkoBDown, "ShujinkoBDown" },
		{ Enums::EManualMoveTraceEvent::PeacemakerAntiGravAttack, "PeacemakerAntiGravAttack" },
		{ Enums::EManualMoveTraceEvent::TakedaStolenStarPlant, "TakedaStolenStarPlant" },
		{ Enums::EManualMoveTraceEvent::TakedaStolenStarDetonate, "TakedaStolenStarDetonate" },
		{ Enums::EManualMoveTraceEvent::TakedaStolenRekka1, "TakedaStolenRekka1" },
		{ Enums::EManualMoveTraceEvent::TakedaStolenRekka2, "TakedaStolenRekka2" },
		{ Enums::EManualMoveTraceEvent::TakedaStolenRekka3, "TakedaStolenRekka3" },
		{ Enums::EManualMoveTraceEvent::TakedaStolenRekka3EX, "TakedaStolenRekka3EX" },
		{ Enums::EManualMoveTraceEvent::CyraxAway24_1stHit, "CyraxAway24_1stHit" },
		{ Enums::EManualMoveTraceEvent::CyraxAway24_2ndHit, "CyraxAway24_2ndHit" },
		{ Enums::EManualMoveTraceEvent::GhostfaceKnifeTossHit, "GhostfaceKnifeTossHit" },
		{ Enums::EManualMoveTraceEvent::GhostfaceKnifeTossMBHit, "GhostfaceKnifeTossMBHit" },
		{ Enums::EManualMoveTraceEvent::SarrenaKAMJataakasBlessing30, "SarrenaKAMJataakasBlessing30" },
		{ Enums::EManualMoveTraceEvent::SubZeroKAMDeepFreeze, "SubZeroKAMDeepFreeze" },
		{ Enums::EManualMoveTraceEvent::SubZeroCharIceBall, "SubZeroCharIceBall" },
		{ Enums::EManualMoveTraceEvent::ScorpionKAMFireAura, "ScorpionKAMFireAura" },
		{ Enums::EManualMoveTraceEvent::CyraxKAMNet, "CyraxKAMNet" },
		{ Enums::EManualMoveTraceEvent::CyraxCharBomb, "CyraxCharBomb" },
		{ Enums::EManualMoveTraceEvent::CyraxCharGoo, "CyraxCharGoo" },
		{ Enums::EManualMoveTraceEvent::JohnnyNutPunch, "JohnnyNutPunch" },
		{ Enums::EManualMoveTraceEvent::JanetNutPunch, "JanetNutPunch" },
		{ Enums::EManualMoveTraceEvent::MileenaBallRoll, "MileenaBallRoll" },
		{ Enums::EManualMoveTraceEvent::KungLaoKAMGroundHat, "KungLaoKAMGroundHat" },
		{ Enums::EManualMoveTraceEvent::KungLaoKAMOrbitHat, "KungLaoKAMOrbitHat" },
		{ Enums::EManualMoveTraceEvent::KungLaoCharHatToss, "KungLaoCharHatToss" },
		{ Enums::EManualMoveTraceEvent::KungLaoCharHatTossHigh, "KungLaoCharHatTossHigh" },
		{ Enums::EManualMoveTraceEvent::KungLaoCharHatTossAir, "KungLaoCharHatTossAir" },
		{ Enums::EManualMoveTraceEvent::ConanAway2Charge1, "ConanAway2Charge1" },
		{ Enums::EManualMoveTraceEvent::ConanAway2Charge2, "ConanAway2Charge2" },
		{ Enums::EManualMoveTraceEvent::RisingAttackDownHit, "RisingAttackDownHit" },
		{ Enums::EManualMoveTraceEvent::EManualMoveTraceEvent_MAX, "EManualMoveTraceEvent_MAX" },
	};

	std::unordered_map<Enums::MKProfileStatID, std::string> MKProfileStatIDMap = {
		{Enums::MKProfileStatID::INVALID, "INVALID"},
		{Enums::MKProfileStatID::ProfileStatVersion, "ProfileStatVersion"},
		{Enums::MKProfileStatID::CurrentSeason, "CurrentSeason"},
		{Enums::MKProfileStatID::TotalPlayTime, "TotalPlayTime"},
		{Enums::MKProfileStatID::TotalBloodSpilt, "TotalBloodSpilt"},
		{Enums::MKProfileStatID::TotalFlawlessRounds, "TotalFlawlessRounds"},
		{Enums::MKProfileStatID::TotalTowersKompleted, "TotalTowersKompleted"},
		{Enums::MKProfileStatID::TotalDailyQuestChallengesKompleted, "TotalDailyQuestChallengesKompleted"},
		{Enums::MKProfileStatID::TotalWeeklyQuestChallengesKompleted, "TotalWeeklyQuestChallengesKompleted"},
		{Enums::MKProfileStatID::TotalFatalities, "TotalFatalities"},
		{Enums::MKProfileStatID::TotalDifferentFatalities, "TotalDifferentFatalities"},
		{Enums::MKProfileStatID::TotalBrutalities, "TotalBrutalities"},
		{Enums::MKProfileStatID::TotalFighterFatalities, "TotalFighterFatalities"},
		{Enums::MKProfileStatID::TotalFighterBrutalities, "TotalFighterBrutalities"},
		{Enums::MKProfileStatID::TotalFighterDeepDishes, "TotalFighterDeepDishes"},
		{Enums::MKProfileStatID::TotalKameoFatalities, "TotalKameoFatalities"},
		{Enums::MKProfileStatID::TotalKameoBrutalities, "TotalKameoBrutalities"},
		{Enums::MKProfileStatID::StoryCompletionPercentage, "StoryCompletionPercentage"},
		{Enums::MKProfileStatID::StoryCompletionHighestDifficulty, "StoryCompletionHighestDifficulty"},
		{Enums::MKProfileStatID::Ch15OpponentsDefeated, "Ch15OpponentsDefeated"},
		{Enums::MKProfileStatID::TutorialCompletionPercentage, "TutorialCompletionPercentage"},
		{Enums::MKProfileStatID::TutorialBaseCompletionPercentage, "TutorialBaseCompletionPercentage"},
		{Enums::MKProfileStatID::TotalRosterCharactersUsed, "TotalRosterCharactersUsed"},
		{Enums::MKProfileStatID::TotalKameoCharactersUsed, "TotalKameoCharactersUsed"},
		{Enums::MKProfileStatID::TotalTimeSpentInMapMode, "TotalTimeSpentInMapMode"},
		{Enums::MKProfileStatID::MapModeProfileLevel, "MapModeProfileLevel"},
		{Enums::MKProfileStatID::TotalMapModeKompletions, "TotalMapModeKompletions"},
		{Enums::MKProfileStatID::TotalMapModeEncountersCompleted, "TotalMapModeEncountersCompleted"},
		{Enums::MKProfileStatID::TotalMapModeEncountersCompletedAndRecompleted, "TotalMapModeEncountersCompletedAndRecompleted"},
		{Enums::MKProfileStatID::TotalMapModeCurrencySpent, "TotalMapModeCurrencySpent"},
		{Enums::MKProfileStatID::TotalItemsSold, "TotalItemsSold"},
		{Enums::MKProfileStatID::TotalTalismansForged, "TotalTalismansForged"},
		{Enums::MKProfileStatID::TotalMapModeRelicsCollected, "TotalMapModeRelicsCollected"},
		{Enums::MKProfileStatID::TotalGatewayTowersKompleted, "TotalGatewayTowersKompleted"},
		{Enums::MKProfileStatID::TotalMapModeTYMEncountersCompleted, "TotalMapModeTYMEncountersCompleted"},
		{Enums::MKProfileStatID::TotalMapModeTYMEncountersCompletedAndRecompleted, "TotalMapModeTYMEncountersCompletedAndRecompleted"},
		{Enums::MKProfileStatID::TotalTimesDefeatingFloyd, "TotalTimesDefeatingFloyd"},
		{Enums::MKProfileStatID::ProfileStat9001, "ProfileStat9001"},
		{Enums::MKProfileStatID::ProfileStat9002, "TotalTimesEncounteredFloyd"}, // Encouter or defeat
		{Enums::MKProfileStatID::ProfileStat9003, "UnlockedFloydCluesBitMask"},
		{Enums::MKProfileStatID::ProfileStat9004, "ProfileStat9004"}, // Became 12 after defeating him 9 after losing // 11 means in fight
		{Enums::MKProfileStatID::ProfileStat9005, "TotalTimesDefeatedFloyd"}, // Encouter or defeat
		{Enums::MKProfileStatID::ProfileStat9006, "TotalMatchesSinceLastSeenFloyd"},
		{Enums::MKProfileStatID::ProfileStat9100, "TotalFatalitiesSinceLastFloydFight"},
		{Enums::MKProfileStatID::ProfileStat9101, "TotalAnimalitiesSinceLastFloydFight"},
		{Enums::MKProfileStatID::ProfileStat9102, "TimesFinishedKlassicLadderAsLiuKangKungLao"},
		{Enums::MKProfileStatID::ProfileStat9103, "TimesBeatenBarakasTestYourMightStoryChallenge"},
		{Enums::MKProfileStatID::ProfileStat9104, "Chapter15CurrentLocation"},
		{Enums::MKProfileStatID::ProfileStat9105, "TowersOfTimeChallengePointsEarned"},
		{Enums::MKProfileStatID::ProfileStat9106, "ChallengesCompleted"},
		{Enums::MKProfileStatID::TotalMapModeTitanBossesDefeated, "TotalMapModeTitanBossesDefeated"},
		{Enums::MKProfileStatID::TotalMapModeSecretFightsDefeated, "TotalMapModeSecretFightsDefeated"},
		{Enums::MKProfileStatID::TotalMapModeAmbushesDefeated, "TotalMapModeAmbushesDefeated"},
		{Enums::MKProfileStatID::TotalScavengerHuntsKompleted, "TotalScavengerHuntsKompleted"},
		{Enums::MKProfileStatID::TotalMilesTraveled, "TotalMilesTraveled"},
		{Enums::MKProfileStatID::TotalMapModeObstructionsCleared, "TotalMapModeObstructionsCleared"},
		{Enums::MKProfileStatID::TotalMapModeChestsOpened, "TotalMapModeChestsOpened"},
		{Enums::MKProfileStatID::TotalMapModeCharactersUsed, "TotalMapModeCharactersUsed"},
		{Enums::MKProfileStatID::TotalMapModeKameosUsed, "TotalMapModeKameosUsed"},
		{Enums::MKProfileStatID::TotalDifferentMultiverseOpponentsDefeated, "TotalDifferentMultiverseOpponentsDefeated"},
		{Enums::MKProfileStatID::RankedSetsLeaderboardPosition, "RankedSetsLeaderboardPosition"},
		{Enums::MKProfileStatID::RankedSetsWon, "RankedSetsWon"},
		{Enums::MKProfileStatID::TotalRankedSetsPlayed, "TotalRankedSetsPlayed"},
		{Enums::MKProfileStatID::LongestWinStreakRankedMatches, "LongestWinStreakRankedMatches"},
		{Enums::MKProfileStatID::KasualMatchesLeaderboardPosition, "KasualMatchesLeaderboardPosition"},
		{Enums::MKProfileStatID::KasualMatchesWon, "KasualMatchesWon"},
		{Enums::MKProfileStatID::TotalKasualMatchesPlayed, "TotalKasualMatchesPlayed"},
		{Enums::MKProfileStatID::LongestWinStreakKasualMatches, "LongestWinStreakKasualMatches"},
		{Enums::MKProfileStatID::KOTHWins, "KOTHWins"},
		{Enums::MKProfileStatID::KingDethrones, "KingDethrones"},
		{Enums::MKProfileStatID::LongestKOTHWinStreak, "LongestKOTHWinStreak"},
		{Enums::MKProfileStatID::TotalRespect, "TotalRespect"},
		{Enums::MKProfileStatID::TotalRespectPointsReceived, "TotalRespectPointsReceived"},
		{Enums::MKProfileStatID::TotalTimeOnlineMatches, "TotalTimeOnlineMatches"},
		{Enums::MKProfileStatID::TotalOnlineQuitalities, "TotalOnlineQuitalities"},
		{Enums::MKProfileStatID::TotalOnlineFatalities, "TotalOnlineFatalities"},
		{Enums::MKProfileStatID::TotalOnlineBrutalities, "TotalOnlineBrutalities"},
		{Enums::MKProfileStatID::TotalOnlineFighterFatalities, "TotalOnlineFighterFatalities"},
		{Enums::MKProfileStatID::TotalOnlineFighterBrutalities, "TotalOnlineFighterBrutalities"},
		{Enums::MKProfileStatID::TotalOnlineFighterDeepDishes, "TotalOnlineFighterDeepDishes"},
		{Enums::MKProfileStatID::TotalOnlineFighterCharacterFatalities, "TotalOnlineFighterCharacterFatalities"},
		{Enums::MKProfileStatID::TotalOnlineFighterCharacterBrutalities, "TotalOnlineFighterCharacterBrutalities"},
		{Enums::MKProfileStatID::TotalOnlineFighterCharacterDeepDishes, "TotalOnlineFighterCharacterDeepDishes"},
		{Enums::MKProfileStatID::TotalOnlineKameoFatalities, "TotalOnlineKameoFatalities"},
		{Enums::MKProfileStatID::TotalOnlineKameoBrutalities, "TotalOnlineKameoBrutalities"},
		{Enums::MKProfileStatID::TotalOnlineKameoCharacterFatalities, "TotalOnlineKameoCharacterFatalities"},
		{Enums::MKProfileStatID::TotalOnlineKameoCharacterBrutalities, "TotalOnlineKameoCharacterBrutalities"},
		{Enums::MKProfileStatID::TotalOnlineCharactersUsed, "TotalOnlineCharactersUsed"},
		{Enums::MKProfileStatID::TotalOnlineKameosUsed, "TotalOnlineKameosUsed"},
		{Enums::MKProfileStatID::TotalCharacterFighterFatalities, "TotalCharacterFighterFatalities"},
		{Enums::MKProfileStatID::TotalCharacterFighterBrutalities, "TotalCharacterFighterBrutalities"},
		{Enums::MKProfileStatID::TotalCharacterFighterDeepDishes, "TotalCharacterFighterDeepDishes"},
		{Enums::MKProfileStatID::TotalCharacterFighterOnlineUsage, "TotalCharacterFighterOnlineUsage"},
		{Enums::MKProfileStatID::TotalCharacterFighterWinPercentage, "TotalCharacterFighterWinPercentage"},
		{Enums::MKProfileStatID::TotalCharacterKameoFatalities, "TotalCharacterKameoFatalities"},
		{Enums::MKProfileStatID::TotalCharacterKameoBrutalities, "TotalCharacterKameoBrutalities"},
		{Enums::MKProfileStatID::TotalCharacterKameoOnlineUsage, "TotalCharacterKameoOnlineUsage"},
		{Enums::MKProfileStatID::TotalCharacterKameoWinPercentage, "TotalCharacterKameoWinPercentage"},
		{Enums::MKProfileStatID::TotalUniqueKameoCharactersUnlocked, "TotalUniqueKameoCharactersUnlocked"},
		{Enums::MKProfileStatID::SecondsSpentInPracticeMode, "SecondsSpentInPracticeMode"},
		{Enums::MKProfileStatID::TotalCharacterTutorialsKompleted, "TotalCharacterTutorialsKompleted"},
		{Enums::MKProfileStatID::TotalTalismansUsed, "TotalTalismansUsed"},
		{Enums::MKProfileStatID::TotalRelicsEquipped, "TotalRelicsEquipped"},
		{Enums::MKProfileStatID::CharacterMasteryLevel, "CharacterMasteryLevel"},
		{Enums::MKProfileStatID::KameoMasteryLevel, "KameoMasteryLevel"},
		{Enums::MKProfileStatID::TotalMapModeMajorBossesDefeated, "TotalMapModeMajorBossesDefeated"},
		{Enums::MKProfileStatID::TotalMapModeFinalBossesDefeated, "TotalMapModeFinalBossesDefeated"},
		{Enums::MKProfileStatID::HighestTowerOfTimeScore, "HighestTowerOfTimeScore"},
		{Enums::MKProfileStatID::TotalCharacterKlassicTowersKompleted, "TotalCharacterKlassicTowersKompleted"},
		{Enums::MKProfileStatID::CH15Sequence1Completions, "CH15Sequence1Completions"},
		{Enums::MKProfileStatID::CH15Sequence2Completions, "CH15Sequence2Completions"},
		{Enums::MKProfileStatID::CH15Sequence3Completions, "CH15Sequence3Completions"},
		{Enums::MKProfileStatID::CH15Sequence4Completions, "CH15Sequence4Completions"},
		{Enums::MKProfileStatID::CH15Sequence5Completions, "CH15Sequence5Completions"},
		{Enums::MKProfileStatID::CH15Sequence6Completions, "CH15Sequence6Completions"},
		{Enums::MKProfileStatID::CH15Sequence7Completions, "CH15Sequence7Completions"},
		{Enums::MKProfileStatID::CH15Sequence8Completions, "CH15Sequence8Completions"},
		{Enums::MKProfileStatID::CH15Sequence9Completions, "CH15Sequence9Completions"},
		{Enums::MKProfileStatID::CH15Sequence10Completions, "CH15Sequence10Completions"},
		{Enums::MKProfileStatID::CH15Sequence11Completions, "CH15Sequence11Completions"},
		{Enums::MKProfileStatID::CH15Sequence12Completions, "CH15Sequence12Completions"},
		{Enums::MKProfileStatID::CH15Sequence13Completions, "CH15Sequence13Completions"},
		{Enums::MKProfileStatID::TotalKoinsSpent, "TotalKoinsSpent"},
		{Enums::MKProfileStatID::TotalKoinsDonatedToShrine, "TotalKoinsDonatedToShrine"},
		{Enums::MKProfileStatID::TotalMapModeTutorialsCompleted, "TotalMapModeTutorialsCompleted"},
		{Enums::MKProfileStatID::TotalLadderEndingsUnlocked, "TotalLadderEndingsUnlocked"},
		{Enums::MKProfileStatID::TotalKeysUsed, "TotalKeysUsed"},
		{Enums::MKProfileStatID::TotalSingleUseItemsUsed, "TotalSingleUseItemsUsed"},
		{Enums::MKProfileStatID::RosterMasteriesAllComplete, "RosterMasteriesAllComplete"},
		{Enums::MKProfileStatID::KameoMasteriesAllComplete, "KameoMasteriesAllComplete"},
		{Enums::MKProfileStatID::RosterAndKameoMasteriesAllComplete, "RosterAndKameoMasteriesAllComplete"},
		{Enums::MKProfileStatID::TotalCharacterBloodSpilt, "TotalCharacterBloodSpilt"},
		{Enums::MKProfileStatID::TotalOnlineMercies, "TotalOnlineMercies"},
		{Enums::MKProfileStatID::TotalKrossplayMatchesWon, "TotalKrossplayMatchesWon"},
		{Enums::MKProfileStatID::TotalKombatKardsViewed, "TotalKombatKardsViewed"},
		{Enums::MKProfileStatID::TotalCharacterFighterFightsWon, "TotalCharacterFighterFightsWon"},
		{Enums::MKProfileStatID::TotalCharacterFighterOnlineFightsWon, "TotalCharacterFighterOnlineFightsWon"},
		{Enums::MKProfileStatID::TotalCharacterKameoFightsWon, "TotalCharacterKameoFightsWon"},
		{Enums::MKProfileStatID::TotalCharacterKameoOnlineFightsWon, "TotalCharacterKameoOnlineFightsWon"},
		{Enums::MKProfileStatID::FightsCompleted, "FightsCompleted"},
		{Enums::MKProfileStatID::TotalFlawlessVictories, "TotalFlawlessVictories"},
		{Enums::MKProfileStatID::FightsWon, "FightsWon"},
		{Enums::MKProfileStatID::FightsLost, "FightsLost"},
		{Enums::MKProfileStatID::TotalMercies, "TotalMercies"},
		{Enums::MKProfileStatID::TotalQuitalities, "TotalQuitalities"},
		{Enums::MKProfileStatID::TotalSupermoves, "TotalSupermoves"},
		{Enums::MKProfileStatID::TotalSupermovesConnected, "TotalSupermovesConnected"},
		{Enums::MKProfileStatID::TotalRoundsWonWithSuperMoves, "TotalRoundsWonWithSuperMoves"},
		{Enums::MKProfileStatID::TotalPunishes, "TotalPunishes"},
		{Enums::MKProfileStatID::TotalDamagePerformed, "TotalDamagePerformed"},
		{Enums::MKProfileStatID::LongestWinStreak, "LongestWinStreak"},
		{Enums::MKProfileStatID::TotalThrowsAttempted, "TotalThrowsAttempted"},
		{Enums::MKProfileStatID::TotalParryAttempted, "TotalParryAttempted"},
		{Enums::MKProfileStatID::TotalStatPointsSpent, "TotalStatPointsSpent"},
		{Enums::MKProfileStatID::TotalMapModeMiniBossesDefeated, "TotalMapModeMiniBossesDefeated"},
		{Enums::MKProfileStatID::TotalMapModeSeasonalBossesDefeated, "TotalMapModeSeasonalBossesDefeated"},
		{Enums::MKProfileStatID::TotalMapModeTowersCompleted, "TotalMapModeTowersCompleted"},
		{Enums::MKProfileStatID::TotalMapModePercentCompletion, "TotalMapModePercentCompletion"},
		{Enums::MKProfileStatID::PermanentMesaNodesComplete, "PermanentMesaNodesComplete"},
		{Enums::MKProfileStatID::EventBasedMesaNodesComplete, "EventBasedMesaNodesComplete"},
		{Enums::MKProfileStatID::PermanentMesaNodesCompletePercentage, "PermanentMesaNodesCompletePercentage"},
		{Enums::MKProfileStatID::EventBasedMesaNodesCompletePercentage, "EventBasedMesaNodesCompletePercentage"},
		{Enums::MKProfileStatID::TotalMapModeHourlyChallengesCompleted, "TotalMapModeHourlyChallengesCompleted"},
		{Enums::MKProfileStatID::TotalMapModeDailyChallengesCompleted, "TotalMapModeDailyChallengesCompleted"},
		{Enums::MKProfileStatID::TotalMapModeWeeklyChallengesCompleted, "TotalMapModeWeeklyChallengesCompleted"},
		{Enums::MKProfileStatID::TotalSurvivalEncountersCompleted, "TotalSurvivalEncountersCompleted"},
		{Enums::MKProfileStatID::TotalTalismansRecharged, "TotalTalismansRecharged"},
		{Enums::MKProfileStatID::TotalQuestKardsKompleted, "TotalQuestKardsKompleted"},
		{Enums::MKProfileStatID::TotalQuestChallengesKompleted, "TotalQuestChallengesKompleted"},
		{Enums::MKProfileStatID::TotalOnlineTeaunts, "TotalOnlineTeaunts"},
		{Enums::MKProfileStatID::FTUEKardKompleted, "FTUEKardKompleted"},
		{Enums::MKProfileStatID::TotalTimesEnteredSettings, "TotalTimesEnteredSettings"},
		{Enums::MKProfileStatID::TotalTimesEnteredOfflinePractice, "TotalTimesEnteredOfflinePractice"},
		{Enums::MKProfileStatID::TotalTimesEnteredStory, "TotalTimesEnteredStory"},
		{Enums::MKProfileStatID::KombatLeagueRank, "KombatLeagueRank"},
		{Enums::MKProfileStatID::HighestKombatLeagueRank, "HighestKombatLeagueRank"},
		{Enums::MKProfileStatID::TotalOnlineFights, "TotalOnlineFights"},
		{Enums::MKProfileStatID::OnlineWinPercentage, "OnlineWinPercentage"},
		{Enums::MKProfileStatID::TotalDifferentBrutalities, "TotalDifferentBrutalities"},
		{Enums::MKProfileStatID::DevelopersSlayed, "DevelopersSlayed"},
		{Enums::MKProfileStatID::DevelopersMatched, "DevelopersMatched"},
		{Enums::MKProfileStatID::TotalKomboBreakers, "TotalKomboBreakers"},
		{Enums::MKProfileStatID::TotalKlassicTowersKompleted, "TotalKlassicTowersKompleted"},
		{Enums::MKProfileStatID::TotalItemsPurchasedFromOutworldShops, "TotalItemsPurchasedFromOutworldShops"},
		{Enums::MKProfileStatID::TotalItemsPurchasedFromEarthrealmShops, "TotalItemsPurchasedFromEarthrealmShops"},
		{Enums::MKProfileStatID::TotalSecretFightsUnlocked, "TotalSecretFightsUnlocked"},
		{Enums::MKProfileStatID::TotalTimesWatchedKredits, "TotalTimesWatchedKredits"},
		{Enums::MKProfileStatID::TotalTimesKombatKardUpdated, "TotalTimesKombatKardUpdated"},
		{Enums::MKProfileStatID::TotalSeasonalKreditsSpent, "TotalSeasonalKreditsSpent"},
		{Enums::MKProfileStatID::KombatKardBackground, "KombatKardBackground"},
		{Enums::MKProfileStatID::KombatKardForeground, "KombatKardForeground"},
		{Enums::MKProfileStatID::ReplaySharingOption, "ReplaySharingOption"},
		{Enums::MKProfileStatID::GameEdition, "GameEdition"},
		{Enums::MKProfileStatID::RegionCode, "RegionCode"},
		{Enums::MKProfileStatID::SeasonalTimePlayed, "SeasonalTimePlayed"},
		{Enums::MKProfileStatID::SeasonalBloodSpilt, "SeasonalBloodSpilt"},
		{Enums::MKProfileStatID::SeasonalFlawlessRounds, "SeasonalFlawlessRounds"},
		{Enums::MKProfileStatID::SeasonalRespectPointsReceived, "SeasonalRespectPointsReceived"},
		{Enums::MKProfileStatID::SeasonalTowersKompleted, "SeasonalTowersKompleted"},
		{Enums::MKProfileStatID::SeasonalDailyQuestsKompleted, "SeasonalDailyQuestsKompleted"},
		{Enums::MKProfileStatID::SeasonalWeeklyKompleted, "SeasonalWeeklyKompleted"},
		{Enums::MKProfileStatID::SeasonalFighterFatalities, "SeasonalFighterFatalities"},
		{Enums::MKProfileStatID::SeasonalFighterBrutalities, "SeasonalFighterBrutalities"},
		{Enums::MKProfileStatID::SeasonalFighterDeepDishes, "SeasonalFighterDeepDishes"},
		{Enums::MKProfileStatID::SeasonalKameoFatalities, "SeasonalKameoFatalities"},
		{Enums::MKProfileStatID::SeasonalKameoBrutalities, "SeasonalKameoBrutalities"},
		{Enums::MKProfileStatID::SeasonalQuitalities, "SeasonalQuitalities"},
		{Enums::MKProfileStatID::SeasonalShrineCompletion, "SeasonalShrineCompletion"},
		{Enums::MKProfileStatID::SeasonalMostFightersPlayed, "SeasonalMostFightersPlayed"},
		{Enums::MKProfileStatID::SeasonalMostFightersWon, "SeasonalMostFightersWon"},
		{Enums::MKProfileStatID::SeasonalMostKameosPlayed, "SeasonalMostKameosPlayed"},
		{Enums::MKProfileStatID::SeasonalMostKameosWon, "SeasonalMostKameosWon"},
		{Enums::MKProfileStatID::SeasonalInvasionsTimePlayed, "SeasonalInvasionsTimePlayed"},
		{Enums::MKProfileStatID::SeasonalInvasionsLevel, "SeasonalInvasionsLevel"},
		{Enums::MKProfileStatID::SeasonalInvasionsKompletionPercentage, "SeasonalInvasionsKompletionPercentage"},
		{Enums::MKProfileStatID::SeasonalInvasionsEncountersKompleted, "SeasonalInvasionsEncountersKompleted"},
		{Enums::MKProfileStatID::SeasonalInvasionsKurrencySpent, "SeasonalInvasionsKurrencySpent"},
		{Enums::MKProfileStatID::SeasonalInvasionsItemsSold, "SeasonalInvasionsItemsSold"},
		{Enums::MKProfileStatID::SeasonalTalismansUpgraded, "SeasonalTalismansUpgraded"},
		{Enums::MKProfileStatID::SeasonalRelicsKollected, "SeasonalRelicsKollected"},
		{Enums::MKProfileStatID::SeasonalGatewaysKompleted, "SeasonalGatewaysKompleted"},
		{Enums::MKProfileStatID::SeasonalTYMEncountersKompleted, "SeasonalTYMEncountersKompleted"},
		{Enums::MKProfileStatID::SeasonalTitanBossesDefeated, "SeasonalTitanBossesDefeated"},
		{Enums::MKProfileStatID::SeasonalSecretFightsKompleted, "SeasonalSecretFightsKompleted"},
		{Enums::MKProfileStatID::SeasonalAmbushesDefeated, "SeasonalAmbushesDefeated"},
		{Enums::MKProfileStatID::SeasonalObstructionsCleared, "SeasonalObstructionsCleared"},
		{Enums::MKProfileStatID::SeasonalChestsOpened, "SeasonalChestsOpened"},
		{Enums::MKProfileStatID::SeasonalInvasionsMostFightersPlayed, "SeasonalInvasionsMostFightersPlayed"},
		{Enums::MKProfileStatID::SeasonalInvasionsMostKameosPlayed, "SeasonalInvasionsMostKameosPlayed"},
		{Enums::MKProfileStatID::SeasonalBasedMesaNodesComplete, "SeasonalBasedMesaNodesComplete"},
		{Enums::MKProfileStatID::SeasonalBasedMesaNodesCompletePercentage, "SeasonalBasedMesaNodesCompletePercentage"},
		{Enums::MKProfileStatID::SeasonalBasedMesaTotalChests, "SeasonalBasedMesaTotalChests"},
		{Enums::MKProfileStatID::SeasonalBasedMesaTotalChestsOpened, "SeasonalBasedMesaTotalChestsOpened"},
		{Enums::MKProfileStatID::RankedSeasonPoints, "RankedSeasonPoints"},
		{Enums::MKProfileStatID::RankedSeasonRank, "RankedSeasonRank"},
		{Enums::MKProfileStatID::RankedSeasonExpiration, "RankedSeasonExpiration"},
		{Enums::MKProfileStatID::RankedSeasonLastPlayed, "RankedSeasonLastPlayed"},
		{Enums::MKProfileStatID::LatestMatchPlayTime, "LatestMatchPlayTime"},
		{Enums::MKProfileStatID::MKProfileStatID_MAX, "MKProfileStatID_MAX"},
	};
}