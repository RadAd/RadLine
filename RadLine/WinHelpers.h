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

inline bool Match(const std::wstring& s, const wchar_t* p)
{
    return Match(s, p, wcslen(p));
}

// TODO Replace with PathFindFileName from shlwapi.h
inline const wchar_t* PathFindName(const wchar_t* pFullName)
{
    const wchar_t* pName = nullptr;
    for (const wchar_t* f = L"\\/:"; *f != L'\0'; ++f)
    {
        const wchar_t* t = wcsrchr(pFullName, *f);
        if (t > pName)
            pName = t;
    }
    return pName ? pName + 1 : pFullName;
}

inline char* PathFindName(char* pFullName)
{
    char* pName = nullptr;
    for (char* f = "\\/:"; *f != L'\0'; ++f)
    {
        char* t = strrchr(pFullName, *f);
        if (t > pName)
            pName = t;
    }
    return pName ? pName + 1 : pFullName;
}

inline BOOL FileExists(LPCSTR szPath)
{
    DWORD dwAttrib = GetFileAttributesA(szPath);

    return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
        !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

template <size_t size>
inline DWORD GetEnvironmentVariableW(LPCWSTR lpName, WCHAR (&rBuffer)[size])
{
    return GetEnvironmentVariableW(lpName, rBuffer, size);
}

inline DWORD GetEnvironmentVariableW(LPCWSTR lpName, std::vector<wchar_t>& buffer)
{
    return GetEnvironmentVariableW(lpName, buffer.data(), (DWORD) buffer.size());
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
    int len = _vsnwprintf_s(buffer, ARRAYSIZE(buffer), format, args);
    DWORD written = 0;
    WriteConsoleW(hStdOutput, buffer, len, &written, nullptr);
    va_end(args);
}

enum class FindFilesE { All, DirOnly, FileOnly };
std::vector<std::wstring> findFiles(const std::wstring& s, FindFilesE filter);
std::vector<std::wstring> findExeFiles(const std::wstring& s);
std::vector<std::wstring> findPath(const std::wstring& s);
std::vector<std::wstring> findEnv(const std::wstring& s);
std::vector<std::wstring> findAlias(const std::wstring& s);
std::wstring getAlias(const std::wstring& s);
std::vector<std::wstring> findRegKey(const std::wstring& s);
