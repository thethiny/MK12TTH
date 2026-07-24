#pragma once
#include "MemoryMgr.h"
#include "Trampoline.h"
#include <cstdint>

namespace ProcessPatch {

	// Patches address with ret (0xC3) + nops to fill 'size' bytes
	// Common sizes: 5 (call near), 6 (call far/jne near), 2 (short jmp/je)
	inline void PatchReturn(uint64_t addr, uint8_t size)
	{
		DWORD dwProtect;
		VirtualProtect((void*)addr, size, PAGE_EXECUTE_READWRITE, &dwProtect);
		Memory::Patch(addr, (uint8_t)0xC3);
		if (size > 1)
			Memory::Nop(addr + 1, size - 1);
		VirtualProtect((void*)addr, size, dwProtect, &dwProtect);
	}

	// Patches a conditional jump (je/jne/jg etc) into an unconditional jmp
	// Near conditional: 0F 8x xx xx xx xx (6 bytes) -> E9 xx xx xx xx 90 (6 bytes)
	inline void ConditionalToUnconditional(uint64_t addr)
	{
		int32_t offset = *(int32_t*)(addr + 2);
		DWORD dwProtect;
		VirtualProtect((void*)addr, 6, PAGE_EXECUTE_READWRITE, &dwProtect);
		Memory::Patch(addr, (uint8_t)0xE9);
		Memory::Patch(addr + 1, (uint32_t)(offset + 1));
		Memory::Patch(addr + 5, (uint8_t)0x90);
		VirtualProtect((void*)addr, 6, dwProtect, &dwProtect);
	}

	// Redirects an existing call/jmp instruction to a new target by patching its relative offset
	// addr must point to the start of the call/jmp instruction
	// opcodeSize: 1 for E8/E9, 2 for 0F 8x/FF 15
	inline void RedirectCall(uint64_t addr, uint64_t newTarget, uint8_t opcodeSize = 1)
	{
		uint8_t addrFieldSize = 4;
		uint8_t instrSize = opcodeSize + addrFieldSize;
		uint64_t nextInstr = addr + instrSize;
		int32_t newOffset = (int32_t)(newTarget - nextInstr);

		DWORD dwProtect;
		VirtualProtect((void*)(addr + opcodeSize), addrFieldSize, PAGE_EXECUTE_READWRITE, &dwProtect);
		Memory::Patch(addr + opcodeSize, newOffset);
		VirtualProtect((void*)(addr + opcodeSize), addrFieldSize, dwProtect, &dwProtect);
	}

	// Returns the opcode size at an address
	// 1 for E8 (call), E9 (jmp), EB (short jmp), 70-7F (short conditional)
	// 2 for 0F 80-8F (near conditional), FF 15 (indirect call), FF 25 (indirect jmp)
	// 0 for unknown
	inline uint8_t GetOpCodeSize(uint64_t addr)
	{
		uint8_t b0 = *(uint8_t*)addr;
		if (b0 == 0xE8 || b0 == 0xE9 || b0 == 0xEB)
			return 1;
		if (b0 >= 0x70 && b0 <= 0x7F)
			return 1;
		if (b0 == 0x0F)
		{
			uint8_t b1 = *(uint8_t*)(addr + 1);
			if (b1 >= 0x80 && b1 <= 0x8F)
				return 2;
		}
		if (b0 == 0xFF)
		{
			uint8_t b1 = *(uint8_t*)(addr + 1);
			if (b1 == 0x15 || b1 == 0x25)
				return 2;
		}
		return 0;
	}

	// Returns the address size for a given opcode
	// 1 for short jumps (EB, 70-7F), 4 for near (E8, E9, 0F 8x, FF 15/25)
	// 0 for unknown
	inline uint8_t GetAddrSize(uint64_t addr)
	{
		uint8_t b0 = *(uint8_t*)addr;
		if (b0 == 0xEB || (b0 >= 0x70 && b0 <= 0x7F))
			return 1;
		if (b0 == 0xE8 || b0 == 0xE9 || b0 == 0x0F || b0 == 0xFF)
			return 4;
		return 0;
	}

	// Resolves the destination address from a call/jmp opcode
	// addr must point to the start of the instruction
	inline uint64_t ResolveDestination(uint64_t addr)
	{
		uint8_t opcSize = GetOpCodeSize(addr);
		uint8_t adrSize = GetAddrSize(addr);
		if (!opcSize || !adrSize)
			return 0;

		int32_t offset = 0;
		memcpy(&offset, (void*)(addr + opcSize), adrSize);

		uint64_t nextInstr = addr + opcSize + adrSize;
		uint64_t target = nextInstr + offset;

		// FF 15/FF 25: indirect - the target is a pointer to the real address
		uint8_t b0 = *(uint8_t*)addr;
		if (b0 == 0xFF)
			target = *(uint64_t*)target;

		return target;
	}

	// Extracts original function pointer and redirects call/jmp to a proxy via trampoline
	// Returns the original function address
	// PatchType: PATCH_CALL or PATCH_JUMP (from MemoryMgr.h)
	inline uint64_t* ProxyCallSite(Trampoline* tramp, uint64_t addr, void* proxyFunc, PatchTypeEnum patchType = PATCH_CALL)
	{
		uint64_t originalFunc = ResolveDestination(addr);
		if (!originalFunc)
			return nullptr;

		Memory::VP::InjectHook(addr, tramp->Jump(proxyFunc), patchType);
		return (uint64_t*)originalFunc;
	}

	// Typed version: extracts original function pointer into a typed variable
	template <typename T>
	inline void ProxyCallSite(Trampoline* tramp, uint64_t addr, void* proxyFunc, T** originalFunc, PatchTypeEnum patchType = PATCH_CALL)
	{
		*originalFunc = (T*)ProxyCallSite(tramp, addr, proxyFunc, patchType);
	}

	// Replaces a function at its entry point with a jump to replacement
	// addr is the start of the function to replace
	inline void ReplaceFunction(Trampoline* tramp, uint64_t addr, void* replacement)
	{
		Memory::VP::InjectHook(addr, tramp->Jump(replacement), PATCH_JUMP);
	}

}
