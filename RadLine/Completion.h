#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

class bufstring;

struct Extra
{
    SHORT line;
    SHORT length;
};

void Complete(const HANDLE hStdOutput, bufstring& line, size_t* i, Extra* extra, const COORD size);
void CleanUpExtra(const HANDLE hStdOutput, Extra* extra);
//void ExpandAlias(bufstring& line);
