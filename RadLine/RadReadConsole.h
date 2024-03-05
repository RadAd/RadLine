#pragma once
#include <Windows.h>

extern "C" {
    extern decltype(&ReadConsoleW)  pOrigReadConsoleW;

    __declspec(dllexport)
    BOOL WINAPI RadLineReadConsoleW(
        _In_     HANDLE  hConsoleInput,
        _Out_    LPVOID  lpBuffer,
        _In_     DWORD   nNumberOfCharsToRead,
        _Out_    LPDWORD lpNumberOfCharsRead,
        _In_opt_ PCONSOLE_READCONSOLE_CONTROL pInputControl
    );
};
