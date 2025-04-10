#include "WinHelpers.h"

namespace {
    inline bool Match(const std::wstring& s, const wchar_t* p, size_t len)
    {
        return (s.length() <= len && _wcsnicmp(s.c_str(), p, s.length()) == 0);
    }

    inline bool Match(const std::wstring& s, const wchar_t* p)
    {
        return Match(s, p, wcslen(p));
    }
};

#if 0
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
#endif

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
