#include "IniReader.h"
#include <iostream>
#include <string>
#include <windows.h>
using namespace std;
#pragma warning(disable:4996)
EXTERN_C IMAGE_DOS_HEADER __ImageBase;

CIniReader::CIniReader(char* szFileName)
{
	char moduleName[sizeof(m_szFileName)] = {};
	GetModuleFileNameA((HINSTANCE)&__ImageBase, moduleName, sizeof(moduleName));
	std::string Module = moduleName;
	std::string Folder = Module.substr(0, Module.find_last_of('\\') + 1);
	std::string Name = Module.substr(Folder.size());
	Name = Name.substr(0, Name.find_last_of('.')) + ".ini";
	std::string Path = Folder + (strcmp(szFileName, "") == 0 ? Name : std::string(szFileName));

	memset(m_szFileName, 0x00, sizeof(m_szFileName));
	strncpy(m_szFileName, Path.c_str(), sizeof(m_szFileName) - 1);
}

// One lock for every ini file of this module: writes are exclusive, reads shared
static SRWLOCK IniLock = SRWLOCK_INIT;

// The whole value, however long: the buffer grows until it fits (the API returns size - 1 when it had to cut). Every
// string read goes through here, so no value can come back cut. new[]: the caller owns it
static char* ReadWhole(const char* szSection, const char* szKey, const char* szDefault, const char* szFile)
{
	for (DWORD Size = 256;; Size *= 2)
	{
		char* Result = new char[Size];
		memset(Result, 0x00, Size);
		AcquireSRWLockShared(&IniLock);
		DWORD Length = GetPrivateProfileStringA(szSection, szKey, szDefault, Result, Size, szFile);
		ReleaseSRWLockShared(&IniLock);
		if (Length < Size - 1 || Size >= (1u << 24))
			return Result;
		delete[] Result;
	}
}

DWORD IniReadSection(const char* szSection, std::vector<char>& Buffer, const char* szFile)
{ // The API returns size - 2 when it had to cut the section
	DWORD Length = 0;
	for (size_t Size = 64 * 1024; Size <= (64u << 20); Size *= 2)
	{
		Buffer.assign(Size, 0);
		AcquireSRWLockShared(&IniLock);
		Length = GetPrivateProfileSectionA(szSection, Buffer.data(), (DWORD)Buffer.size(), szFile);
		ReleaseSRWLockShared(&IniLock);
		if (Length < Size - 2)
			break;
	}
	return Length;
}

bool IniHasKey(const char* szSection, const char* szKey, const char* szFile)
{
	char Existing[8] = {};
	AcquireSRWLockShared(&IniLock);
	DWORD Length = GetPrivateProfileStringA(szSection, szKey, "", Existing, sizeof(Existing), szFile);
	ReleaseSRWLockShared(&IniLock);
	return Length != 0;
}

BOOL IniWriteSafe(const char* szSection, const char* szKey, const char* szValue, const char* szFile)
{
	std::string Temp = std::string(szFile) + ".tmp";
	AcquireSRWLockExclusive(&IniLock);
	BOOL Result = FALSE;
	// Start from the current content (no file yet is fine: the write creates it)
	bool Copied = CopyFileA(szFile, Temp.c_str(), FALSE) != FALSE;
	if (Copied || GetFileAttributesA(szFile) == INVALID_FILE_ATTRIBUTES)
	{
		BOOL Written = WritePrivateProfileStringA(szSection, szKey, szValue, Temp.c_str());
		WritePrivateProfileStringA(nullptr, nullptr, nullptr, Temp.c_str()); // Flushes the API's cache to the temp file
		if (Written && MoveFileExA(Temp.c_str(), szFile, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			Result = TRUE;
	}
	if (!Result)
	{
		DeleteFileA(Temp.c_str()); // Temp copy is ours, never the real file
		Result = WritePrivateProfileStringA(szSection, szKey, szValue, szFile); // Fallback: in place
	}
	ReleaseSRWLockExclusive(&IniLock);
	return Result;
}
int CIniReader::ReadInteger(char* szSection, char* szKey, int iDefaultValue)
{
 AcquireSRWLockShared(&IniLock);
 int iResult = GetPrivateProfileInt(szSection,  szKey, iDefaultValue, m_szFileName); 
 ReleaseSRWLockShared(&IniLock);
 return iResult;
}
float CIniReader::ReadFloat(char* szSection, char* szKey, float fltDefaultValue)
{
 char szDefault[64];
 _snprintf(szDefault, sizeof(szDefault), "%f", fltDefaultValue);
 char* szResult = ReadWhole(szSection, szKey, szDefault, m_szFileName);
 float fltResult = (float)atof(szResult);
 delete[] szResult;
 return fltResult;
}
bool CIniReader::ReadBoolean(char* szSection, char* szKey, bool bolDefaultValue)
{
 char* szResult = ReadWhole(szSection, szKey, bolDefaultValue ? "True" : "False", m_szFileName);
 bool bolResult = strcmp(szResult, "True") == 0 || strcmp(szResult, "true") == 0;
 delete[] szResult;
 return bolResult;
}
char* CIniReader::ReadString(char* szSection, char* szKey, const char* szDefaultValue)
{
 return ReadWhole(szSection, szKey, szDefaultValue, m_szFileName);
}
void CIniReader::WriteInteger(char* szSection, char* szKey, int iValue)
{
	char szValue[255];
	_snprintf(szValue, 255, "%d", iValue);
	IniWriteSafe(szSection, szKey, szValue, m_szFileName);
}
void CIniReader::WriteFloat(char* szSection, char* szKey, float fltValue)
{
	char szValue[255];
	_snprintf(szValue, 255, "%f", fltValue);
	IniWriteSafe(szSection, szKey, szValue, m_szFileName);
}
void CIniReader::WriteBoolean(char* szSection, char* szKey, bool bolValue)
{
	char szValue[255];
	_snprintf(szValue, 255, "%s", bolValue ? "True" : "False");
	IniWriteSafe(szSection, szKey, szValue, m_szFileName);
}
void CIniReader::WriteString(char* szSection, char* szKey, char* szValue)
{
	IniWriteSafe(szSection, szKey, szValue, m_szFileName);
}