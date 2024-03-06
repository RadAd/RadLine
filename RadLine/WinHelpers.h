#pragma once

#include <vector>
#include <string>
#include <Windows.h>

inline BOOL FileExists(LPCSTR szPath)
{
    DWORD dwAttrib = GetFileAttributesA(szPath);

    return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
        !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

inline BOOL FileExists(LPCWSTR szPath)
{
    DWORD dwAttrib = GetFileAttributesW(szPath);

    return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
        !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

template <size_t size>
inline DWORD GetCurrentDirectoryA(CHAR(&rBuffer)[size])
{
    return GetCurrentDirectoryA(size, rBuffer);
}

template <size_t size>
inline DWORD GetCurrentDirectoryW(WCHAR(&rBuffer)[size])
{
    return GetCurrentDirectoryW(size, rBuffer);
}

template <size_t size>
inline DWORD GetEnvironmentVariableW(LPCWSTR lpName, WCHAR (&rBuffer)[size])
{
    DWORD len = GetEnvironmentVariableW(lpName, rBuffer, size);
    _ASSERTE(len < size);
    rBuffer[len] = L'\0';
    return len;
}

inline DWORD GetEnvironmentVariableW(LPCWSTR lpName, std::vector<wchar_t>& buffer)
{
    return GetEnvironmentVariableW(lpName, buffer.data(), (DWORD) buffer.size());
}

inline DWORD GetEnvironmentVariableA(LPCSTR lpName, std::vector<char>& buffer)
{
    return GetEnvironmentVariableA(lpName, buffer.data(), (DWORD)buffer.size());
}

inline int GetEnvironmentInt(LPCWSTR name, int def = 0)
{
    wchar_t value[100] = L"";
    if (GetEnvironmentVariableW(name, value) != 0)
        return _wtoi(value);
    else
        return def;
}

template <size_t size>
inline DWORD ExpandEnvironmentStringsA(LPCSTR lpSrc, CHAR(&rBuffer)[size])
{
    DWORD len = ExpandEnvironmentStringsA(lpSrc, rBuffer, size);
    _ASSERTE(len < size);
    rBuffer[len] = L'\0';
    return len;
}

template <size_t size>
inline DWORD ExpandEnvironmentStringsW(LPCWSTR lpSrc, WCHAR(&rBuffer)[size])
{
    DWORD len = ExpandEnvironmentStringsW(lpSrc, rBuffer, size);
    _ASSERTE(len < size);
    rBuffer[len] = L'\0';
    return len;
}

inline DWORD ExpandEnvironmentStrings(_In_ LPWSTR lpSrc, _In_ DWORD nSize)
{
    std::wstring buffer(lpSrc);
    return ExpandEnvironmentStrings(buffer.c_str(), lpSrc, nSize);
}

inline CONSOLE_SCREEN_BUFFER_INFO GetConsoleScreenBufferInfo(const HANDLE hStdOutput)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi = {};
    GetConsoleScreenBufferInfo(hStdOutput, &csbi);
    return csbi;
}

inline COORD GetConsoleCursorPosition(const HANDLE hStdOutput)
{
    const CONSOLE_SCREEN_BUFFER_INFO csbi = GetConsoleScreenBufferInfo(hStdOutput);
    return csbi.dwCursorPosition;
}

inline COORD normalize(COORD p, SHORT cols)
{
    while (p.X < 0)
    {
        p.X += cols;
        --p.Y;
    }
    p.Y += p.X / cols;
    p.X %= cols;
    return p;
}

inline COORD Add(COORD p, SHORT d, SHORT cols)
{
    p.X += d;
    return normalize(p, cols);
}

inline SHORT Diff(COORD a, COORD b, SHORT cols)
{
    return a.X - b.X + (a.Y - b.Y) * cols;
}

inline bool operator==(const COORD& a, const COORD& p)
{
    return a.X == p.X && a.Y == p.Y;
}

inline void EasyWriteChar(const HANDLE hStdOutput, WCHAR c)
{
    DWORD written = 0;
    WriteConsoleW(hStdOutput, &c, 1, &written, nullptr);
}

inline void EasyWriteString(const HANDLE hStdOutput, LPCWCHAR s)
{
    DWORD written = 0;
    WriteConsoleW(hStdOutput, s, (DWORD) wcslen(s), &written, nullptr);
}

inline void EasyWriteFormat(const HANDLE hStdOutput, LPCWCHAR format, ...)
{
    WCHAR buffer[1024];
    va_list args;
    va_start(args, format);
    int len = _vsnwprintf_s(buffer, ARRAYSIZE(buffer), format, args);
    DWORD written = 0;
    WriteConsoleW(hStdOutput, buffer, len, &written, nullptr);
    va_end(args);
}

std::vector<std::wstring> findAlias(const std::wstring& s);
std::wstring getAlias(const std::wstring& s);
std::vector<std::wstring> findRegKey(const std::wstring& s);
