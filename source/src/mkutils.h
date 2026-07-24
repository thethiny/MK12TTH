#pragma once
#include <string>
#include "../../pch.h"
#include "../utils/ProcessPatch.h"
#include "../CPPython/cppython.h"
#undef in
#include "eSettingsManager.h"

#define			FNAME_STR(FName)			MK12::FNameFunc::ToStr(*FName)

std::string		GetProcessName();
std::string		GetDirName();
std::string		toLower(std::string s);
std::string		toUpper(std::string s);
std::string		GetFileName(std::string filename);
int				StringToVK(std::string);
void			RaiseException(const char*, int64_t = 1);

class PatternFinder
{
private:
	uint64_t address = 0;

public:
	PatternFinder() = default;
	PatternFinder(const std::string pattern, bool cache = true) { Search(pattern, cache); }
	operator uint64_t () { return address; }
	operator uint64_t* () { return (uint64_t*)address; }
	operator __int64() { return __int64(address); }
	operator bool() { return bool(address); }
	PatternFinder& operator+=(const uint64_t b) { address += b; return *this; }
	PatternFinder& operator=(const std::string pattern) { return Search(pattern, true); }

	PatternFinder& Search(const std::string pattern, bool cache = true)
	{
		if (cache)
		{
			uint64_t returned = CachedPatternsMgr->Get(pattern);
			if (returned)
			{
				address = returned;
				return *this;
			}
		}

		address = (uint64_t)ProcessPatch::SearchPattern(pattern);

		if (cache && address)
			CachedPatternsMgr->Set(pattern, address);

		return *this;
	}

	uint64_t Resolve(bool cacheTarget = true)
	{
		if (!address)
			return 0;

		uint64_t target = ProcessPatch::ResolveDestination(address);
		if (!target)
			target = ProcessPatch::ResolveDestination(address, 1, 5, 4);

		return target;
	}

	PatternFinder operator+(const int b) {
		PatternFinder obj;
		obj.address = this->address + b;
		return obj;
	}
	PatternFinder operator-(const int b) {
		PatternFinder obj;
		obj.address = this->address - b;
		return obj;
	}
	PatternFinder operator+(const uint64_t b) {
		PatternFinder obj;
		obj.address = this->address + b;
		return obj;
	}
	PatternFinder operator-(const uint64_t b) {
		PatternFinder obj;
		obj.address = this->address - b;
		return obj;
	}
};


struct LibFuncStruct {
	std::string FullName;
	std::string LibName;
	std::string ProcName;
	bool bIsValid = false;
};

LibFuncStruct	ParseLibFunc(CPPython::string);
void			ParseLibFunc(LibFuncStruct&);
extern LibMap	IATable;
