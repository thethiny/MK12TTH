#pragma once
#include "../utils/ProcessPatch.h"
#include "../utils/Patterns.h"
#include "eSettingsManager.h"
#include "../utils/prettyprint.h"
#include <string>

class GamePatchManager
{
private:
	HMODULE Module;
	uint64_t BaseAddr;
	Trampoline* Tramp;
	const char* Version;

public:
	GamePatchManager() : Module(nullptr), BaseAddr(0), Tramp(nullptr), Version(nullptr) {}

	bool Init(HMODULE module, const char* version)
	{
		Module = module;
		BaseAddr = (uint64_t)module;
		Version = version;

		Tramp = Trampoline::MakeTrampoline(module);
		if (!Tramp)
		{
			printfError("Failed to create Trampoline!");
			return false;
		}

		return true;
	}

	Trampoline* GetTrampoline() { return Tramp; }
	HMODULE GetModule() { return Module; }
	uint64_t GetBaseAddr() { return BaseAddr; }

	// Find a pattern in the game module
	uint64_t* FindPattern(std::string_view pattern)
	{
		hook::pattern p = hook::make_module_pattern(Module, pattern);
		if (!p.count_hint(1).empty())
			return p.get(0).get<uint64_t>(0);
		return nullptr;
	}

	// Find pattern with cache support
	uint64_t* FindCachedPattern(const std::string& pattern)
	{
		uint64_t cached = CachedPatternsMgr->Load((char*)pattern.c_str());
		if (cached)
			return (uint64_t*)cached;

		uint64_t* result = FindPattern(pattern);
		if (result)
			CachedPatternsMgr->Save((char*)pattern.c_str(), (uint64_t)result);
		return result;
	}

	// Find pattern, log result, return address. Returns 0 on failure.
	uint64_t FindPatternOrFail(const std::string& pattern, const char* name)
	{
		if (pattern.empty())
		{
			printfError("%s Not Specified. Please Add Pattern to ini file!\n", name);
			return 0;
		}

		uint64_t* result = FindCachedPattern(pattern);
		if (!result)
		{
			printfError("Couldn't find %s Pattern\n", name);
			return 0;
		}

		if (SettingsMgr->iLogLevel)
			printf("%s Pattern found at: %p\n", name, result);

		return (uint64_t)result;
	}

	// Patch return at address with nops to fill size bytes
	void PatchReturn(uint64_t addr, uint8_t size)
	{
		ProcessPatch::PatchReturn(addr, size);
	}

	// Convert conditional jump to unconditional
	void ConditionalToUnconditional(uint64_t addr)
	{
		ProcessPatch::ConditionalToUnconditional(addr);
	}

	// Redirect a call/jmp to a new target
	void RedirectCall(uint64_t addr, uint64_t newTarget, uint8_t opcodeSize = 0)
	{
		if (opcodeSize == 0)
			opcodeSize = ProcessPatch::GetOpCodeSize(addr);
		ProcessPatch::RedirectCall(addr, newTarget, opcodeSize);
	}

	// Proxy a call site: redirect to proxy, save original function pointer
	template <typename T>
	bool ProxyCallSite(uint64_t addr, void* proxyFunc, T** originalFunc, PatchTypeEnum patchType = PATCH_CALL)
	{
		if (!Tramp)
		{
			printfError("Trampoline not initialized!");
			return false;
		}
		ProcessPatch::ProxyCallSite(Tramp, addr, proxyFunc, originalFunc, patchType);
		return true;
	}

	// Replace a function entry point with a jump to replacement
	bool ReplaceFunction(uint64_t addr, void* replacement)
	{
		if (!Tramp)
		{
			printfError("Trampoline not initialized!");
			return false;
		}
		ProcessPatch::ReplaceFunction(Tramp, addr, replacement);
		return true;
	}

	// Full hook workflow: find pattern -> proxy call site -> save original -> log result
	// Returns true on success
	template <typename T>
	bool HookPattern(const std::string& pattern, const char* name, int64_t offset, void* proxyFunc, T** originalFunc, PatchTypeEnum patchType = PATCH_CALL)
	{
		uint64_t patAddr = FindPatternOrFail(pattern, name);
		if (!patAddr)
			return false;

		uint64_t callAddr = patAddr + offset;

		if (!ProxyCallSite(callAddr, proxyFunc, originalFunc, patchType))
			return false;

		printfSuccess("%s Hooked", name);
		return true;
	}
};

extern GamePatchManager* GamePatcher;
