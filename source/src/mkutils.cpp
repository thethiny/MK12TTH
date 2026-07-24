#include "mkutils.h"
#include "prettyprint.h"

LibMap IATable;

// String Utilities

std::string toLower(std::string s)
{
	std::string new_string("");
	for (int i = 0; i < s.length(); i++)
	{
		new_string += std::tolower(s[i]);
	}
	return new_string;
}

std::string toUpper(std::string s)
{
	std::string new_string("");
	for (int i = 0; i < s.length(); i++)
	{
		new_string += std::toupper(s[i]);
	}
	return new_string;
}

std::string GetFileName(std::string filename)
{
	std::string basename;
	size_t pos = filename.find_last_of("/\\"); // Or
	if (pos != -1)
	{
		basename = filename.substr(pos + 1);
		return basename;
	}
	return filename;
}

std::string GetProcessName()
{
	CHAR fileName[MAX_PATH + 1];
	DWORD chars = GetModuleFileNameA(NULL, fileName, MAX_PATH + 1);
	if (chars)
	{
		return GetFileName(std::string(fileName));
	}
	return "";
}

std::string GetDirName()
{
	CHAR fileName[MAX_PATH + 1];
	DWORD chars = GetModuleFileNameA(NULL, fileName, MAX_PATH + 1);
	if (chars)
	{
		std::string basename;
		std::string filename = std::string(fileName);
		size_t pos = filename.find_last_of('\\');
		if (pos != -1)
		{
			basename = filename.substr(0, pos);
			return basename;
		}
		return filename;
	}
	return "";
}

int StringToVK(std::string sKey)
{
	for (auto& c : sKey) c = toupper(c); // To Upper sKey

	if (sKey == "SHIFT")
	{
		return VK_SHIFT;
	}

	if (sKey == "TAB")
	{
		return VK_TAB;
	}

	if (sKey == "SPACE")
	{
		return VK_SPACE;
	}

	if (sKey == "ALT")
	{
		return VK_MENU;
	}

	if (sKey.substr(0, 6) == "NUMPAD") // Numpad
	{
		char cLast = sKey[6];
		switch (cLast)
		{
		case '+':
			return VK_ADD;
		case '-':
			return VK_SUBTRACT;
		case 'E': //NUMPAD Enter
			return VK_RETURN;
		case '*':
			return VK_MULTIPLY;
		case '/':
			return VK_DIVIDE;
		default:
			return cLast + 0x30; // NUMPAD Number
		}

	}
	if (sKey.length() == 1)
	{
		if (sKey[0] >= '0' && sKey[0] <= 'Z' && sKey[0] != 0x40) // Letter or Number. 0x40 is not available which is @
			return sKey[0]; // Letters and Numbers' VK are their ASCII Repr
		if (sKey[0] == '/' || sKey[0] == '?')
			return VK_OEM_2;
		if (sKey[0] == '-')
			return VK_OEM_MINUS;
		if (sKey[0] == '+')
			return VK_OEM_PLUS;
		if (sKey[0] == '`' || sKey[0] == '~')
			return VK_OEM_3;
	}

	if (sKey[0] == 'F' && sKey.length() > 1 && sKey.length() <= 3) // F1-F12 buttons
	{
		std::string digits = sKey.substr(1, sKey.length() - 1).c_str();
		return std::stoi(digits.c_str()) + 0x70 - 1;
	}

	// Arrows
	if (sKey == "DOWN")
		return VK_DOWN;
	if (sKey == "UP")
		return VK_UP;
	if (sKey == "LEFT")
		return VK_LEFT;
	if (sKey == "RIGHT")
		return VK_RIGHT;

	// Intentional crash - catches bad INI keybind config during testing
	RaiseException("Button Binding Unsupported!");
}

void RaiseException(const char* Message, int64_t ErrorCode)
{
	std::string ErrorMessage = "Error: " + std::string(Message);
	MessageBoxA(0, ErrorMessage.c_str(), "Error", MB_ICONERROR);
	throw(ErrorCode);
}

LibFuncStruct ParseLibFunc(CPPython::string path)
{
	LibFuncStruct LFS;
	auto vars = path.rsplit(".", 1);

	if (vars.size() != 2)
		return LFS;

	LFS.FullName = path;
	LFS.LibName = vars[0].lower();
	LFS.ProcName = vars[1];

	LFS.LibName = CPPython::string(LFS.LibName).endswith(".dll") || CPPython::string(LFS.LibName).endswith(".exe") ? LFS.LibName : LFS.LibName + ".dll";

	LFS.bIsValid = true;

	return LFS;
}

void ParseLibFunc(LibFuncStruct& LFS)
{
	LFS = ParseLibFunc(LFS.FullName);
}
