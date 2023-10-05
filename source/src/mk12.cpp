#include "mkutils.h"
#include "mk12.h"

//MK12::IntroStruct					MK12::sIntroStruct;
//MK12::IntroStruct					MK12::sIntroStruct2;
MK12::ActiveMods					MK12::sActiveMods;
//MK12::CheatsStruct					MK12::sCheatsStruct;
MK12::LibMapsStruct					MK12::sLFS;
MK12::UserKeysStruct				MK12::sUserKeys;
MK12::GameReadyState				MK12::sGameState;
uint64_t*							MK12::lpGameVersionFull			= nullptr;
uint64_t*							MK12::lpGameVersion				= nullptr;
uint8_t								MK12::ulCharactersCount;
std::vector<std::wstring>			MK12::vSwappedFiles;
//std::vector<MK12::CharacterStruct>	MK12::sCharacters;
LibMap								MK12::IAT{};

// Generic Functions

void		MK12::DummyFunc()		{};
uint64_t	MK12::TrueFunc()		{ return 0x1; }
uint64_t	MK12::FalseFunc()		{ return 0x0; }


//bool MK12::operator==(const MK12::CharacterStruct& s1, std::string s2)
//{
//	return (s1.name == s2);
//}

void MK12::PopulateCharList()
{
	//std::cout << "Creating Character List" << std::endl;
	//for (const auto& file : std::filesystem::directory_iterator(GetDirName() + "\\..\\..\\Asset"))
	//{
	//	if (file.is_directory())
	//		continue;
	//	if (!file.path().has_extension() || strcmp(toLower(file.path().extension().string()).c_str(), ".xxx"))
	//		continue;
	//	
	//	std::string basename = GetFileName(file.path().string());
	//	std::string filename = toLower(basename);
	//	if (filename.length() < 28) // No Char
	//		continue;

	//	if (strcmp("charintro_scriptassets.xxx", filename.substr(filename.length() - 26).c_str())) // Not Equal
	//		continue;

	//	std::string asset_name = filename.substr(0, filename.length() - 27);
	//	if (asset_name[asset_name.length()-2] != '_')
	//		continue; // Something Fishy

	//	uint8_t IntroLetter = asset_name[asset_name.length()-1] - 'a';
	//	std::string CharName = toUpper(basename.substr(0, basename.length() - 29)); // Maintain Casing

	//	auto found = std::find(sCharacters.begin(), sCharacters.end(), CharName);
	//	if (found != sCharacters.end())
	//	{
	//		if (found->intros < IntroLetter)
	//		{
	//			found->intros = IntroLetter;
	//		}
	//			
	//	}
	//	else
	//	{
	//		CharacterStruct sChar;
	//		sChar.intros = IntroLetter;
	//		sChar.name = CharName;
	//		sCharacters.push_back(sChar);
	//	}
	//	
	//}
}


std::string MK12::GetGameVersion()
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

std::string MK12::GetGameVersionFull()
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

MK12::LibFuncStruct MK12::ParseLibFunc(CPPython::string path)
{
	MK12::LibFuncStruct LFS;
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

void MK12::ParseLibFunc(MK12::LibFuncStruct& LFS)
{
	LFS = ParseLibFunc(LFS.FullName);
}

uint64_t* MK12::GetLibProcFromNT(const MK12::LibFuncStruct& LFS)
{
	return LFS.bIsValid ? (uint64_t*)IAT[LFS.LibName][LFS.ProcName] : nullptr;
}

void MK12::PrintErrorProcNT(const MK12::LibFuncStruct& LFS, uint8_t bMode)
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