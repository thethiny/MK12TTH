#pragma once
#include <string>
#include <vector>

// Lists the files in a mod's .pak or .utoc from their own indexes. Paths are lowercase with forward slashes, without
// the engine's "../../../". Encrypted indexes can't be read
namespace PakContents {
	struct Listing {
		bool						bRead = false; // Missing, encrypted or unknown format
		std::vector<std::wstring>	Files;
	};

	Listing ReadPak(const std::wstring& Path);
	Listing ReadUtoc(const std::wstring& Path);
}
