#include "PakContents.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <climits>
#include <cwctype>
#include <fstream>

namespace PakContents {
	constexpr uint32_t	PAK_MAGIC				= 0x5A6F12E1;
	constexpr size_t	PAK_FOOTER_SEARCH		= 300; // The footer is at most 221 bytes, the magic is somewhere in there
	constexpr int32_t	PAK_PATH_HASH_INDEX		= 10; // From this version the names live in a separate directory index
	constexpr int32_t	PAK_COMPRESSION_BLOCKS	= 3; // From this version entries list their compression blocks
	constexpr int32_t	PAK_TIMESTAMPS			= 1; // Up to this version entries have a timestamp
	constexpr size_t	MAX_INDEX_SIZE			= 64 * 1024 * 1024; // A mod's index is far smaller, anything bigger is broken

	constexpr char		UTOC_MAGIC[]			= "-==--==--==--==-";
	constexpr uint8_t	UTOC_PERFECT_HASH		= 4; // Versions that add tables before the directory index
	constexpr uint8_t	UTOC_PERFECT_HASH_OVERFLOW = 5;
	constexpr uint8_t	UTOC_ENCRYPTED			= 2; // Container flags
	constexpr uint8_t	UTOC_SIGNED				= 4;
	constexpr uint8_t	UTOC_INDEXED			= 8;
	constexpr uint32_t	NONE					= 0xFFFFFFFF;

	// Reads little endian values and engine strings from a buffer, stops at its end
	class Reader {
	public:
		Reader(const std::vector<uint8_t>& Data, size_t Start = 0) : Bytes(Data), At(Start) {}

		bool Ok() const { return !bFailed; }
		void Skip(size_t Count) { Need(Count); At += Count; }

		template <typename T> T Read()
		{
			T Value = {};
			if (Need(sizeof(T)))
				memcpy(&Value, Bytes.data() + At, sizeof(T));
			At += sizeof(T);
			return Value;
		}

		std::wstring ReadString()
		{
			int32_t Length = Read<int32_t>();
			std::wstring Text;
			if (Length > 0 && Need(Length))
			{
				Text.assign(Bytes.begin() + At, Bytes.begin() + At + Length - 1); // Ansi, ends with a 0
				At += Length;
			}
			else if (Length < 0 && Length != INT32_MIN && Need((size_t)-Length * 2))
			{
				Text.assign((const wchar_t*)(Bytes.data() + At), (size_t)-Length - 1);
				At += (size_t)-Length * 2;
			}
			return Text;
		}

	private:
		bool Need(size_t Count)
		{
			if (bFailed || At + Count > Bytes.size() || At + Count < At)
				bFailed = true;
			return !bFailed;
		}

		const std::vector<uint8_t>& Bytes;
		size_t At;
		bool bFailed = false;
	};

	static bool ReadBytes(std::ifstream& File, uint64_t Offset, uint64_t Size, std::vector<uint8_t>& Out)
	{
		if (Size > MAX_INDEX_SIZE)
			return false;
		Out.resize((size_t)Size);
		File.seekg((std::streamoff)Offset);
		File.read((char*)Out.data(), (std::streamsize)Size);
		return (uint64_t)File.gcount() == Size;
	}

	// The engine's own form: lowercase, forward slashes, no relative start
	static std::wstring Normalize(std::wstring Path)
	{
		std::replace(Path.begin(), Path.end(), L'\\', L'/');
		std::transform(Path.begin(), Path.end(), Path.begin(), [](wchar_t c) { return (wchar_t)towlower(c); });
		while (Path.rfind(L"../", 0) == 0)
			Path.erase(0, 3);
		while (!Path.empty() && Path[0] == L'/')
			Path.erase(0, 1);
		return Path;
	}

	Listing ReadPak(const std::filesystem::path& Path)
	{
		Listing Out;
		std::ifstream File(Path, std::ios::binary);
		if (!File)
			return Out;
		File.seekg(0, std::ios::end);
		uint64_t FileSize = (uint64_t)File.tellg();
		size_t TailSize = (size_t)std::min<uint64_t>(FileSize, PAK_FOOTER_SEARCH);
		std::vector<uint8_t> Tail;
		if (!ReadBytes(File, FileSize - TailSize, TailSize, Tail))
			return Out;

		// The footer: [guid][encrypted index] magic, version, index offset and size
		size_t Magic = std::string::npos;
		for (size_t i = TailSize >= 4 ? TailSize - 4 : 0; i-- > 0;)
		{
			if (!memcmp(Tail.data() + i, &PAK_MAGIC, 4))
			{
				Magic = i;
				break;
			}
		}
		if (Magic == std::string::npos)
			return Out;
		Reader Footer(Tail, Magic + 4);
		int32_t Version = Footer.Read<int32_t>();
		int64_t IndexOffset = Footer.Read<int64_t>();
		int64_t IndexSize = Footer.Read<int64_t>();
		bool bEncryptedIndex = Magic >= 1 && Version >= 4 && Tail[Magic - 1] != 0;
		std::vector<uint8_t> Index;
		if (!Footer.Ok() || bEncryptedIndex || IndexOffset < 0 || IndexSize <= 0 || !ReadBytes(File, IndexOffset, IndexSize, Index))
			return Out;

		Reader In(Index);
		std::wstring MountPoint = In.ReadString();
		int32_t Count = In.Read<int32_t>();
		if (Version >= PAK_PATH_HASH_INDEX)
		{
			In.Skip(8); // Path hash seed
			if (In.Read<uint32_t>())
				In.Skip(8 + 8 + 20); // Path hash index offset, size, hash
			if (!In.Read<uint32_t>())
				return Out; // Names only as hashes
			int64_t DirectoryOffset = In.Read<int64_t>();
			int64_t DirectorySize = In.Read<int64_t>();
			std::vector<uint8_t> Directory;
			if (!In.Ok() || !ReadBytes(File, DirectoryOffset, DirectorySize, Directory))
				return Out;

			Reader Dirs(Directory);
			int32_t DirCount = Dirs.Read<int32_t>();
			for (int32_t d = 0; d < DirCount && Dirs.Ok(); d++)
			{
				std::wstring DirName = Dirs.ReadString();
				int32_t FileCount = Dirs.Read<int32_t>();
				for (int32_t f = 0; f < FileCount && Dirs.Ok(); f++)
				{
					std::wstring FileName = Dirs.ReadString();
					Dirs.Skip(4); // Entry location
					Out.Files.push_back(Normalize(MountPoint + DirName + FileName));
				}
			}
			Out.bRead = Dirs.Ok();
			return Out;
		}

		// Older versions: each entry follows its name
		for (int32_t i = 0; i < Count && In.Ok(); i++)
		{
			std::wstring FileName = In.ReadString();
			In.Skip(8 + 8 + 8); // Offset, size, uncompressed size
			int32_t Compression = In.Read<int32_t>();
			if (Version <= PAK_TIMESTAMPS)
				In.Skip(8);
			In.Skip(20); // Hash
			if (Version >= PAK_COMPRESSION_BLOCKS)
			{
				if (Compression)
					In.Skip((size_t)In.Read<int32_t>() * 16);
				In.Skip(1 + 4); // Flags, block size
			}
			Out.Files.push_back(Normalize(MountPoint + FileName));
		}
		Out.bRead = In.Ok();
		return Out;
	}

	Listing ReadUtoc(const std::filesystem::path& Path)
	{
		Listing Out;
		std::ifstream File(Path, std::ios::binary);
		if (!File)
			return Out;
		File.seekg(0, std::ios::end);
		uint64_t FileSize = (uint64_t)File.tellg();
		std::vector<uint8_t> Toc;
		if (!ReadBytes(File, 0, FileSize, Toc) || Toc.size() < 0x90 || memcmp(Toc.data(), UTOC_MAGIC, 16))
			return Out;

		Reader Header(Toc, 16);
		uint8_t Version = Header.Read<uint8_t>();
		Header.Skip(3);
		uint32_t HeaderSize = Header.Read<uint32_t>();
		uint32_t EntryCount = Header.Read<uint32_t>();
		uint32_t BlockCount = Header.Read<uint32_t>();
		Header.Skip(4); // Block entry size
		uint32_t MethodCount = Header.Read<uint32_t>();
		uint32_t MethodLength = Header.Read<uint32_t>();
		Header.Skip(4); // Block size
		uint32_t DirectorySize = Header.Read<uint32_t>();
		uint8_t Flags = Toc[0x50];
		uint32_t HashSeedCount = *(uint32_t*)(Toc.data() + 0x54);
		uint32_t NoHashCount = *(uint32_t*)(Toc.data() + 0x60);
		if (!(Flags & UTOC_INDEXED) || (Flags & UTOC_ENCRYPTED))
			return Out;

		// The directory index comes after the chunk tables
		Reader In(Toc, HeaderSize);
		In.Skip((size_t)EntryCount * (12 + 10)); // Chunk ids, offsets and lengths
		if (Version >= UTOC_PERFECT_HASH)
			In.Skip((size_t)HashSeedCount * 4);
		if (Version >= UTOC_PERFECT_HASH_OVERFLOW)
			In.Skip((size_t)NoHashCount * 4);
		In.Skip((size_t)BlockCount * 12 + (size_t)MethodCount * MethodLength);
		if (Flags & UTOC_SIGNED)
		{
			int32_t HashSize = In.Read<int32_t>();
			In.Skip((size_t)HashSize * 2 + (size_t)BlockCount * 20);
		}
		std::wstring MountPoint = In.ReadString();
		struct Dir { uint32_t Name, FirstChild, NextSibling, FirstFile; };
		struct Entry { uint32_t Name, NextFile, UserData; };
		std::vector<Dir> Dirs(In.Read<uint32_t>());
		for (Dir& dir : Dirs)
			dir = In.Read<Dir>();
		std::vector<Entry> Entries(In.Read<uint32_t>());
		for (Entry& entry : Entries)
			entry = In.Read<Entry>();
		std::vector<std::wstring> Strings(In.Read<uint32_t>());
		for (std::wstring& Text : Strings)
			Text = In.ReadString();
		if (!In.Ok() || Dirs.empty())
			return Out;

		// Depth first from the root, guarded against broken links
		struct Visit { uint32_t Dir; std::wstring Path; };
		std::vector<Visit> Stack = { { 0, MountPoint } };
		size_t Steps = 0, MaxSteps = (Dirs.size() + Entries.size()) * 2 + 16;
		while (!Stack.empty() && Steps++ < MaxSteps)
		{
			Visit Current = Stack.back();
			Stack.pop_back();
			if (Current.Dir >= Dirs.size())
				continue;
			const Dir& dir = Dirs[Current.Dir];
			std::wstring Path = Current.Path;
			if (dir.Name != NONE && dir.Name < Strings.size())
				Path += Strings[dir.Name] + L"/";
			for (uint32_t f = dir.FirstFile; f != NONE && f < Entries.size() && Steps++ < MaxSteps; f = Entries[f].NextFile)
			{
				if (Entries[f].Name < Strings.size())
					Out.Files.push_back(Normalize(Path + Strings[Entries[f].Name]));
			}
			for (uint32_t c = dir.FirstChild; c != NONE && c < Dirs.size() && Steps++ < MaxSteps; c = Dirs[c].NextSibling)
				Stack.push_back({ c, Path });
		}
		Out.bRead = true;
		return Out;
	}
}
