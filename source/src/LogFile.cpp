#include "LogFile.h"
#include <Windows.h>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <string>

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

namespace LogFile {
	// The C runtime's print functions every printf, fprintf and wprintf of the mod ends up in
	constexpr const char*	RUNTIME_STDIO	= "api-ms-win-crt-stdio";
	constexpr const char*	PRINT			= "__stdio_common_vfprintf";
	constexpr const char*	PRINT_WIDE		= "__stdio_common_vfwprintf";
	constexpr const char*	FILE_SUFFIX		= "_log.txt";

	typedef int (__cdecl PrintType)(unsigned __int64 Options, FILE* Stream, const char* Format, _locale_t Locale, va_list Args);
	typedef int (__cdecl PrintWideType)(unsigned __int64 Options, FILE* Stream, const wchar_t* Format, _locale_t Locale, va_list Args);

	static HANDLE			File			= INVALID_HANDLE_VALUE;
	static PrintType*		RealPrint		= nullptr;
	static PrintWideType*	RealPrintWide	= nullptr;

	void Write(const std::string& Text)
	{ // Without the console's color codes (ESC [ ... letter)
		std::string Plain;
		Plain.reserve(Text.size());
		for (size_t i = 0; i < Text.size(); i++)
		{
			if (Text[i] == '\x1b' && i + 1 < Text.size() && Text[i + 1] == '[')
			{
				i += 2;
				while (i < Text.size() && !(Text[i] >= '@' && Text[i] <= '~'))
					i++;
				continue;
			}
			Plain += Text[i];
		}
		if (Plain.empty())
			return;
		DWORD Written = 0;
		WriteFile(File, Plain.data(), (DWORD)Plain.size(), &Written, nullptr);
	}

	bool IsConsoleStream(FILE* Stream)
	{
		return Stream == stdout || Stream == stderr;
	}

	int __cdecl ClonePrint(unsigned __int64 Options, FILE* Stream, const char* Format, _locale_t Locale, va_list Args)
	{
		if (IsConsoleStream(Stream))
		{
			va_list Copy;
			va_copy(Copy, Args);
			int Length = _vscprintf(Format, Copy);
			va_end(Copy);
			if (Length > 0)
			{
				std::string Text(Length, '\0');
				va_copy(Copy, Args);
				vsnprintf(&Text[0], Text.size() + 1, Format, Copy);
				va_end(Copy);
				Write(Text);
			}
		}
		return RealPrint(Options, Stream, Format, Locale, Args);
	}

	int __cdecl ClonePrintWide(unsigned __int64 Options, FILE* Stream, const wchar_t* Format, _locale_t Locale, va_list Args)
	{
		if (IsConsoleStream(Stream))
		{
			va_list Copy;
			va_copy(Copy, Args);
			int Length = _vscwprintf(Format, Copy);
			va_end(Copy);
			if (Length > 0)
			{
				std::wstring Wide(Length, L'\0');
				va_copy(Copy, Args);
				_vsnwprintf_s(&Wide[0], Wide.size() + 1, _TRUNCATE, Format, Copy);
				va_end(Copy);
				int Size = WideCharToMultiByte(CP_UTF8, 0, Wide.c_str(), (int)Wide.size(), nullptr, 0, nullptr, nullptr);
				std::string Text(Size, '\0');
				WideCharToMultiByte(CP_UTF8, 0, Wide.c_str(), (int)Wide.size(), &Text[0], Size, nullptr, nullptr);
				Write(Text);
			}
		}
		return RealPrintWide(Options, Stream, Format, Locale, Args);
	}

	// Points this module's import of a C runtime print function at ours; the game and other mods keep theirs
	bool Redirect(const char* Function, void* Clone, void** Real)
	{
		BYTE* Base = (BYTE*)&__ImageBase;
		IMAGE_NT_HEADERS* Nt = (IMAGE_NT_HEADERS*)(Base + __ImageBase.e_lfanew);
		const IMAGE_DATA_DIRECTORY& Imports = Nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
		if (!Imports.VirtualAddress)
			return false;
		for (IMAGE_IMPORT_DESCRIPTOR* Library = (IMAGE_IMPORT_DESCRIPTOR*)(Base + Imports.VirtualAddress); Library->Name; Library++)
		{
			if (_strnicmp((const char*)(Base + Library->Name), RUNTIME_STDIO, strlen(RUNTIME_STDIO)) != 0)
				continue;
			IMAGE_THUNK_DATA* Names = (IMAGE_THUNK_DATA*)(Base + Library->OriginalFirstThunk);
			IMAGE_THUNK_DATA* Addresses = (IMAGE_THUNK_DATA*)(Base + Library->FirstThunk);
			for (; Names->u1.AddressOfData; Names++, Addresses++)
			{
				if (IMAGE_SNAP_BY_ORDINAL(Names->u1.Ordinal))
					continue;
				IMAGE_IMPORT_BY_NAME* Import = (IMAGE_IMPORT_BY_NAME*)(Base + Names->u1.AddressOfData);
				if (strcmp((const char*)Import->Name, Function) != 0)
					continue;
				DWORD Protection = 0;
				if (!VirtualProtect(&Addresses->u1.Function, sizeof(void*), PAGE_READWRITE, &Protection))
					return false;
				*Real = (void*)Addresses->u1.Function;
				Addresses->u1.Function = (ULONG_PTR)Clone;
				VirtualProtect(&Addresses->u1.Function, sizeof(void*), Protection, &Protection);
				return true;
			}
		}
		return false;
	}

	std::string FilePath()
	{ // In the game's folder, the mod's name in lowercase
		char Exe[MAX_PATH] = {};
		GetModuleFileNameA(nullptr, Exe, MAX_PATH);
		std::string Folder = Exe;
		Folder = Folder.substr(0, Folder.find_last_of("\\/") + 1);

		char Module[MAX_PATH] = {};
		GetModuleFileNameA((HINSTANCE)&__ImageBase, Module, MAX_PATH);
		std::string Name = Module;
		Name = Name.substr(Name.find_last_of("\\/") + 1);
		Name = Name.substr(0, Name.find_last_of('.'));
		for (size_t i = 0; i < Name.size(); i++)
			Name[i] = (char)tolower((unsigned char)Name[i]);
		return Folder + Name + FILE_SUFFIX;
	}

	bool Start()
	{
		if (File != INVALID_HANDLE_VALUE)
			return true;
		// Written on every print: everything is in the file even if the game crashes
		File = CreateFileA(FilePath().c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (File == INVALID_HANDLE_VALUE)
			return false;
		char Header[64];
		time_t Now = time(nullptr);
		snprintf(Header, sizeof(Header), "Logging Started - %s", ctime(&Now));
		Write(Header);
		Redirect(PRINT, (void*)ClonePrint, (void**)&RealPrint);
		Redirect(PRINT_WIDE, (void*)ClonePrintWide, (void**)&RealPrintWide);
		return RealPrint != nullptr;
	}
}
