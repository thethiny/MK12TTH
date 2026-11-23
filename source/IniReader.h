#ifndef INIREADER_H
#define INIREADER_H
#include <windows.h>
#include <vector>

// Every ini and state file write goes through here, one writer at a time (one lock for all files, reads wait for a
// write in progress). The change goes to a temp copy next to the file, then the copy replaces the file in one step,
// so a crash or power cut mid write never leaves a half written file. Falls back to writing in place when the swap
// isn't possible. Null key removes a section, null value removes a key (as the Windows API)
BOOL IniWriteSafe(const char* szSection, const char* szKey, const char* szValue, const char* szFile);
// key=value lines of a whole section (the buffer grows until it fits), returns the length used
DWORD IniReadSection(const char* szSection, std::vector<char>& Buffer, const char* szFile);
bool IniHasKey(const char* szSection, const char* szKey, const char* szFile);

class CIniReader
{
public:
 CIniReader(char* szFileName); 
 int ReadInteger(char* szSection, char* szKey, int iDefaultValue);
 float ReadFloat(char* szSection, char* szKey, float fltDefaultValue);
 bool ReadBoolean(char* szSection, char* szKey, bool bolDefaultValue);
 char* ReadString(char* szSection, char* szKey, const char* szDefaultValue); // Whole value, new[]: the caller owns it
 void WriteInteger(char* szSection, char* szKey, int iValue);
 void WriteFloat(char* szSection, char* szKey, float fltValue);
 void WriteBoolean(char* szSection, char* szKey, bool bolValue);
 void WriteString(char* szSection, char* szKey, char* szValue);
private:
  char m_szFileName[1024]; // Room for long install paths
};
#endif//INIREADER_H