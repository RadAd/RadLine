#pragma once

#include <vector>
#include <string>
#include <Windows.h>

inline void append(std::vector<std::wstring>& all, const std::vector<std::wstring>& other)
{
    all.insert(all.end(), other.begin(), other.end());
}

inline bool Match(const std::wstring& s, const wchar_t* p, size_t len)
{
    return (s.length() <= len && _wcsnicmp(s.c_str(), p, s.length()) == 0);
}

inline const wchar_t* PathFindName(const wchar_t* pFullName)
{
    const wchar_t* pName = wcsrchr(pFullName, L'\\');
    return pName ? pName + 1 : pFullName;
}

template <size_t size>
inline DWORD GetEnvironmentVariableW(LPCWSTR lpName, WCHAR (&rBuffer)[size])
{
    return GetEnvironmentVariableW(lpName, rBuffer, size);
}

inline COORD GetConsoleCursorPosition(const HANDLE hStdOutput)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi = {};
    GetConsoleScreenBufferInfo(hStdOutput, &csbi);
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
    int len = _vsnwprintf_s(buffer, 1024, format, args);
    DWORD written = 0;
    WriteConsoleW(hStdOutput, buffer, len, &written, nullptr);
    va_end(args);
}

std::vector<std::wstring> findFiles(const std::wstring& s, bool dirOnly);
std::vector<std::wstring> findExeFiles(const std::wstring& s);
std::vector<std::wstring> findPath(const std::wstring& s);
std::vector<std::wstring> findEnv(const std::wstring& s);
std::vector<std::wstring> findAlias(const std::wstring& s);
std::wstring getAlias(const std::wstring& s);
