#include "Completion.h"

#undef min
#undef max

#include <shlwapi.h>

#include <vector>
#include <algorithm>
#include <string_view>
#include <cctype>

#include "bufstring.h"
#include "WinHelpers.h"
#include "Debug.h"
#include "LuaUtils.h"

extern HMODULE g_hModule;

namespace {
    std::wstring_view substr(const bufstring& s, bufstring::const_iterator b, bufstring::const_iterator e)
    {
        assert(e >= b);
        assert(b >= s.begin() && b <= s.end());
        assert(e >= s.begin() && e <= s.end());
        return std::wstring_view(b, e - b);
    }

    struct string_range
    {
        bufstring::iterator begin;
        bufstring::iterator end;

        size_t length() const
        {
            return end - begin;
        }

        auto front() const
        {
            assert(end > begin);
            return *begin;
        }

        auto back() const
        {
            assert(end > begin);
            return *(end - 1);
        }

        auto substr(const bufstring& s) const
        {
            return ::substr(s, begin, end);
        }

        auto operator[](size_t i) const
        {
            assert(i >= 0);
            assert(i < length());
            return *(begin + i);
        }
    };

    bool isCommandSeparator(std::wstring_view s)
    {
        return s.compare(L"&") == 0
            || s.compare(L"|") == 0
            || s.compare(L"&&") == 0
            || s.compare(L"||") == 0;
    }

    inline bool CharCaseInsensitiveEqual(wchar_t a, wchar_t b)
    {
        return std::toupper(a) == std::toupper(b);
    }

    // TODO This could return a wstring_view
    std::wstring getLongestMatch(const std::vector<std::wstring>& list)
    {
        std::wstring match = list.front();
        for (const std::wstring& p : list)
        {
            std::size_t l = std::min(match.length(), p.length());
            auto mm = std::mismatch(match.begin(), match.begin() + l, p.begin(), CharCaseInsensitiveEqual);
            match.resize(mm.first - match.begin());
            if (match.empty())
                break;
        }
        return match;
    }

    const wchar_t* ws = L" \t";
    const wchar_t* delim2 = L"=<>|&";
    inline bool isWhiteSpace(wchar_t c) { return wcschr(ws, c) != nullptr; }
    inline bool isDelim(wchar_t c) { return wcschr(delim2, c) != nullptr; }

    std::vector<string_range> findParam(bufstring& line)
    {
        std::vector<string_range> rs;
        bufstring::iterator begin = line.begin();
        while (begin < line.end())
        {
            while (begin < line.end() && isWhiteSpace(*begin))
                ++begin;
            if (begin < line.end())
            {
                bufstring::iterator end = begin;
                while (end < line.end() && !isWhiteSpace(*end))
                {
                    if ((end == begin || *(end - 1) != '^') && isDelim(*end))
                    {
                        if (end == begin || isWhiteSpace(*(end - 1)))
                            while ((end == begin || *(end - 1) != '^') && isDelim(*end))
                                ++end;
                        break;
                    }
                    else if (*end == L'\"')
                    {
                        ++end;
                        while (end < line.end() && *end != L'\"')
                            ++end;
                        if (end < line.end())
                            ++end;
                    }
                    else
                    {
                        ++end;
                    }
                }
                rs.push_back({ begin, end });
                begin = end;
            }
        }
        return rs;
    }

    inline void LuaPush(lua_State* lua, const bufstring& s, const std::vector<string_range>& v)
    {
        lua_createtable(lua, (int)v.size(), 0);
        int i = 1;
        for (string_range w : v)
        {
            LuaPush(lua, w.substr(s));
            lua_rawseti(lua, -2, i);
            ++i;
        }
    }

    int l_DebugOut(lua_State* lua)
    {
        OutputDebugStringA(luaL_checklstring(lua, -1, nullptr));
        return 0;  /* number of results */
    }

    int l_GetEnv(lua_State* lua)
    {
        const char* s = luaL_checklstring(lua, -1, nullptr);
        if (_stricmp(s, "CD") == 0)
        {
            char FullNameS[MAX_PATH];
            if (GetCurrentDirectoryA(ARRAYSIZE(FullNameS), FullNameS) == 0)
                luaL_error(lua, "GetCurrentDirectory failed 0x%08x\n", GetLastError());
            lua_pushstring(lua, FullNameS);
        }
        else
        {
            static std::vector<char> v(1024, 0);
            DWORD ret = GetEnvironmentVariableA(s, v);
            if (ret == 0)
                luaL_error(lua, "GetEnvironmentVariable failed 0x%08x\n", GetLastError());
            else if (ret != ERROR_ENVVAR_NOT_FOUND)
            {
                v.resize((size_t) ret);
                ret = GetEnvironmentVariableA(s, v);
                if (ret == 0)
                    luaL_error(lua, "GetEnvironmentVariable failed 0x%08x\n", GetLastError());
            }
            lua_pushstring(lua, v.data());
        }
        return 1;  /* number of results */
    }

    int l_FindFiles(lua_State* lua)
    {
        FindFilesE filter = static_cast<FindFilesE>(luaL_checkinteger(lua, -1));
        std::wstring s = From_utf8(luaL_checklstring(lua, -2, nullptr));
        std::vector<std::wstring> f = findFiles(s, filter);
        LuaPush(lua, f);
        return 1;  /* number of results */
    }

    int l_FindEnv(lua_State* lua)
    {
        std::wstring s = From_utf8(luaL_checklstring(lua, -1, nullptr));
        std::vector<std::wstring> f = findEnv(s);
        LuaPush(lua, f);
        return 1;  /* number of results */
    }

    int l_FindAlias(lua_State* lua)
    {
        std::wstring s = From_utf8(luaL_checklstring(lua, -1, nullptr));
        std::vector<std::wstring> f = findAlias(s);
        LuaPush(lua, f);
        return 1;  /* number of results */
    }

    int l_FindRegKey(lua_State* lua)
    {
        std::wstring s = From_utf8(luaL_checklstring(lua, -1, nullptr));
        std::vector<std::wstring> f = findRegKey(s);
        LuaPush(lua, f);
        return 1;  /* number of results */
    }

    std::vector<std::wstring> findPotential(const bufstring& line, const std::vector<string_range>& params, std::vector<string_range>::const_iterator p, const std::wstring& s, std::size_t* i, std::wstring& msg)
    {
        std::unique_ptr<lua_State, LuaCloser> L(luaL_newstate());
        luaL_openlibs(L.get());

        lua_pushcfunction(L.get(), l_DebugOut);
        lua_setglobal(L.get(), "DebugOut");

        lua_pushcfunction(L.get(), l_GetEnv);
        lua_setglobal(L.get(), "GetEnv");

        lua_pushcfunction(L.get(), l_FindFiles);
        lua_setglobal(L.get(), "FindFiles");

        lua_pushcfunction(L.get(), l_FindEnv);
        lua_setglobal(L.get(), "FindEnv");

        lua_pushcfunction(L.get(), l_FindAlias);
        lua_setglobal(L.get(), "FindAlias");

        lua_pushcfunction(L.get(), l_FindRegKey);
        lua_setglobal(L.get(), "FindRegKey");

        char strFile[MAX_PATH];
        GetModuleFileNameA(g_hModule, strFile, ARRAYSIZE(strFile));
        char* const pFileName = PathFindFileNameA(strFile);
        const rsize_t strFileLen = ARRAYSIZE(strFile) - (pFileName - strFile);

        const char* files[] = {
            "RadLine.lua",
#ifdef _DEBUG
            "..\\..\\RadLine.lua",
#endif
            "UserRadLine.lua",
#ifdef _DEBUG
            "..\\..\\UserRadLine.lua",
#endif
        };

        for (const char* f : files)
        {
            strcpy_s(pFileName, strFileLen, f);
            if (FileExists(strFile))
            {
                if (luaL_dofile(L.get(), strFile) != LUA_OK)
                {
                    msg = LuaPopString(L.get());
                    return std::vector<std::wstring>();
                }
            }
            assert(lua_gettop(L.get()) == 0);
        }

        std::vector<std::wstring> all;

        //int fi = 0;
        //int di = 0;
        if (lua_getglobal(L.get(), "FindPotential") != LUA_TFUNCTION)
        {
            msg = L"FindPotential is not a function";
            lua_pop(L.get(), 1);
        }
        else if (//fi = lua_absindex(L.get(), 0),
                LuaPush(L.get(), line, params),
                lua_pushinteger(L.get(), std::distance(params.begin(), p) + 1),
                lua_pushinteger(L.get(), s.length()),
                //di = lua_absindex(L.get(), 0),
                lua_pcall(L.get(), 3, 2, 0) != LUA_OK)
        {
            msg = LuaPopString(L.get());
        }
        else
        {
            *i = (size_t) (lua_isnil(L.get(), -1) ? (lua_pop(L.get(), 1), 0) : LuaPopInteger(L.get()) - 1);
            all = LuaPopVectorOfStrings(L.get());
        }
        assert(lua_gettop(L.get()) == 0);

        return all;
    }

    SHORT MakeRoom(const HANDLE hConsoleOutput, SHORT req_lines)
    {
        CONSOLE_SCREEN_BUFFER_INFO csbi = {};
        GetConsoleScreenBufferInfo(hConsoleOutput, &csbi);

        if ((csbi.dwCursorPosition.Y + req_lines) > csbi.srWindow.Bottom)
        {
            if (csbi.srWindow.Bottom < (csbi.dwSize.Y - 1))
            {
                SHORT d = std::min(req_lines, (SHORT) (csbi.dwSize.Y - csbi.srWindow.Bottom));
                csbi.srWindow.Top += d;
                csbi.srWindow.Bottom += d;
                SetConsoleWindowInfo(hConsoleOutput, TRUE, &csbi.srWindow);
                req_lines -= d;
            }

            if (req_lines > 0)
            {
                SMALL_RECT sr = {};
                sr.Top = req_lines;
                sr.Left = 0;
                sr.Bottom = csbi.dwSize.Y - 1;
                sr.Right = csbi.dwSize.X - 1;
                CHAR_INFO ci = {};
                ci.Attributes = csbi.wAttributes;
                ci.Char.UnicodeChar = L' ';
                ScrollConsoleScreenBufferW(hConsoleOutput, &sr, nullptr, COORD({ 0,0 }), &ci);

                csbi.dwCursorPosition.Y -= req_lines;
                SetConsoleCursorPosition(hConsoleOutput, csbi.dwCursorPosition);
            }

            return req_lines;
        }
        else
            return 0;
    }
};

SHORT DisplayPotentials(const HANDLE hConsoleOutput, const std::vector<std::wstring>& list, const COORD size, SHORT below, Extra* extra)
{
    SHORT l = 0;
    for (const std::wstring& p : list)
        l = std::max(l, (SHORT) (p.length() + 1));
    const SHORT c = size.X / l;
    const SHORT req_lines = (SHORT) (list.size() + c - 1) / c;

    SHORT scroll = 0;
    COORD pos = GetConsoleCursorPosition(hConsoleOutput);

    if (req_lines > 10)
    {
        extra->length = 1;
        scroll -= MakeRoom(hConsoleOutput, extra->length);
        pos.Y += scroll;
        extra->line = pos.Y + below;

        const COORD newpos = { 0, extra->line };
        SetConsoleCursorPosition(hConsoleOutput, newpos);

        EasyWriteFormat(hConsoleOutput, L"Found %d possibilities", (int) list.size());
    }
    else
    {
        extra->length = req_lines;
        scroll -= MakeRoom(hConsoleOutput, extra->length);
        pos.Y += scroll;
        extra->line = pos.Y + below;

        COORD newpos = { 0, extra->line };
        SetConsoleCursorPosition(hConsoleOutput, newpos);

        SHORT cc = 0;
        for (const std::wstring& p : list)
        {
            ++cc;
            EasyWriteFormat(hConsoleOutput, L"%-*s", (int) l, p.c_str());
            if (cc == c)
            {
                ++newpos.Y;
                SetConsoleCursorPosition(hConsoleOutput, newpos);
                cc = 0;
            }
        }
    }

    SetConsoleCursorPosition(hConsoleOutput, pos);
    return scroll;
}

SHORT DisplayMessage(const HANDLE hConsoleOutput, const std::wstring& msg, const COORD size, SHORT below, Extra* extra)
{
    SHORT req_lines = 1;
    size_t l = 0;
    size_t i = msg.find(L'\n');
    while (i != std::wstring::npos)
    {
        size_t len = i - l;
        req_lines += (SHORT) (len / size.X);
        l = i;
        i = msg.find(L'\n', i + 1);
    }
    {
        size_t len = msg.length() - l;
        req_lines += (SHORT)(len / size.X);
    }

    SHORT scroll = 0;
    COORD pos = GetConsoleCursorPosition(hConsoleOutput);

    extra->length = req_lines;
    scroll -= MakeRoom(hConsoleOutput, extra->length);
    pos.Y += scroll;
    extra->line = pos.Y + below;

    COORD newpos = { 0, extra->line };
    SetConsoleCursorPosition(hConsoleOutput, newpos);

    DWORD written = 0;
    WriteConsoleW(hConsoleOutput, msg.c_str(), (DWORD) msg.length(), &written, nullptr);

    SetConsoleCursorPosition(hConsoleOutput, pos);
    return scroll;
}

void Complete(const HANDLE hConsoleOutput, bufstring& line, size_t* i, Extra* extra, const COORD size)
{
    std::vector<string_range> params = findParam(line);

    std::vector<string_range>::const_iterator p = params.begin();
    while (p != params.end() && (line.begin() + *i) > p->end)
        ++p;

    const std::wstring substr = p != params.end() && (line.begin() + *i) >= p->begin
        ? std::wstring(::substr(line, p->begin, std::min(line.begin() + *i, p->end)))
        : std::wstring();

    std::wstring msg;
    std::size_t rp = 0;
    const std::vector<std::wstring> list = findPotential(line, params, p, substr, &rp, msg);

    const COORD posstart = Add(GetConsoleCursorPosition(hConsoleOutput), -(SHORT)*i, size.X);

    if (!msg.empty())
    {
        SHORT below = (SHORT)(line.length() / size.X - *i / size.X + 1);
        DisplayMessage(hConsoleOutput, msg, size, below, extra);
    }
    else if (!list.empty())
    {
        const std::wstring match = getLongestMatch(list);
        bufstring::iterator refresh = line.end();

        if (match.length() > (substr.length() - rp) || list.size() == 1)
        {
            string_range r = p != params.end() ? *p : string_range({ line.end(), line.end() });

            bool openquote = false;
            if (r.begin < line.end())
            {
                if (r.front() == L'"')
                {
                    openquote = true;
                    ++r.begin;
                    --rp;
                }
                else if (match.find(L' ') != std::wstring::npos)
                {
                    openquote = true;
                    line.insert(r.begin, L'"');
                    refresh = std::min(refresh, r.begin);
                    ++r.begin;
                    ++r.end;
                    ++* i;
                }
            }

            bool removequote = rp > 0 && r[rp - 1] == L'"';

            if (substr.compare(rp, std::wstring::npos, match) != 0)
            {
                // TODO Make sure replace doesn't go over nSize
                string_range nr = r;
                nr.begin += rp;
                if (removequote)
                    --nr.begin;
                line.replace(nr.begin, nr.length(), match);
                refresh = std::min(refresh, nr.begin);
                *i -= nr.length();
                *i += match.length();
                r.end = nr.begin + match.length();
            }

            bool closequote = openquote && r.back() != L'"';
            if (closequote)
            {
                line.insert(r.end, L'\"');
                refresh = std::min(refresh, r.end);
                ++*i;
            }

            if (list.size() == 1 && match.back() != L'\\' && *i == line.length())
            {
                refresh = std::min(refresh, line.end());
                line += L' ';
                ++*i;
            }

            {
                const COORD pos = Add(posstart, (SHORT)(refresh - line.begin()), size.X);
                SetConsoleCursorPosition(hConsoleOutput, pos);
                EasyWriteString(hConsoleOutput, refresh);
                // TODO This could cause the screen to scroll shifting posstart
            }
            const int scroll = std::max(Add(posstart, (SHORT)line.length(), size.X).Y - size.Y + 1, 0);
            const COORD newposstart = Add(posstart, (SHORT)-scroll * size.X, size.X);
            {
                const COORD pos = Add(newposstart, (SHORT)*i, size.X);
                SetConsoleCursorPosition(hConsoleOutput, pos);
            }

            assert(GetConsoleCursorPosition(hConsoleOutput) == Add(newposstart, (SHORT)*i, size.X));
#if _DEBUG
            std::vector<WCHAR> buf(line.length() + 1);
            DWORD read = 0;
            ReadConsoleOutputCharacterW(hConsoleOutput, buf.data(), (DWORD)buf.size(), newposstart, &read);
            buf[line.length()] = L'\0';
            assert(std::wstring_view(line.begin(), line.length()).compare(buf.data()) == 0);
#endif
        }
        else
        {
            SHORT below = (SHORT)(line.length() / size.X - *i / size.X + 1);
            DisplayPotentials(hConsoleOutput, list, size, below, extra);
        }
    }
}

size_t Complete(const HANDLE hConsoleOutput, wchar_t* pStr, size_t nSize, size_t nNumberOfCharsRead, size_t i, Extra* extra, const COORD size)
{
    bufstring line(pStr, nSize, nNumberOfCharsRead);
    Complete(hConsoleOutput, line, &i, extra, size);

    // Leave cursor at end of line, pOrigReadConsoleW has no way of starting with the cursor in the middle
    if (line.length() != i)
    {
        COORD pos = GetConsoleCursorPosition(hConsoleOutput);
        pos = Add(pos, (SHORT) (line.length() - i), size.X);
        SetConsoleCursorPosition(hConsoleOutput, pos);
    }

    return line.length();
}

void CleanUpExtra(const HANDLE hConsoleOutput, Extra* extra)
{
    if (extra->length != 0)
    {
        CONSOLE_SCREEN_BUFFER_INFO csbi = {};
        GetConsoleScreenBufferInfo(hConsoleOutput, &csbi);

        COORD pos = { 0, extra->line };
        DWORD written = 0;
        FillConsoleOutputCharacter(hConsoleOutput, L' ', extra->length * csbi.dwSize.X, pos, &written);

        extra->length = 0;
    }
}

void ExpandAlias(bufstring& line)
{
    std::vector<string_range> params = findParam(line);
    if (params.empty())
        return;

    // TODO Repeat for all alias ie use isFirstCommand

    std::vector<string_range>::iterator pb = params.begin();
    std::wstring ae = getAlias(std::wstring(pb->substr(line)));
    if (!ae.empty())
    {
        std::vector<string_range>::iterator pe = pb;
        if (pe != params.end())
        {
            while ((pe + 1) != params.end() && !isCommandSeparator((pe + 1)->substr(line)))
                ++pe;
        }

        // TODO How to handle $T - ReadConsoleW splits it and returns the rest in the next call
        std::size_t i = 0;
        while ((i = ae.find(L'$', i)) != std::wstring::npos)
        {
            ++i;
            if (i < ae.length())
            {
                wchar_t c = ae[i];
                switch (c)
                {
                case L'B': case L'b':
                    ae.replace(i - 1, 2, L"|");
                    break;
                case L'G': case L'g':
                    ae.replace(i - 1, 2, L">");
                    break;
                case L'L': case L'l':
                    ae.replace(i - 1, 2, L"<");
                    break;
                case L'1': case L'2': case L'3': case L'4': case L'5':
                case L'6': case L'7': case L'8': case L'9':
                    {
                        int j = c - L'0';
                        std::wstring_view s;
                        if (std::distance(params.begin(), pb) + j <= std::distance(params.begin(), pe))
                            s = (pb + j)->substr(line);
                        ae.replace(i - 1, 2, s);
                        i += s.length() - 1;
                    }
                    break;
                case L'*':
                    {
                        std::wstring_view s;
                        if ((pb + 1) <= pe)
                            s = substr(line, (pb + 1)->begin, pe->end);
                        ae.replace(i - 1, 2, s);
                        i += s.length() - 1;

                    }
                    break;
                default:
                    ++i;
                    break;
                }
            }
            else
                break;
        }

        line.replace(pb->begin, pe->length(), ae);
    }
}
