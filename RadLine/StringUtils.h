#pragma once

#include <string>
#include <vector>
//#include <locale>
//#include <codecvt>

inline std::wstring From_utf8(std::string_view str)
{
    wchar_t res[1024];
    int len = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.length(), res, ARRAYSIZE(res));
    return std::wstring(res, len);
}

inline std::string To_utf8(std::wstring_view wstr)
{
    char res[1024];
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.length(), res, ARRAYSIZE(res), nullptr, nullptr);
    return std::string(res, len);
}

inline std::vector<std::wstring_view> Split(std::wstring_view s, wchar_t sep)
{
    std::vector<std::wstring_view> list;

    size_t b = 0;
    while (b < s.length())
    {
        size_t e = s.find(sep, b);
        if (e == std::wstring_view::npos)
        {
            list.push_back(s.substr(b));
            break;
        }
        else
        {
            list.push_back(s.substr(b, e - b));
            b = e + 1;
        }
    }

    return list;
}

inline void Replace(std::wstring& input, std::wstring_view from, std::wstring_view to)
{
    size_t pos = 0;
    while (true)
    {
        size_t startPosition = input.find(from, pos);
        if (startPosition == std::wstring::npos)
            return;
        input.replace(startPosition, from.length(), to);
        pos += to.length();
    }
}
