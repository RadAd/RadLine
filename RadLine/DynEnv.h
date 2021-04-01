#pragma once
#include <Windows.h>

extern "C" {
    extern decltype(&GetEnvironmentVariableW)  pOrigGetEnvironmentVariableW;

    __declspec(dllexport)
        DWORD WINAPI RadGetEnvironmentVariableW(
            _In_opt_ LPCWSTR lpName,
            _Out_writes_to_opt_(nSize, return +1) LPWSTR lpBuffer,
            _In_ DWORD nSize
        );
};
