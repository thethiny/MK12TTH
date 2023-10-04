#pragma once

#include <string>
#include <iostream>
#include <vector>

template <class T, class U>
int ifin(T LHS, U RHS, size_t SIZE)
{
    int found = -1;
    for (int i = 0; i < SIZE; i++)
    {
        if (LHS == RHS[i])
        {
            found = i;
            break;
        }
    }
    return found;
}

#define in(LHS, RHS, SIZE) (ifin(LHS, RHS, SIZE)!=-1)

class PyString : public std::string
{
private:
    std::string _char_to_charptr(char c) { char C[1] = { c }; return std::string(C); }

public:
    // Constructors
    PyString() : std::string() {};
    PyString(const PyString& s) : std::string(s) {};
    PyString(const PyString* s) : std::string(*s) {};
    PyString(const std::string& s) : std::string(s) {};
    PyString(const std::string* s) : std::string(*s) {};
    PyString(const char* c) : std::string(c) {};
    PyString(char* c) : std::string(c) {};
    PyString(char c) : std::string(_char_to_charptr(c)) {};

    // Python functions
    std::vector<PyString> split(PyString ToFind = " ", int MaxSplit = -1); // Need a split function that is "OR" instead of "AND".
    std::vector<PyString> rsplit(PyString ToFind = " ", int MaxSplit = -1);
    PyString strip(PyString ToStrip = "\n\t\r ");
    PyString lower();
    PyString upper();
    PyString join(PyString*, uint64_t size);
    PyString join(std::vector<PyString> vec) { return this->join(&vec[0], vec.size()); }
    PyString replace(PyString Replacement, PyString ReplaceWith);
    char index(int64_t i) { return i >= 0 ? std::string::operator[](i) : std::string::operator[](this->length() - 1); }
    bool startswith(PyString Start);
    bool endswith(PyString End);


    // Convertors
    operator std::string() { return std::string(*this); }
    operator const char* () { return this->c_str(); }
    operator char* () { return (char*)this->c_str(); }

    // Operators
    template <class T> char operator[](T i) { return index(i); }
    
};