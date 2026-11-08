#pragma once
#include <filesystem>
#include <string>
#include <vector>

// Lists the files inside a mod's .pak or .utoc, from their own indexes. Paths come back lowercase with forward
// slashes and without the "../../../" the engine puts in front. Encrypted indexes can't be read
namespace PakContents {
	struct Listing {
		bool bRead = false; // False when the file is missing, encrypted or in a format we don't know
		std::vector<std::wstring> Files;
	};

	Listing ReadPak(const std::filesystem::path& Path);
	Listing ReadUtoc(const std::filesystem::path& Path);
}
