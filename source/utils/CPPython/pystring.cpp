#include "pystring.h"

std::vector<PyString> PyString::split(PyString ToFind, int MaxSplit)
{
	std::vector<PyString> found;
	PyString ToSplit = *this;
	for (size_t i = 0; i < ToSplit.length(); i++)
	{
		if (MaxSplit >= 0 && found.size() >= MaxSplit)
			break;
		for (int j = 0; j < ToFind.length(); j++)
			if (ToSplit[i] == ToFind[j])
			{
				found.push_back(ToSplit.substr(0, i));
				ToSplit = ToSplit.substr(i + 1);
				i--;
				break;
			}
	}

	if (!this->empty())
	{
		found.push_back(ToSplit);
	}

	return found;
}

std::vector<PyString> PyString::rsplit(PyString ToFind, int MaxSplit)
{
	std::vector<PyString> found;
	PyString ToSplit = *this;
	for (int64_t i = ToSplit.length() - 1; i >= 0; i--)
	{
		if (MaxSplit >= 0 && found.size() >= MaxSplit)
			break;

		for (int j = 0; j < ToFind.length(); j++)
			if (ToSplit[i] == ToFind[j])
			{
				found.insert(found.begin(), ToSplit.substr(i + 1));
				ToSplit = ToSplit.substr(0, i);
				i = ToSplit.length();
				break;
			}
	}
	if (!ToSplit.empty())
	{
		found.insert(found.begin(), ToSplit);
	}

	return found;
}

PyString PyString::lower()
{
	PyString new_string("");
	for (int i = 0; i < this->length(); i++)
	{
		new_string += std::tolower((*this)[i]);
	}
	return new_string;
}

PyString PyString::upper()
{
	PyString new_string("");
	for (int i = 0; i < this->length(); i++)
	{
		new_string += std::toupper((*this)[i]);
	}
	return new_string;
}

PyString PyString::join(PyString* str, uint64_t size)
{
	PyString x = "";
	for (uint64_t i = 0; i < size; i++)
	{
		x += str [i];
		if (i == size - 1)
		{
			x += *this;
		}
	}
	return x;
}

PyString PyString::strip(PyString ToStrip)
{
	PyString x = *this;
	uint64_t pos = 0, posend = x.length();
	for (auto i = 0; i < x.length(); i++)
	{
		bool in = false;
		bool in2 = false;
		for (auto j = 0; j < ToStrip.length(); j++)
		{
			if (x[i] == ToStrip[j])
			{
				in = true;
			}
			if (x[-1] == ToStrip[j])
			{
				in2 = true;
			}
		}
		if (in)
			pos = i+1;
		if (in2)
			posend = x.length() - i - 1;
	}

	return x.substr(pos, posend-pos);
}

bool PyString::startswith(PyString Start)
{
	return strncmp(*this, Start, Start.length()) == 0;
}

bool PyString::endswith(PyString End)
{
	if (End.length() > this->length())
		return false;
	PyString NewStr = this->substr(this->length() - End.length());
	return strncmp(NewStr, End, End.length()) == 0;
}

PyString PyString::replace(PyString Replacement, PyString ReplaceWith)
{
	if (this->length() < Replacement.length())
		return *this;

	if (this->length() == Replacement.length())
	{
		return strcmp(this->c_str(), Replacement.c_str()) ? (*this) : ReplaceWith;
	}

	size_t i = 0;
	PyString NewStr = *this;

	while (i < NewStr.length())
	{
		PyString s1 = NewStr.substr(i, Replacement.length());
		if (strncmp(s1.c_str(), Replacement.c_str(), Replacement.length()) == 0)
		{
			NewStr = NewStr.substr(0, i) + ReplaceWith + NewStr.substr(i + Replacement.length(), NewStr.length() - i - Replacement.length());
			i += ReplaceWith.length();
			continue;
		}
		i++;
	}
	return NewStr;
}