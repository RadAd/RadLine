#pragma once
#include <Windows.h>

class bufstring;

struct Extra
{
    SHORT line;
    SHORT length;
};

void Complete(const HANDLE hStdOutput, bufstring& line, size_t* i, Extra* extra, const COORD size);
size_t Complete(const HANDLE hStdOutput, wchar_t* pStr, size_t nSize, size_t nNumberOfCharsRead, size_t i, Extra* extra, const COORD size);
void CleanUpExtra(const HANDLE hStdOutput, Extra* extra);
void ExpandAlias(bufstring& line);
