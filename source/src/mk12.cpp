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
	printf("FNameInfoToWStringNoId::%p = %ls\n", (void*)FNameInfo, (wchar_t*)Destination);
	return NameSize;
}

uint64_t __fastcall	MK12::Remake::FNameInfoToWStringWithId(MK12::FNameInfoStruct* FNameInfo, char* Destination)
{
	uint64_t NameSize = FNameInfoToWString(FNameInfo, Destination);
	
	uint64_t DupId = FNameInfo->DuplicateId;
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