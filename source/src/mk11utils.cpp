#include "mk11utils.h"
#include "prettyprint.h"
#include <map>

using namespace Memory::VP;
using namespace hook;

// Address Utilities

int64 GetGameEntryPoint()
{
	static __int64 addr = reinterpret_cast<__int64>(GetModuleHandle(nullptr));
	return addr;
}

int64 GetUser32EntryPoint()
{
	static __int64 addr = reinterpret_cast<__int64>(GetModuleHandle("user32.dll"));
	return addr;
}

int64 GetModuleEntryPoint(const char* name)
{
	static __int64 addr = reinterpret_cast<__int64>(GetModuleHandle(name));
	return addr;
}

int64 GetGameAddr(__int64 addr)
{
	int64 Entry = GetGameEntryPoint();
	return addr > Entry - 40000000 ? addr : Entry + addr;
	if (addr > Entry)
		return addr;
	if (addr > Entry - 40000000)
		return addr;
	return Entry + addr;
}

int64 GetUser32Addr(__int64 addr)
{
	return GetUser32EntryPoint() + addr;
}

int64 GetModuleAddr(__int64 addr, const char* name)
{
	return GetModuleEntryPoint(name) + addr;
}


// Functional Utilities

std::string toLower(std::string s)
{
	std::string new_string("");
	for (int i = 0; i < s.length(); i++)
	{
		new_string += std::tolower(s[i]);
	}
	return new_string;
}

std::string toUpper(std::string s)
{
	std::string new_string("");
	for (int i = 0; i < s.length(); i++)
	{
		new_string += std::toupper(s[i]);
	}
	return new_string;
}

std::string GetFileName(std::string filename)
{
	std::string basename;
	size_t pos = filename.find_last_of("/\\"); // Or
	if (pos != -1)
	{
		basename = filename.substr(pos + 1);
		return basename;
	}
	return filename;
}

std::string GetProcessName()
{
	CHAR fileName[MAX_PATH + 1];
	DWORD chars = GetModuleFileNameA(NULL, fileName, MAX_PATH + 1);
	if (chars)
	{
		return GetFileName(std::string(fileName));
	}
	return "";
}

std::string GetDirName()
{
	CHAR fileName[MAX_PATH + 1];
	DWORD chars = GetModuleFileNameA(NULL, fileName, MAX_PATH + 1);
	if (chars)
	{
		std::string basename;
		std::string filename = std::string(fileName);
		size_t pos = filename.find_last_of('\\');
		if (pos != -1)
		{
			basename = filename.substr(0, pos);
			return basename;
		}
		return filename;
	}
	return "";
}

HMODULE AwaitHModule(const char* name, uint64_t timeout)
{
	HMODULE toAwait = NULL;
	auto start = std::chrono::system_clock::now();
	while (!toAwait)
	{
		if (timeout > 0)
		{
			std::chrono::duration<double> now = std::chrono::system_clock::now() - start;
			if (now.count()*1000 > timeout)
			{
				std::cout << "No Handle " << name << std::endl;
				return NULL;
			}
		}
		toAwait = GetModuleHandle(name);	
	}
	if (SettingsMgr->iLogLevel)
		std::cout << "Obtained Handle for " << name << std::endl;
	return toAwait;
}

uint64_t stoui64h(std::string szString)
{
	return strtoull(szString.c_str(), nullptr, 16);
}


uint64_t* FindPattern(void* handle, std::string_view bytes)
{
	hook::pattern pCamPattern = hook::make_module_pattern(handle, bytes); // Make pattern external
	if (!pCamPattern.count_hint(1).empty())
	{
		return pCamPattern.get(0).get<uint64_t>(0);
	}
	return nullptr;
}

uint64_t HookPattern(std::string Pattern, const char* PatternName, void* HookProc, int64_t PatternOffset, PatchTypeEnum PatchType, uint64_t PrePat, uint64_t* Entry)
{
	uint64_t lpPattern;
	if (PrePat)
	{
		lpPattern = PrePat;
	}
	else
	{
		lpPattern = (uint64_t)FindPattern(GetModuleHandleA(NULL), Pattern);
		if (lpPattern != NULL)
		{
			SetColorGreen();
			if (SettingsMgr->iLogLevel)
				std::cout << PatternName << " Pattern found at: " << std::hex << lpPattern << std::endl;
			ResetColors();
		}
		else
		{
			SetColorRed();
			std::cout << "Couldn't find pattern. " << PatternName << " Disabled" << std::endl;
			ResetColors();
			return lpPattern;
		}
	}

	SetColorGreen();
	if (Entry != NULL)
	{
		uint64_t FuncEntry = GetDestinationFromOpCode(lpPattern + PatternOffset); // Already relative to game address so GetGameAddr is unnecessary
		*Entry = FuncEntry;
		if (SettingsMgr->iLogLevel)
			std::cout << PatternName << " Function Entry found at " << FuncEntry << std::endl;
	}
	
	uint64_t hook_address = lpPattern + PatternOffset;
	if (SettingsMgr->iLogLevel)
		std::cout << "Injecting at " << hook_address << std::endl;
	InjectHook(hook_address, HookProc, PatchType);
	
	ResetColors();
	return lpPattern;
}

void ConditionalJumpToJump(uint64_t HookAddress)
{
	uint64_t GameAddr = GetGameAddr(HookAddress);
	uint32_t JumpAddress = GetOffsetFromOpCode(GameAddr, 2, 4);
	Patch(GameAddr, (uint8_t)0xE9); // jmp
	Patch(GameAddr + 1, JumpAddress + 1); // jmp address+1 cuz we reduced function size by 1
	Patch(GameAddr + 5, (uint8_t)0x90); // nop
}

void ConditionalJumpToJump(uint64_t HookAddress, uint32_t Offset)
{
	HookAddress += Offset;
	uint64_t GameAddr = GetGameAddr(HookAddress);
	uint32_t JumpAddress = GetOffsetFromOpCode(GameAddr, 2, 4);
	Patch<uint8_t>(GameAddr, 0xE9); // jmp
	Patch<uint32_t>(GameAddr + 1, JumpAddress + 1); // jmp address+1 cuz we reduced function size by 1
	Patch<uint8_t>(GameAddr + 5, 0x90); // nop
}

/**
	Get Relative Offset From OpCode. Caller is the address to call / jmp / jne ...etc, Offset is the size of the opcode to the address, size is the size of the address.
	Example, call MK12.exe+21CDA30 from MK12.exe+3855356 becomes Function(143855356, 1, 4) since Call is E8 * * * * where * are the relative jump address.

	 * Get the relative offset from an opcode.
	 *
	 * @param Caller The address of the jump / call.
	 * @param Offset The offset to add to the caller to reach the address. It is 1 for jmp / call, 2 for je, jne, jg... etc since jmp is E9 Address.
	 * @param Size The size of the address, usually 4 for near jumps, 8 for long jumps.
	 * @return The calculated offset.
*/
int32_t GetOffsetFromOpCode(uint64_t Caller, uint64_t Offset, uint16_t Size)
{
	int32_t offset = 0;
	memcpy(&offset, (uint64_t*)(Caller + Offset), Size);
	return offset;
}


/**
 * Calculate the destination address from an opcode call.
 * Get relative offset and then adds it to address to get the entry of the called function, or destination jump address.
 *
 * @param Caller The address of the opcode call.
 * @param Offset The offset to add to the caller to reach the address in the opcode. Example, 1 for jmp, 2 for je.
 * @param FuncLen The length of the function to be added to the RIP register.
 * @param Size The size of the address.
 * @return The calculated destination address.
 * 
 * @todo offset should allow int64_t in cases where Size is > 4 so negatives are handled correctly.
 */
uint64_t GetDestinationFromOpCode(uint64_t Caller, uint64_t Offset, uint64_t FuncLen, uint16_t Size)
{
	int32_t offset = GetOffsetFromOpCode(Caller, Offset, Size);
	return uint64_t(Caller + offset) + FuncLen;
}

void SetCheatPattern(std::string pattern, std::string name, uint64_t** lpPattern)
{
	if (!pattern.empty())
	{
		SetColorCyan();
		if (SettingsMgr->iLogLevel)
			std::cout << "Searching for " << name << ": " << pattern << std::endl;
		*lpPattern = FindPattern(GetModuleHandleA(NULL), pattern);
		if (*lpPattern != nullptr)
		{
			SetColorGreen();
			if (SettingsMgr->iLogLevel)
				std::cout << "Found at: " << std::hex << *lpPattern << std::dec << std::endl;
		}
		else
		{
			SetColorRed();
			std::cout << name << " Not found >> Disabled." << std::endl;
		}
		ResetColors();
	}
}

LibMap ParsePEHeader()
{
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)GetModuleHandleA(NULL);
	PIMAGE_NT_HEADERS pNTHeader = (PIMAGE_NT_HEADERS)RVAtoLP((PBYTE)pDosHeader, pDosHeader->e_lfanew);
	if (pNTHeader->Signature != IMAGE_NT_SIGNATURE)
		RaiseException("NT Signature Failed!", -1); // Not an EXE

	LibMap IAT{};

	PIMAGE_IMPORT_DESCRIPTOR pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)RVAtoLP(pDosHeader, pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

	for (int i = 0; pImportDesc[i].Characteristics != 0; i++)
	{
		std::string szLibrary = (char*)RVAtoLP(pDosHeader, pImportDesc[i].Name);
		if (!pImportDesc[i].FirstThunk || !pImportDesc[i].OriginalFirstThunk)
			continue;

		PIMAGE_THUNK_DATA pThunk = (PIMAGE_THUNK_DATA)RVAtoLP(pDosHeader, pImportDesc[i].FirstThunk);
		PIMAGE_THUNK_DATA pOrigThunk = (PIMAGE_THUNK_DATA)RVAtoLP(pDosHeader, pImportDesc[i].OriginalFirstThunk);

		FuncMap FunctionsMap{};

		for (; pOrigThunk->u1.Function != NULL; pOrigThunk++, pThunk++)
		{
			if (pOrigThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG)
				continue;

			PIMAGE_IMPORT_BY_NAME import = (PIMAGE_IMPORT_BY_NAME)RVAtoLP(pDosHeader, pOrigThunk->u1.AddressOfData);

			std::string FuncName = (char*)import->Name;
			FunctionsMap[FuncName] = pThunk->u1.Function;
		}
		std::string szLibraryStoreName = toLower(szLibrary);
		IAT[szLibraryStoreName] = FunctionsMap;
	}

	return IAT;

}

int StringToVK(std::string sKey)
{
	for (auto& c : sKey) c = toupper(c); // To Upper sKey

	if (sKey == "SHIFT")
	{
		return VK_SHIFT;
	}

	if (sKey == "TAB")
	{
		return VK_TAB;
	}

	if (sKey == "SPACE")
	{
		return VK_SPACE;
	}

	if (sKey == "ALT")
	{
		return VK_MENU;
	}

	if (sKey.substr(0, 6) == "NUMPAD") // Numpad
	{
		char cLast = sKey[6];
		switch (cLast)
		{
		case '+':
			return VK_ADD;
		case '-':
			return VK_SUBTRACT;
		case 'E': //NUMPAD Enter
			return VK_RETURN;
		case '*':
			return VK_MULTIPLY;
		case '/':
			return VK_DIVIDE;
		default:
			return cLast + 0x30; // NUMPAD Number
		}

	}
	if (sKey.length() == 1)
	{
		if (sKey[0] >= '0' && sKey[0] <= 'Z' && sKey[0] != 0x40) // Letter or Number. 0x40 is not available which is @
			return sKey[0]; // Letters and Numbers' VK are their ASCII Repr
		if (sKey[0] == '/' || sKey[0] == '?')
			return VK_OEM_2;
		if (sKey[0] == '-')
			return VK_OEM_MINUS;
		if (sKey[0] == '+')
			return VK_OEM_PLUS;
		if (sKey[0] == '`' || sKey[0] == '~')
			return VK_OEM_3;
	}

	if (sKey[0] == 'F' && sKey.length() > 1 && sKey.length() <= 3) // F1-F12 buttons
	{
		std::string digits = sKey.substr(1, sKey.length() - 1).c_str();
		return std::stoi(digits.c_str()) + 0x70 - 1;
	}

	// Arrows
	if (sKey == "DOWN")
		return VK_DOWN;
	if (sKey == "UP")
		return VK_UP;
	if (sKey == "LEFT")
		return VK_LEFT;
	if (sKey == "RIGHT")
		return VK_RIGHT;

	RaiseException("Button Binding Unsupported!");
}

void RaiseException(const char* Message, int64_t ErrorCode)
{
	std::string ErrorMessage = "Error: " + std::string(Message);
	MessageBoxA(0, ErrorMessage.c_str(), "Error", MB_ICONERROR);
	throw(ErrorCode);
}

bool IsHex(char c)
{
	return IsBase(c, 16);
}

bool IsBase(char c, int base)
{
	std::string alpha = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+/";
	return alpha.substr(0, base).find(c) != -1;
}