#include "WinHelpers.h"

#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <sstream>

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

    inline bool Match(const std::wstring& s, const wchar_t* p, size_t len)
    {
        return (s.length() <= len && _wcsnicmp(s.c_str(), p, s.length()) == 0);
    }

    inline bool Match(const std::wstring& s, const wchar_t* p)
    {
        return Match(s, p, wcslen(p));
    }
};

std::vector<std::wstring> findFiles(const std::wstring& s, FindFilesE filter)
{
    std::vector<std::wstring> list;

    std::wstring FullName(s);
    FullName.erase(std::remove(FullName.begin(), FullName.end(), L'\"'), FullName.end());

#if 1
    {
        wchar_t FullNameS[MAX_PATH];
        GetCurrentDirectoryW(ARRAYSIZE(FullNameS), FullNameS);
        SetEnvironmentVariableW(L"CD", FullNameS);
        ExpandEnvironmentStringsW(FullName.c_str(), FullNameS, ARRAYSIZE(FullNameS));
        SetEnvironmentVariableW(L"CD", nullptr);
        FullName = FullNameS;
    }
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
                bool add = true;
                switch (filter)
                {
                case FindFilesE::DirOnly:
                    add = (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                    break;

                case FindFilesE::FileOnly:
                    add = (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
                    break;
                }
                if (add)
                    list.push_back(FindFileData.cFileName);
            }
        } while (FindNextFileW(hFind, &FindFileData) != 0);
        FindClose(hFind);
    }

    std::sort(list.begin(), list.end(), FileNameLess);

    return list;
}

std::vector<std::wstring> findEnv(const std::wstring& s, bool enclose)
{
    std::vector<std::wstring> list;

    LPWCH env = GetEnvironmentStringsW();
    const wchar_t* e = env;
    while (*e != L'\0')
    {
        if (Match(s, e))
        {
            const wchar_t* eq = wcschr(e, L'=');
            std::wstring n(e, eq - e);
            list.push_back(enclose ? L'%' + n + L'%' : n);
        }
        e += wcslen(e) + 1;
    }
    FreeEnvironmentStrings(env);

    return list;
}

std::vector<std::wstring> findAlias(const std::wstring& s)
{
    std::vector<std::wstring> list;
    std::vector<wchar_t> buf(10240);
    GetConsoleAliases(buf.data(), (DWORD) buf.size(), L"cmd.exe");  // TODO Use this exe
    for (const wchar_t* a = buf.data(); *a != L'\0'; a += wcslen(a) + 1)
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

std::vector<std::wstring> findRegKey(const std::wstring& s)
{
    HKEY hParentKey = NULL;
    struct
    {
        const wchar_t* name;
        HKEY hKey;
    } roots[] = {
        { L"HKLM\\", HKEY_LOCAL_MACHINE },
        { L"HKCU\\", HKEY_CURRENT_USER },
        { L"HKCR\\", HKEY_CLASSES_ROOT },
        { L"HKCC\\", HKEY_CURRENT_CONFIG },
        { L"HKU\\", HKEY_USERS },
        { L"HKEY_LOCAL_MACHINE\\", HKEY_LOCAL_MACHINE },
        { L"HKEY_CURRENT_USER\\", HKEY_CURRENT_USER },
        { L"HKEY_CLASSES_ROOT\\", HKEY_CLASSES_ROOT },
        { L"HKEY_CURRENT_CONFIG\\", HKEY_CURRENT_CONFIG },
        { L"HKEY_USERS\\", HKEY_USERS },
    };
    for (const auto& r : roots)
    {
        if (s.rfind(r.name, 0) != std::string::npos)
        {
            hParentKey = r.hKey;
            break;
        }
    }

    std::vector<std::wstring> ret;
    if (hParentKey != NULL)
    {
        size_t b = s.find(L'\\');
        size_t e = s.rfind(L'\\');
        std::wstring subkey(s.substr(b + 1, e - b));
        std::wstring partial(s.substr(e + 1));
        HKEY hKey = NULL;
        if (RegOpenKey(hParentKey, subkey.c_str(), &hKey) == ERROR_SUCCESS)
        {
            wchar_t name[1024];
            DWORD i = 0;
            DWORD len = ARRAYSIZE(name);
            while (RegEnumKeyEx(hKey, i, name, &len, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
            {
                if (Match(partial, name))
                {
                    wcscat_s(name, L"\\");
                    ret.push_back(name);
                }
                len = ARRAYSIZE(name);
                ++i;
            }
            RegCloseKey(hKey);
        }
    }
    else
    {
        for (const auto& r : roots)
        {
            if (Match(s, r.name))
                ret.push_back(r.name);
        }
    }
    return ret;
}
