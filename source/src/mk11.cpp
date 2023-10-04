#include "mk11.h"

MK11::IntroStruct					MK11::sIntroStruct;
MK11::IntroStruct					MK11::sIntroStruct2;
MK11::ActiveMods					MK11::sActiveMods;
MK11::CheatsStruct					MK11::sCheatsStruct;
MK11::LibMapsStruct					MK11::sLFS;
MK11::UserKeysStruct				MK11::sUserKeys;
MK11::GameReadyState				MK11::sGameState;
uint64_t*							MK11::lpGameVersionFull			= nullptr;
uint64_t*							MK11::lpGameVersion				= nullptr;
uint8_t								MK11::ulCharactersCount;
std::vector<std::wstring>			MK11::vSwappedFiles;
std::vector<MK11::CharacterStruct>	MK11::sCharacters;
LibMap								MK11::IAT{};

// Generic Functions

void		MK11::DummyFunc()		{};
uint64_t	MK11::TrueFunc()		{ return 0x1; }
uint64_t	MK11::FalseFunc()		{ return 0x0; }


bool MK11::operator==(const MK11::CharacterStruct& s1, std::string s2)
{
	return (s1.name == s2);
}

void MK11::PopulateCharList()
{
	std::cout << "Creating Character List" << std::endl;
	for (const auto& file : std::filesystem::directory_iterator(GetDirName() + "\\..\\..\\Asset"))
	{
		if (file.is_directory())
			continue;
		if (!file.path().has_extension() || strcmp(toLower(file.path().extension().string()).c_str(), ".xxx"))
			continue;
		
		std::string basename = GetFileName(file.path().string());
		std::string filename = toLower(basename);
		if (filename.length() < 28) // No Char
			continue;

		if (strcmp("charintro_scriptassets.xxx", filename.substr(filename.length() - 26).c_str())) // Not Equal
			continue;

		std::string asset_name = filename.substr(0, filename.length() - 27);
		if (asset_name[asset_name.length()-2] != '_')
			continue; // Something Fishy

		uint8_t IntroLetter = asset_name[asset_name.length()-1] - 'a';
		std::string CharName = toUpper(basename.substr(0, basename.length() - 29)); // Maintain Casing

		auto found = std::find(sCharacters.begin(), sCharacters.end(), CharName);
		if (found != sCharacters.end())
		{
			if (found->intros < IntroLetter)
			{
				found->intros = IntroLetter;
			}
				
		}
		else
		{
			CharacterStruct sChar;
			sChar.intros = IntroLetter;
			sChar.name = CharName;
			sCharacters.push_back(sChar);
		}
		
	}
}


std::string MK11::GetGameVersion()
{
	if (!lpGameVersion)
	{
		return "";
	}
	if (!*lpGameVersion)
	{
		return "";
	}
	return reinterpret_cast<char*>(*lpGameVersion);
}

std::string MK11::GetGameVersionFull()
{
	if (lpGameVersionFull == NULL)
	{
		return "";
	}
	if (!*lpGameVersionFull)
	{
		return "";
	}
	uint64_t* ptr = (uint64_t*)(uint64_t(*lpGameVersionFull) + 0x30);
	if (!*ptr)
	{
		return "";
	}
	return reinterpret_cast<char*>(*ptr);
};

MK11::LibFuncStruct MK11::ParseLibFunc(CPPython::string path)
{
	MK11::LibFuncStruct LFS;
	auto vars = path.rsplit(".", 1);

	if (vars.size() != 2)
		//RaiseException("Incorrect Argument", -1);
		return LFS;
	
	LFS.FullName = path;
	LFS.LibName = vars[0].lower();
	LFS.ProcName = vars[1];

	LFS.LibName = CPPython::string(LFS.LibName).endswith(".dll") || CPPython::string(LFS.LibName).endswith(".exe") ? LFS.LibName : LFS.LibName + ".dll";

	LFS.bIsValid = true;

	return LFS;
}

void MK11::ParseLibFunc(MK11::LibFuncStruct& LFS)
{
	LFS = ParseLibFunc(LFS.FullName);
}

uint64_t* MK11::GetLibProcFromNT(const MK11::LibFuncStruct& LFS)
{
	return LFS.bIsValid ? (uint64_t*)IAT[LFS.LibName][LFS.ProcName] : nullptr;
}

void MK11::PrintErrorProcNT(const MK11::LibFuncStruct& LFS, uint8_t bMode)
{
	switch (bMode)
	{
	case 0:
		printfError("Couldn't patch %s!\n", LFS.ProcName); break;
	case 1:
		printfError("Couldn't find %s in %s!\n", LFS.ProcName, LFS.LibName); break;
	case 2:
		printfError("Couldn't patch %s!\n", LFS.LibName); break;
	default:
		printfError("Couldn't patch %s::%s!\n", LFS.LibName, LFS.ProcName);
	}
}