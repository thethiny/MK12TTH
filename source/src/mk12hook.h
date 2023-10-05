#pragma once
#include "../includes.h"

namespace MK12Functions {
	HANDLE __stdcall	CreateFileProxy(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);

	// FString
	__int64	__fastcall	ReadFStringProxy(__int64, __int64);
	typedef	__int64		(__fastcall ReadFString)(__int64, __int64);
	extern ReadFString* MK1ReadFString;
}

namespace HookMetadata {
	extern HHOOK KeyboardProcHook;
	extern HMODULE CurrentDllModule;
	extern HANDLE Console;
}

namespace MK12Hooks {
	bool DisableSignatureCheck();
	bool DisableSignatureWarn();
	bool bDisableChunkSigCheck();
	bool bDisableTOCSigCheck();
}