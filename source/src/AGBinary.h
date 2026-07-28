#pragma once
#include <string>
#include <cstdint>

namespace AGBinary {

	uint64_t ReadBE(const char* data, int32_t& cursor, int bytes);
	bool Skip(const char* data, int32_t size, int32_t& cursor);
	std::string ReadString(const char* data, int32_t size, int32_t& cursor);
	int32_t EnterMap(const char* data, int32_t size, int32_t& cursor);
	bool FindKey(const char* data, int32_t size, int32_t& cursor, const char* key);
	std::string GetString(const char* data, int32_t size, const char* key);

}
