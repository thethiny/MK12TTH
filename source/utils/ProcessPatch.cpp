#include "ProcessPatch.h"
#include "prettyprint.h"
#include <map>

using namespace hook;

// Module base addresses

int64_t ProcessPatch::GetGameEntryPoint()
{
	static int64_t addr = reinterpret_cast<int64_t>(GetModuleHandle(nullptr));
	return addr;
}

int64_t ProcessPatch::GetModuleEntryPoint(const char* name)
{
	int64_t addr = reinterpret_cast<int64_t>(GetModuleHandle(name));
	return addr;
}

// Pattern scanning

uint64_t* ProcessPatch::FindPattern(void* handle, std::string_view bytes)
{
	hook::pattern pCamPattern = hook::make_module_pattern(handle, bytes);
	if (!pCamPattern.count_hint(1).empty())
	{
		return pCamPattern.get(0).get<uint64_t>(0);
	}
	return nullptr;
}

uint64_t* ProcessPatch::FindPattern(std::string pattern)
{
	return FindPattern(GetModuleHandleA(NULL), pattern);
}

uint64_t* ProcessPatch::FindPattern(const char* pattern)
{
	return FindPattern(std::string(pattern));
}

// Instruction parsing

InstructionInfo ProcessPatch::ParseInstruction(uint64_t addr)
{
	InstructionInfo info = { 0, 0, 0, false, false };

	uint8_t b0 = *(uint8_t*)addr;
	uint8_t b1 = *(uint8_t*)(addr + 1);

	// E8: call near (1 byte opcode, 4 byte disp)
	if (b0 == 0xE8)
	{
		info = { 1, 4, 5, false, true };
		return info;
	}

	// E9: jmp near
	if (b0 == 0xE9)
	{
		info = { 1, 4, 5, false, true };
		return info;
	}

	// EB: jmp short (1 byte opcode, 1 byte disp)
	if (b0 == 0xEB)
	{
		info = { 1, 1, 2, false, true };
		return info;
	}

	// 70-7F: short conditional jump
	if (b0 >= 0x70 && b0 <= 0x7F)
	{
		info = { 1, 1, 2, false, true };
		return info;
	}

	// 0F 80-8F: near conditional jump (2 byte opcode, 4 byte disp)
	if (b0 == 0x0F && b1 >= 0x80 && b1 <= 0x8F)
	{
		info = { 2, 4, 6, false, true };
		return info;
	}

	// FF 15: indirect call [rip+disp32]
	if (b0 == 0xFF && b1 == 0x15)
	{
		info = { 2, 4, 6, true, true };
		return info;
	}

	// FF 25: indirect jmp [rip+disp32]
	if (b0 == 0xFF && b1 == 0x25)
	{
		info = { 2, 4, 6, true, true };
		return info;
	}

	// REX prefix (48, 4C) + opcode + ModRM with RIP-relative addressing
	if (b0 == 0x48 || b0 == 0x4C)
	{
		uint8_t opcode = b1;
		uint8_t modrm = *(uint8_t*)(addr + 2);
		uint8_t mod = (modrm >> 6) & 0x3;
		uint8_t rm = modrm & 0x7;

		// mod=00 rm=101 means RIP-relative
		if (mod == 0x00 && rm == 0x05)
		{
			// 8D = lea, 8B = mov load, 89 = mov store, 3B = cmp
			if (opcode == 0x8D || opcode == 0x8B || opcode == 0x89 || opcode == 0x3B)
			{
				info = { 3, 4, 7, false, true };
				return info;
			}
		}
	}

	// 80 3D: cmp byte ptr [rip+disp32], imm8
	if (b0 == 0x80 && b1 == 0x3D)
	{
		info = { 2, 4, 7, false, true }; // 2 opcode + 4 disp + 1 imm8 = 7
		return info;
	}

	return info; // isValid = false
}

// Opcode helpers (manual params)

int32_t ProcessPatch::GetOffsetFromOpCode(uint64_t Caller, uint64_t Offset, uint16_t Size)
{
	int32_t offset = 0;
	memcpy(&offset, (uint64_t*)(Caller + Offset), Size);
	return offset;
}

uint64_t ProcessPatch::GetDestinationFromOpCode(uint64_t Caller, uint64_t Offset, uint64_t FuncLen, uint16_t Size)
{
	int32_t offset = GetOffsetFromOpCode(Caller, Offset, Size);
	return uint64_t(Caller + offset) + FuncLen;
}

// Auto-detect resolve

uint64_t ProcessPatch::ResolveDestination(uint64_t addr)
{
	InstructionInfo info = ParseInstruction(addr);
	if (!info.isValid || !info.dispSize)
		return 0;

	int32_t disp = 0;
	memcpy(&disp, (void*)(addr + info.dispOffset), info.dispSize);

	uint64_t target = addr + info.totalSize + disp;

	if (info.isIndirect)
		target = *(uint64_t*)target;

	return target;
}

// PE parsing

LibMap ProcessPatch::ParsePEHeader()
{
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)GetModuleHandleA(NULL);
	PIMAGE_NT_HEADERS pNTHeader = (PIMAGE_NT_HEADERS)RVAtoLP((PBYTE)pDosHeader, pDosHeader->e_lfanew);

	LibMap IAT{};

	if (pNTHeader->Signature != IMAGE_NT_SIGNATURE)
		return IAT;

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

		// Convert to lowercase for case-insensitive lookup
		std::string szLibraryStoreName = szLibrary;
		for (size_t j = 0; j < szLibraryStoreName.length(); j++)
			szLibraryStoreName[j] = std::tolower(szLibraryStoreName[j]);

		IAT[szLibraryStoreName] = FunctionsMap;
	}

	return IAT;
}

uint64_t ProcessPatch::HashTextSectionOfHost()
{
	LARGE_INTEGER start, end, freq;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&start);

	HMODULE base = GetModuleHandleW(nullptr);
	if (!base) return 0;

	auto dos = (PIMAGE_DOS_HEADER)base;
	if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;

	auto nt = (PIMAGE_NT_HEADERS)((BYTE*)base + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;

	auto section = IMAGE_FIRST_SECTION(nt);
	for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
		if (memcmp(section->Name, ".text", 5) == 0) {
			uint8_t* data = (uint8_t*)base + section->VirtualAddress;
			DWORD size = section->Misc.VirtualSize;

			uint64_t hash = 14695981039346656037ULL;
			for (DWORD j = 0; j < size; ++j)
				hash = (hash ^ data[j]) * 1099511628211ULL;

			QueryPerformanceCounter(&end);
			double elapsedMs = (end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart;

			printf(".text hash: 0x%016llX | Time: %.3f ms\n", hash, elapsedMs);
			return hash;
		}
	}

	QueryPerformanceCounter(&end);
	double elapsedMs = (end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart;
	printf("Failed to find .text | Time: %.3f ms\n", elapsedMs);
	return 0;
}

// RegisterHacks

namespace RegisterHacks {

	MoveToRegister* MoveToRAX;
	MoveToRegister* MoveToRBX;
	MoveToRegister* MoveToRCX;
	MoveToRegister* MoveToRDX;
	MoveToRegister* MoveToR8;
	MoveToRegister* MoveToR9;
	MoveToRegister* MoveToR10;
	MoveToRegister* MoveToR11;
	MoveToRegister* MoveToR12;
	MoveToRegister* MoveToR13;
	MoveToRegister* MoveToR14;
	MoveToRegister* MoveToR15;

	bool			bIsEnabled = false;

	void EnableRegisterHacks()
	{
		if (bIsEnabled)
			return;
		printf("Enabling Register Hack Functions\n");
		uint8_t* CallSpace = new uint8_t[4*12 + 1];
		DWORD oldProtect;
		VirtualProtect(CallSpace, 4*12 + 1, PAGE_EXECUTE_READWRITE, &oldProtect);

		uint32_t ASMs[] = {
			0xC3C88948, // RAX
			0xC3CB8948,
			0xC3C98948,
			0xC3CA8948,
			0xC3C88949, // R8
			0xC3C98949,
			0xC3CA8949,
			0xC3CB8949,
			0xC3CC8949,
			0xC3CD8949,
			0xC3CE8949,
			0xC3CF8949,
		};

		MoveToRegister** Funcs[] = {
			&MoveToRAX,
			&MoveToRBX,
			&MoveToRCX,
			&MoveToRDX,
			&MoveToR8,
			&MoveToR9,
			&MoveToR10,
			&MoveToR11,
			&MoveToR12,
			&MoveToR13,
			&MoveToR14,
			&MoveToR15,
		};

		for (uint64_t i = 0; i < 12; i++)
		{
			uint8_t* funcAddr = CallSpace + (i * 4);
			memcpy(CallSpace + (i * 4), ASMs + i, 4);
			*Funcs[i] = (MoveToRegister*)funcAddr;
		}

		bIsEnabled = true;
	}
}
