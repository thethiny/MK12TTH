#pragma once
#include "MemoryMgr.h"
#include "Trampoline.h"
#include "Patterns.h"
#include <cstdint>
#include <string>
#include <map>

/** Describes the layout of a parsed x86-64 instruction for RIP-relative address resolution.
 *  Only populated for instructions with RIP-relative addressing (call, jmp, lea, mov, cmp).
 *  Check isValid before using other fields. */
struct InstructionInfo {
	uint8_t dispOffset;     /**< Bytes from instruction start to the displacement field */
	uint8_t dispSize;       /**< Size of the displacement field (0, 1, or 4 bytes) */
	uint8_t totalSize;      /**< Total instruction length in bytes */
	bool    isIndirect;     /**< True for FF 15/25: displacement points to a pointer, needs extra dereference */
	bool    isValid;        /**< False if the instruction could not be parsed */
};

#define		RVAtoLP( base, offset )		((PBYTE)base + offset)
typedef		std::map<std::string, ULONGLONG>	FuncMap;
typedef		std::map<std::string, FuncMap>		LibMap;

/**
 * Game-agnostic process patching utilities.
 * Stateless namespace: no instance state, works with any module.
 * For game-specific features (caching, logging), use GamePatchManager instead.
 */
namespace ProcessPatch {

	// ── Get: retrieve module addresses ──

	/** Returns the base address of the main module (exe). Cached on first call. */
	int64_t		GetMainModuleBase();

	/** Returns the base address of the specified module by name. NOT cached, resolves every call.
	 *  @param name  DLL or module name (e.g. "user32.dll") */
	int64_t		GetModuleBase(const char* name);

	// ── Search: locate byte patterns in memory ──

	/** Searches a module for a byte pattern.
	 *  @param handle  Module handle to search (e.g. GetModuleHandle(nullptr))
	 *  @param bytes   Pattern string with ? wildcards (e.g. "48 8B 0D ? ? ? ? E8")
	 *  @return Pointer to the first match, or nullptr if not found */
	uint64_t*	SearchPattern(void* handle, std::string_view bytes);

	/** Searches the main module for a byte pattern.
	 *  @param pattern  Pattern string with ? wildcards
	 *  @return Pointer to the first match, or nullptr if not found */
	uint64_t*	SearchPattern(std::string pattern);

	/** Searches the main module for a byte pattern.
	 *  @param pattern  Pattern string with ? wildcards
	 *  @return Pointer to the first match, or nullptr if not found */
	uint64_t*	SearchPattern(const char* pattern);

	// ── Parse: analyze instruction and PE structure ──

	/** Parses the instruction at addr to determine its layout for RIP-relative resolution.
	 *  Handles: E8 (call), E9 (jmp), EB (short jmp), 70-7F (short conditional),
	 *  0F 80-8F (near conditional), FF 15/25 (indirect call/jmp),
	 *  REX+lea/mov/cmp with RIP-relative addressing, 80 3D (cmp byte [rip+off]).
	 *  @param addr  Address of the first byte of the instruction
	 *  @return InstructionInfo with isValid=false if the encoding is not recognized */
	InstructionInfo ParseInstruction(uint64_t addr);

	/** Parses the PE Import Address Table of the main module.
	 *  @return A map of library names (lowercase) to their function name -> address mappings */
	LibMap		ParseImportTable();

	/** Computes an FNV-1a hash of the .text section of the main module.
	 *  Used for cache invalidation: the hash changes when the game binary updates.
	 *  Prints the hash and timing to stdout.
	 *  @return The 64-bit hash, or 0 if .text section not found */
	uint64_t	HashTextSection();

	// ── Resolve: compute where an instruction points ──

	/** Resolves the destination address from a call/jmp/lea/mov/cmp opcode using auto-detection.
	 *  Uses ParseInstruction internally. Falls back to 0 if instruction not recognized.
	 *  For indirect instructions (FF 15/25), performs the extra pointer dereference automatically.
	 *  @param addr  Address of the first byte of the instruction
	 *  @return The resolved target address, or 0 on failure */
	uint64_t	ResolveDestination(uint64_t addr);

	/** Resolves the destination address from an opcode with manually provided layout.
	 *  Uses ReadDisplacement internally. Use when ParseInstruction doesn't recognize the encoding.
	 *  Formula: target = addr + displacement + instrSize
	 *  @param addr        Address of the instruction
	 *  @param dispOffset  Bytes to skip to reach the displacement field (1 for call/jmp, 2 for conditional, 3 for REX+opcode+ModRM)
	 *  @param instrSize   Total instruction length in bytes (5 for E8/E9, 6 for 0F 8x, 7 for REX+lea/mov)
	 *  @param dispSize    Size of the displacement field in bytes (usually 4)
	 *  @return The absolute target address */
	uint64_t	ResolveDestination(uint64_t addr, uint64_t dispOffset, uint64_t instrSize, uint16_t dispSize);

	// ── Patch: modify bytes at an address ──

	/** Overwrites an instruction with ret (0xC3) followed by nops to fill the remaining bytes.
	 *  Use this to make a function return immediately.
	 *  @param addr  Address to patch
	 *  @param size  Number of bytes to overwrite. Common sizes: 5 (call near), 6 (call far/jne near), 2 (short jmp) */
	inline void PatchReturnAt(uint64_t addr, uint8_t size)
	{
		DWORD dwProtect;
		VirtualProtect((void*)addr, size, PAGE_EXECUTE_READWRITE, &dwProtect);
		Memory::Patch(addr, (uint8_t)0xC3);
		if (size > 1)
			Memory::Nop(addr + 1, size - 1);
		VirtualProtect((void*)addr, size, dwProtect, &dwProtect);
	}

	/** Converts a conditional jump into an unconditional jump.
	 *  Handles both short (7x -> EB, 2 bytes) and near (0F 8x -> E9, 6 bytes) conditionals.
	 *  Auto-detects which encoding is present at addr.
	 *  @param addr  Address of the conditional jump instruction (the first opcode byte) */
	inline void PatchConditionalToUnconditional(uint64_t addr)
	{
		uint8_t b0 = *(uint8_t*)addr;

		if (b0 >= 0x70 && b0 <= 0x7F)
		{
			DWORD dwProtect;
			VirtualProtect((void*)addr, 2, PAGE_EXECUTE_READWRITE, &dwProtect);
			Memory::Patch(addr, (uint8_t)0xEB);
			VirtualProtect((void*)addr, 2, dwProtect, &dwProtect);
		}
		else if (b0 == 0x0F)
		{
			int32_t offset = *(int32_t*)(addr + 2);
			DWORD dwProtect;
			VirtualProtect((void*)addr, 6, PAGE_EXECUTE_READWRITE, &dwProtect);
			Memory::Patch(addr, (uint8_t)0xE9);
			Memory::Patch(addr + 1, (uint32_t)(offset + 1));
			Memory::Patch(addr + 5, (uint8_t)0x90);
			VirtualProtect((void*)addr, 6, dwProtect, &dwProtect);
		}
	}

	// ── Redirect: change where a call/jmp points ──

	/** Redirects an existing call/jmp instruction to a new target by patching its relative displacement.
	 *  @param addr        Address of the call/jmp instruction
	 *  @param newTarget   Absolute address of the new target function
	 *  @param opcodeSize  Bytes before the displacement field. 0 = auto-detect via ParseInstruction */
	inline void RedirectCallTo(uint64_t addr, uint64_t newTarget, uint8_t opcodeSize = 0)
	{
		if (opcodeSize == 0)
		{
			InstructionInfo info = ParseInstruction(addr);
			if (info.isValid)
				opcodeSize = info.dispOffset;
			else
				opcodeSize = 1;
		}

		uint8_t addrFieldSize = 4;
		uint8_t instrSize = opcodeSize + addrFieldSize;
		uint64_t nextInstr = addr + instrSize;
		int32_t newOffset = (int32_t)(newTarget - nextInstr);

		DWORD dwProtect;
		VirtualProtect((void*)(addr + opcodeSize), addrFieldSize, PAGE_EXECUTE_READWRITE, &dwProtect);
		Memory::Patch(addr + opcodeSize, newOffset);
		VirtualProtect((void*)(addr + opcodeSize), addrFieldSize, dwProtect, &dwProtect);
	}

	// ── Proxy: intercept a call and forward to original ──

	/** Proxies a call/jmp instruction via trampoline, capturing the original target.
	 *  Resolves the original destination before patching, so the proxy can forward to it.
	 *  @param tramp      Trampoline for generating the far jump stub
	 *  @param addr       Address of the call/jmp instruction to proxy
	 *  @param proxyFunc  Your proxy function that will be called instead
	 *  @param patchType  PATCH_CALL or PATCH_JUMP (from MemoryMgr.h)
	 *  @return The original function address as a pointer, or nullptr if resolution failed */
	inline uint64_t* ProxyCallAt(Trampoline* tramp, uint64_t addr, void* proxyFunc, PatchTypeEnum patchType = PATCH_CALL)
	{
		uint64_t originalFunc = ResolveDestination(addr);
		if (!originalFunc)
			return nullptr;

		Memory::VP::InjectHook(addr, tramp->Jump(proxyFunc), patchType);
		return (uint64_t*)originalFunc;
	}

	/** Typed version of ProxyCallAt. Stores the original function pointer into a typed variable.
	 *  @param tramp         Trampoline for generating the far jump stub
	 *  @param addr          Address of the call/jmp instruction to proxy
	 *  @param proxyFunc     Your proxy function that will be called instead
	 *  @param originalFunc  Pointer to store the original function address (cast to T*)
	 *  @param patchType     PATCH_CALL or PATCH_JUMP */
	template <typename T>
	inline void ProxyCallAt(Trampoline* tramp, uint64_t addr, void* proxyFunc, T** originalFunc, PatchTypeEnum patchType = PATCH_CALL)
	{
		*originalFunc = (T*)ProxyCallAt(tramp, addr, proxyFunc, patchType);
	}

	// ── Replace: swap a function entirely ──

	/** Replaces a function at its entry point with a jump to a replacement function.
	 *  The original function becomes unreachable. Use ProxyCallAt if you need to call the original.
	 *  @param tramp        Trampoline for generating the far jump stub
	 *  @param addr         Address of the function entry point to replace
	 *  @param replacement  Your replacement function */
	inline void ReplaceFunctionWith(Trampoline* tramp, uint64_t addr, void* replacement)
	{
		Memory::VP::InjectHook(addr, tramp->Jump(replacement), PATCH_JUMP);
	}

	// ── Utility ──

	/** No-op void function stub. Use when you need to patch a function to do nothing. */
	static void DummyVoidFunc() {}
	/** No-op pointer function stub. Returns nullptr. Use when you need to patch a function to return null. */
	static void* DummyPtrFunc(...) { return nullptr; }

}

/** x86-64 register manipulation utilities.
 *  Dynamically generates mov-to-register stubs (mov REG, rcx; ret) for all 12 general-purpose registers.
 *  Call EnableRegisterHacks() once before use. Used by hook proxies that need to set
 *  specific register values after returning (e.g. FName proxies setting RBX). */
namespace RegisterHacks {
	/** Allocates executable memory and generates mov stubs for all registers. Call once at startup. */
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
