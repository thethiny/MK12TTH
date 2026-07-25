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

	// ── Search ──

	/** Searches for a pattern in the game module (no cache, no logging).
	 *  @param pattern  Pattern string with ? wildcards
	 *  @return Pointer to the first match, or nullptr if not found */
	uint64_t* SearchPattern(const std::string& pattern)
	{
		return ProcessPatch::SearchPattern(Module, pattern);
	}

	/** Searches for a pattern with caching, validation, and error logging.
	 *  Checks cache first, scans on miss, saves result, logs errors on failure.
	 *  @param pattern  Pattern string with ? wildcards
	 *  @param name     Hook name (used as cache key and in log messages)
	 *  @param cache    true = use cache, false = always scan (for dynamic code)
	 *  @return The pattern address, or 0 on failure */
	uint64_t ResolvePattern(const std::string& pattern, const char* name, bool cache = true)
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
			result = SearchPattern(pattern);
			if (result && cache)
				CachedPatternsMgr->Set(name, (uint64_t)result);
		}

		if (!result)
		{
			printfError("Couldn't find %s Pattern\n", name);
			return 0;
		}

		if (SettingsMgr->ShouldLog(Log::Verbose))
			printf("%s Pattern found at: %p\n", name, result);

		return (uint64_t)result;
	}

	// ── Patch ──

	/** Patches a return instruction at the given address to disable a function.
	 *  @param addr  Address to patch
	 *  @param size  Number of bytes to overwrite */
	void PatchReturnAt(uint64_t addr, uint8_t size)
	{
		ProcessPatch::PatchReturnAt(addr, size);
	}

	/** Converts a conditional jump to unconditional at the given address.
	 *  @param addr  Address of the conditional jump instruction */
	void PatchConditionalToUnconditional(uint64_t addr)
	{
		ProcessPatch::PatchConditionalToUnconditional(addr);
	}

	// ── Redirect ──

	/** Redirects a call/jmp instruction to a new target.
	 *  @param addr        Address of the call/jmp instruction
	 *  @param newTarget   Absolute address of the new target
	 *  @param opcodeSize  Bytes before displacement. 0 = auto-detect */
	void RedirectCallTo(uint64_t addr, uint64_t newTarget, uint8_t opcodeSize = 0)
	{
		ProcessPatch::RedirectCallTo(addr, newTarget, opcodeSize);
	}

	// ── Proxy ──

	/** Proxies a call/jmp instruction, redirecting to a proxy and saving the original function pointer.
	 *  @param addr          Address of the call/jmp instruction
	 *  @param proxyFunc     Your proxy function
	 *  @param originalFunc  Pointer to store the original function address
	 *  @param patchType     PATCH_CALL or PATCH_JUMP
	 *  @return true on success */
	template <typename T>
	bool ProxyCallAt(uint64_t addr, void* proxyFunc, T** originalFunc, PatchTypeEnum patchType = PATCH_CALL)
	{
		if (!Tramp)
		{
			printfError("Trampoline not initialized!");
			return false;
		}
		ProcessPatch::ProxyCallAt(Tramp, addr, proxyFunc, originalFunc, patchType);
		return true;
	}

	/** Searches for a pattern, then proxies the call/jmp at the given offset from the match.
	 *  Combines ResolvePattern + ProxyCallAt into one call with success logging.
	 *  @param pattern       Pattern string with ? wildcards
	 *  @param name          Hook name (used as cache key and in log messages)
	 *  @param offset        Byte offset from pattern match to the call/jmp instruction
	 *  @param proxyFunc     Your proxy function
	 *  @param originalFunc  Pointer to store the original function address
	 *  @param patchType     PATCH_CALL or PATCH_JUMP
	 *  @param cache         true = use pattern cache, false = always scan */
	template <typename T>
	bool ProxyByPattern(const std::string& pattern, const char* name, int64_t offset, void* proxyFunc, T** originalFunc, PatchTypeEnum patchType = PATCH_CALL, bool cache = true)
	{
		uint64_t patAddr = ResolvePattern(pattern, name, cache);
		if (!patAddr)
			return false;

		uint64_t callAddr = patAddr + offset;

		if (!ProxyCallAt(callAddr, proxyFunc, originalFunc, patchType))
			return false;

		printfSuccess("%s Hooked", name);
		return true;
	}

	// ── Replace ──

	/** Replaces a function at its entry point with a jump to a replacement.
	 *  @param addr         Address of the function entry point
	 *  @param replacement  Your replacement function
	 *  @return true on success */
	bool ReplaceFunctionWith(uint64_t addr, void* replacement)
	{
		if (!Tramp)
		{
			printfError("Trampoline not initialized!");
			return false;
		}
		ProcessPatch::ReplaceFunctionWith(Tramp, addr, replacement);
		return true;
	}
};

extern GamePatchManager* GamePatcher;
