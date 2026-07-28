#include "AGBinary.h"
#include <cstring>

namespace AGBinary {

	uint64_t ReadBE(const char* data, int32_t& cursor, int bytes)
	{
		uint64_t val = 0;
		for (int i = 0; i < bytes; i++)
			val = (val << 8) | (uint8_t)data[cursor++];
		return val;
	}

	static bool SkipBool(const char* data, int32_t size, int32_t& cursor, uint8_t sub)
	{
		return true;
	}

	static bool SkipInt(const char* data, int32_t size, int32_t& cursor, uint8_t sub)
	{
		cursor += 1 << (sub / 2);
		return cursor <= size;
	}

	static bool SkipFloat(const char* data, int32_t size, int32_t& cursor, uint8_t sub)
	{
		cursor += (sub == 0) ? 4 : 8;
		return cursor <= size;
	}

	static bool SkipString(const char* data, int32_t size, int32_t& cursor, uint8_t sub)
	{
		int lenBytes = 1 << (sub % 3);
		if (cursor + lenBytes > size) return false;
		uint32_t strLen = (uint32_t)ReadBE(data, cursor, lenBytes);
		cursor += strLen;
		return cursor <= size;
	}

	static bool SkipTime(const char* data, int32_t size, int32_t& cursor, uint8_t sub)
	{
		cursor += 4;
		return cursor <= size;
	}

	static bool SkipArray(const char* data, int32_t size, int32_t& cursor, uint8_t sub)
	{
		int lenBytes = 1 << sub;
		if (cursor + lenBytes > size) return false;
		uint32_t count = (uint32_t)ReadBE(data, cursor, lenBytes);
		for (uint32_t i = 0; i < count; i++)
			if (!Skip(data, size, cursor)) return false;
		return true;
	}

	static bool SkipMap(const char* data, int32_t size, int32_t& cursor, uint8_t sub)
	{
		if (sub == 7)
		{
			cursor++;
			return Skip(data, size, cursor);
		}
		int lenBytes = 1 << (sub % 4);
		if (cursor + lenBytes > size) return false;
		uint32_t count = (uint32_t)ReadBE(data, cursor, lenBytes);
		for (uint32_t i = 0; i < count; i++)
		{
			if (!Skip(data, size, cursor)) return false;
			if (!Skip(data, size, cursor)) return false;
		}
		return true;
	}

	typedef bool (*SkipFunc)(const char*, int32_t, int32_t&, uint8_t);
	static constexpr SkipFunc SkipTable[] = {
		SkipBool,
		SkipInt,
		SkipFloat,
		SkipString,
		SkipTime,
		SkipArray,
		SkipMap,
	};

	bool Skip(const char* data, int32_t size, int32_t& cursor)
	{
		if (cursor >= size) return false;
		uint8_t tag = (uint8_t)data[cursor++];
		uint8_t category = tag >> 4;
		uint8_t sub = tag & 0x0F;
		if (category > 6) return false;
		return SkipTable[category](data, size, cursor, sub);
	}

	std::string ReadString(const char* data, int32_t size, int32_t& cursor)
	{
		if (cursor >= size) return "";
		uint8_t tag = (uint8_t)data[cursor++];
		if ((tag & 0xF0) != 0x30) return "";
		uint8_t sub = tag & 0x0F;
		int lenBytes = 1 << (sub % 3);
		if (cursor + lenBytes > size) return "";
		uint32_t strLen = (uint32_t)ReadBE(data, cursor, lenBytes);
		if (cursor + (int32_t)strLen > size || strLen > 0x100000) return "";
		std::string result(data + cursor, strLen);
		cursor += strLen;
		return result;
	}

	int32_t EnterMap(const char* data, int32_t size, int32_t& cursor)
	{
		if (cursor >= size) return -1;
		uint8_t tag = (uint8_t)data[cursor++];
		if ((tag & 0xF0) != 0x60) return -1;
		uint8_t sub = tag & 0x0F;
		if (sub > 3) return -1;
		int lenBytes = 1 << sub;
		if (cursor + lenBytes > size) return -1;
		return (int32_t)ReadBE(data, cursor, lenBytes);
	}

	bool FindKey(const char* data, int32_t size, int32_t& cursor, const char* key)
	{
		int32_t count = EnterMap(data, size, cursor);
		if (count < 0) return false;
		for (int32_t i = 0; i < count; i++)
		{
			int32_t keyCursor = cursor;
			std::string k = ReadString(data, size, cursor);
			if (k.empty()) { cursor = keyCursor; Skip(data, size, cursor); }
			if (k == key) return true;
			Skip(data, size, cursor);
		}
		return false;
	}

	std::string GetString(const char* data, int32_t size, const char* key)
	{
		int32_t cursor = 0;
		if (!FindKey(data, size, cursor, key)) return "";
		return ReadString(data, size, cursor);
	}

}
