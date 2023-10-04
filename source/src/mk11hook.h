#pragma once
#include "mk11.h"

namespace MK1Functions {
	HANDLE __stdcall	CreateFileProxy(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);

	// FString
	__int64	__fastcall	ReadFStringProxy(__int64, __int64);
	typedef	__int64		(__fastcall ReadFString)(__int64, __int64);
	extern ReadFString* MK1ReadFString;
}