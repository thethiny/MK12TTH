#pragma once
#include "../utils/ProcessPatch.h"
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

	/** Returns the DLL filename without path or extension, lowercased. */
	std::string GetModuleName()
	{
		char path[MAX_PATH];
		GetModuleFileNameA(Module, path, MAX_PATH);
		std::string name = path;
		size_t lastSlash = name.find_last_of("\\/");
		if (lastSlash != std::string::npos)
			name = name.substr(lastSlash + 1);
		size_t dot = name.find_last_of('.');
		if (dot != std::string::npos)
			name = name.substr(0, dot);
		for (size_t i = 0; i < name.length(); i++)
			name[i] = std::tolower(name[i]);
		return name;
	}

	// Find a pattern in the game module (no cache)
	uint64_t* FindPattern(const std::string& pattern)
	{
		return ProcessPatch::FindPattern(Module, pattern);
	}

	// Find pattern, log result, return address. Returns 0 on failure.
	// cache = true: check/store in CachedPatternsMgr. false: always scan.
	uint64_t FindPatternOrFail(const std::string& pattern, const char* name, bool cache = true)
	{
		if (pattern.empty())
		{
			printfError("%s Not Specified. Please Add Pattern to ini file!\n", name);
			return 0;
		}

		uint64_t* result = nullptr;

		if (cache)
		{
			uint64_t cached = CachedPatternsMgr->Get(name);
			if (cached)
				result = (uint64_t*)cached;
		}

		if (!result)
		{
			result = FindPattern(pattern);
			if (result && cache)
				CachedPatternsMgr->Set(name, (uint64_t)result);
		}

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

	// Convert conditional jump to unconditional (handles both short and near)
	void ConditionalToUnconditional(uint64_t addr)
	{
		ProcessPatch::ConditionalToUnconditional(addr);
	}

	// Redirect a call/jmp to a new target
	void RedirectCall(uint64_t addr, uint64_t newTarget, uint8_t opcodeSize = 0)
	{
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
	template <typename T>
	bool HookPattern(const std::string& pattern, const char* name, int64_t offset, void* proxyFunc, T** originalFunc, PatchTypeEnum patchType = PATCH_CALL, bool cache = true)
	{
		uint64_t patAddr = FindPatternOrFail(pattern, name, cache);
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
