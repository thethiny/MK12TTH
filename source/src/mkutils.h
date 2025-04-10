#pragma once
#include <string>
#include "../../pch.h"
#include "../utils/MemoryMgr.h"
#include "../utils/Trampoline.h"
#include "../utils/Patterns.h"
#include "../CPPython/cppython.h"
#include "eSettingsManager.h"
#include <chrono>
#include <map>

#define			RVAtoLP( base, offset )		((PBYTE)base + offset)
#define			FuncMap						std::map<std::string, ULONGLONG>
#define			LibMap						std::map<std::string, FuncMap>
#define			FNAME_STR(FName)			MK12::FNameFunc::ToStr(*FName)
typedef			__int64						int64;

int64			GetGameEntryPoint();
int64			GetUser32EntryPoint();
int64			GetModuleEntryPoint(const char* name);
int64			GetGameAddr(__int64 addr);
int64			GetUser32Addr(__int64 addr);
int64			GetModuleAddr(__int64 addr, const char* name);
std::string		GetProcessName();
std::string		GetDirName();
std::string		toLower(std::string s);
std::string		toUpper(std::string s);
std::string		GetFileName(std::string filename);
HMODULE			AwaitHModule(const char* name, uint64_t timeout = 0);
uint64_t		stoui64h(std::string szString);
uint64_t*		FindPattern(void* handle, std::string_view bytes);
uint64_t		HookPattern(std::string Pattern, const char* PatternName, void* HookProc, int64_t PatternOffset = 0, PatchTypeEnum PatchType = PatchTypeEnum::PATCH_CALL, uint64_t PrePat = NULL, uint64_t* Entry = nullptr);
uint64_t		GetDestinationFromOpCode(uint64_t Caller, uint64_t Offset = 1, uint64_t FuncLen = 5, uint16_t size = 4);
int32_t			GetOffsetFromOpCode(uint64_t Caller, uint64_t Offset, uint16_t size);
void			ConditionalJumpToJump(uint64_t HookAddress, uint32_t Offset);
void			ConditionalJumpToJump(uint64_t HookAddress);
void			SetCheatPattern(std::string pattern, std::string name, uint64_t** lpPattern);
LibMap			ParsePEHeader();
int				StringToVK(std::string);
void			RaiseException(const char*, int64_t = 1);
bool			IsHex(char);
bool			IsBase(char c, int = 16);

uint64_t* MakeProxyFromOpCode(Trampoline* GameTramp, uint64_t CallAddr, uint8_t JumpAddrSize, void* ProxyFunctionAddr, PatchTypeEnum PatchType = PATCH_CALL);
template <typename T> void MakeProxyFromOpCode(Trampoline* GameTramp, uint64_t CallAddr, uint8_t JumpAddrSize, void* ProxyFunctionAddr, T** ProxyFuncPtr, PatchTypeEnum PatchType = PATCH_CALL);


struct LibFuncStruct {
	std::string FullName;
	std::string LibName;
	std::string ProcName;
	bool bIsValid = false;
};

LibFuncStruct	ParseLibFunc(CPPython::string);
void			ParseLibFunc(LibFuncStruct&);
uint64_t*		GetLibProcFromNT(const LibFuncStruct&);
void			PrintErrorProcNT(const LibFuncStruct& LFS, uint8_t bMode);
extern LibMap	IATable;


// Template Definitions

/**
 * Patch a function from its `call` opcode to point to a proxy to the called function
 *
 * @param GameTramp A Trampoline that lives in the game's code space
 * @param CallAddr The address to the `call` opcode
 * @param JumpAddrSize The Size of the Address in the opcode
 * @param ProxyFunctionAddr The address of the Proxy Function
 * @param ProxyFuncPtr The address of a pointer that will reference the game's function
 * @param PatchType CALL or JUMP, defaults to CALL
 *
 */
template <typename T>
void MakeProxyFromOpCode(Trampoline* GameTramp, uint64_t CallAddr, uint8_t JumpAddrSize, void* ProxyFunctionAddr, T** ProxyFuncPtr, PatchTypeEnum PatchType) // Jump size is either 4 or 8
{
	*ProxyFuncPtr = (T*)MakeProxyFromOpCode(GameTramp, CallAddr, JumpAddrSize, ProxyFunctionAddr, PatchType);
}

namespace RegisterHacks {
	void					EnableRegisterHacks();
	typedef void			(__fastcall MoveToRegister)(uint64_t);
	typedef uint64_t		(__fastcall MoveFromRegister)();

	extern bool				bIsEnabled;

	extern MoveToRegister*	MoveToRAX;
	extern MoveToRegister*	MoveToRBX;
	extern MoveToRegister*	MoveToRCX;
	extern MoveToRegister*	MoveToRDX;
	extern MoveToRegister*	MoveToR8;
	extern MoveToRegister*	MoveToR9;
	extern MoveToRegister*	MoveToR10;
	extern MoveToRegister*	MoveToR11;
	extern MoveToRegister*	MoveToR12;
	extern MoveToRegister*	MoveToR13;
	extern MoveToRegister*	MoveToR14;
	extern MoveToRegister*	MoveToR15;

}

// How to patch jump and patch call into new codespace
/*uint64_t patchAddr = (uint64_t)lpPattern - 3;
uint8_t callSize = 16;
uint8_t jmpSize = 2 + 4 + 8;
uint8_t instrSize = jmpSize + 0;
uint8_t* NewMEM = new uint8_t[instrSize];
uint8_t* CallSpace = new uint8_t[instrSize + callSize + jmpSize];
DWORD oldProtect;
VirtualProtect(CallSpace, instrSize + callSize + jmpSize, PAGE_EXECUTE_READWRITE, &oldProtect);*/


//memcpy(NewMEM, (uint8_t*)patchAddr, instrSize); // Copy old code

//Patch<uint16_t>(patchAddr, 0x25FF); // Patch Jump Long
//Patch<uint32_t>(patchAddr + 2, 0);
//Patch<uint64_t>(patchAddr + 2 + 4, (uint64_t)CallSpace);

//for (int i = jmpSize; i < instrSize; i++)
//{
//	Patch<uint8_t>(patchAddr + i, 0x90); // Nop remaining code
//}

//InjectHook(CallSpace, GameTramp->Jump(DoNothing), PATCH_CALL); // Inject the call
//Patch<uint16_t>(CallSpace, 0x15FF); // Patch Call Long
//Patch<uint32_t>(CallSpace + 2, 2);
//Patch<uint16_t>(CallSpace + 2 + 4, 0x8EB);
//Patch<uint64_t>(CallSpace + 2 + 4 + 2, (uint64_t)override_rcx);

//memcpy(CallSpace + callSize, NewMEM, instrSize); // Copy old code back
//Patch<uint16_t>(CallSpace + instrSize + callSize, 0x25FF); // Patch Jump Long
//Patch<uint32_t>(CallSpace + instrSize + callSize + 2, 0);
//Patch<uint64_t>(CallSpace + instrSize + callSize + 2 + 4, (uint64_t)lpPattern + 0xB);

/*uint8_t Asm[] = { 0x48, 0x89, 0xCb, 0xC3 };
memcpy(CallSpace, Asm, sizeof(Asm));
MK12Hook::Proxies::MoveIntoRBXFunc = (MK12Hook::Proxies::MoveIntoRBX*)CallSpace;*/