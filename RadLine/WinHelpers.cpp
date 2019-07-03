#include "WinHelpers.h"

#include <Windows.h>
#include <algorithm>
#include <cctype>

namespace {
    bool CharCaseInsensitiveLess(wchar_t a, wchar_t b)
    {
        return std::toupper(a) < std::toupper(b);
    }

    bool CaseInsensitiveLess(const std::wstring& s1, const std::wstring& s2)
    {
        return std::lexicographical_compare(s1.begin(), s1.end(), s2.begin(), s2.end(), CharCaseInsensitiveLess);
    }

    bool FileNameIsDir(const std::wstring& s)
    {
        return !s.empty() && s.back() == L'\\';
    }

    bool FileNameLess(const std::wstring& s1, const std::wstring& s2)
    {
        if (FileNameIsDir(s1) && !FileNameIsDir(s2))
            return true;
        else if (!FileNameIsDir(s1) && FileNameIsDir(s2))
            return false;
        else
            return CaseInsensitiveLess(s1, s2);
    }

    std::vector<const wchar_t*> Split(wchar_t* s, wchar_t sep)
    {
        std::vector<const wchar_t*> list;

        wchar_t* b = s;
        while (*b != L'\0')
        {
            wchar_t* e = wcschr(b, sep);
            if (e != nullptr)
                *e = L'\0';

            list.push_back(b);

            if (e != nullptr)
                b = e + 1;
            else
                break;
        }

        return list;
    }
};

std::vector<std::wstring> findFiles(const std::wstring& s, bool dirOnly)
{
    std::vector<std::wstring> list;

    std::wstring FullName(s);
    FullName.erase(std::remove(FullName.begin(), FullName.end(), L'\"'), FullName.end());

#if 1
    wchar_t FullNameS[MAX_PATH];
    ExpandEnvironmentStringsW(FullName.c_str(), FullNameS, ARRAYSIZE(FullNameS));
    FullName = FullNameS;
#endif

    //FullName += L'*';

    WIN32_FIND_DATAW FindFileData;
    HANDLE hFind = FindFirstFileW(FullName.c_str(), &FindFileData);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) == 0
                && wcscmp(FindFileData.cFileName, L".") != 0
                && wcscmp(FindFileData.cFileName, L"..") != 0)
            {
                if ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                    wcscat_s(FindFileData.cFileName, L"\\");
                if (!dirOnly || (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                    list.push_back(FindFileData.cFileName);
            }
        } while (FindNextFileW(hFind, &FindFileData) != 0);
        FindClose(hFind);
    }

    std::sort(list.begin(), list.end(), FileNameLess);

    return list;
}

std::vector<std::wstring> findFiles(const std::wstring& s, const std::vector<const wchar_t*>& xl)
{
    std::vector<std::wstring> list;
    for (const wchar_t *x : xl)
    {
        std::wstring g(s);
        g += x;

        append(list, findFiles(g, false));  // TODO Maybe exclude dirs
    }
    return list;
}

std::vector<std::wstring> findExeFiles(const std::wstring& s)
{
    std::wstring f(s);
    f += L'*';

    wchar_t pathext[1024] = L"";
    GetEnvironmentVariableW(L"PATHEXT", pathext);
    std::vector<const wchar_t*> xl(Split(pathext, L';'));

    return findFiles(f, xl);
}

std::vector<std::wstring> findPath(const std::wstring& s)
{
    wchar_t path[10240] = L"";
    GetEnvironmentVariableW(L"PATH", path);
    wchar_t pathext[1024] = L"";
    GetEnvironmentVariableW(L"PATHEXT", pathext);

    std::vector<const wchar_t*> pl(Split(path, L';'));
    pl.push_back(L".");
    std::vector<const wchar_t*> xl(Split(pathext, L';'));
    std::vector<std::wstring> list;

    for (const wchar_t *p : pl)
    {
        std::wstring f(p);
        f += L'\\';
        f += s;
        f += L'*';

        append(list, findFiles(f, xl));
    }

    std::sort(list.begin(), list.end(), FileNameLess);
    auto it = std::unique(list.begin(), list.end());
    list.erase(it, list.end());
    return list;
}

std::vector<std::wstring> findEnv(const std::wstring& s)
{
    std::vector<std::wstring> list;

    LPWCH env = GetEnvironmentStringsW();
    const wchar_t*e = env;
    while (*e != L'\0')
    {
        if (Match(s, e, wcslen(e)))
        {
            const wchar_t* eq = wcschr(e, L'=');
            std::wstring n(e, eq - e);
            list.push_back(L'%' + n + L'%');
        }
        e += wcslen(e) + 1;
    }
    FreeEnvironmentStrings(env);

    return list;
}

std::vector<std::wstring> findAlias(const std::wstring& s)
{
    std::vector<std::wstring> list;
    wchar_t buf[10240] = L"";
    GetConsoleAliases(buf, ARRAYSIZE(buf), L"cmd.exe");  // TODO Use this exe
    for (const wchar_t* a = buf; *a != L'\0'; a += wcslen(a) + 1)
    {
        const wchar_t* eq = wcschr(a, L'=');
        if (Match(s, a, eq - a))
        {
            list.push_back(std::wstring(a, eq - a));
        }
    }
    return list;
}

std::wstring getAlias(const std::wstring& s)
{
    wchar_t buf[1024] = L"";
    GetConsoleAliasW((LPWSTR) s.c_str(), buf, ARRAYSIZE(buf), L"cmd.exe");  // TODO Use this exe
    return buf;
}
