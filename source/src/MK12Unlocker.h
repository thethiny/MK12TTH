#pragma once
#include "../includes.h"

struct AuthObjectStruct {
	uint64_t* innerSelfPointer;
	uint64_t* innerSelfPointerCopy;
	uint64_t* innerSelfPointer3;
	uint64_t ignore[4]; // This is probably another struct, but I currently do not care since they're sequential
	char fail_on_missing[16];
	uint64_t fail_on_missing_size;
	uint64_t fail_on_missing_size_copy;
	uint64_t ignore2[8]; // Same comment as above
	char* steam_ticket_string;
	uint64_t padding;
	uint64_t steam_ticket_string_size;
	uint64_t steam_ticket_string_size_2;
	uint64_t padding2;
	uint64_t* ignore3;
	char platform_name[16];
	uint64_t platform_name_size;
	// The rest doesn't matter
};

struct UnknownStructName { // It is actually a class
	uint64_t ignore1;
	uint64_t ignore2;
	uint64_t* vftable1;
	uint64_t ignore3;
	uint64_t ignore4;
	uint64_t ignore5;
	uint64_t* vftable2;
	AuthObjectStruct* AuthObject;
};

namespace MK12Unlocker {
	extern UnknownStructName* AuthenticationStruct;
}