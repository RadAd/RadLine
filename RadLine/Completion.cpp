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

        auto length() const
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
    };

    std::wstring str(std::wstring_view s)
    {
        return std::wstring(s);
    }

    std::wstring_view unquote(std::wstring_view s)
    {
        if (s.size() >= 2 && s.front() == L'"' && s.back() == L'"')
        {
            s.remove_prefix(1);
            s.remove_suffix(1);
        }
        return s;
    }

    size_t findEnvBegin(std::wstring_view s)
    {
        bool in = false;
        for (size_t i = 0; i < s.length(); ++i)
        {
            if (s[i] == L'%')
                in = !in;
        }
        if (in)
            return s.rfind('%');
        else
            return std::wstring::npos;
    }

    bool isCommandSeparator(std::wstring_view s)
    {
        return s.compare(L"&") == 0
            || s.compare(L"|") == 0
            || s.compare(L"&&") == 0
            || s.compare(L"||") == 0;
    }

    bool isFirstCommand(const bufstring& line, const std::vector<string_range>& params, std::vector<string_range>::const_iterator p)
    {
        return p == params.begin() || isCommandSeparator((p - 1)->substr(line));
    }

    std::vector<string_range>::const_iterator getFirstCommand(const bufstring& line, const std::vector<string_range>& params, std::vector<string_range>::const_iterator p)
    {
        std::vector<string_range>::const_iterator f = p;
        while (f != params.begin() && !isCommandSeparator((f - 1)->substr(line)))
            --f;
        return f;
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

    const WCHAR* internal_cmds[] = {
        L"assoc", L"call", L"cd", L"chdir", L"cls", L"color", L"copy", L"date", L"del", L"dir", L"echo", L"endlocal", L"erase", L"exit", L"for",
        L"ftype", L"goto", L"if", L"md", L"mkdir", L"mklink", L"move", L"path", L"popd", L"prompt", L"pushd", L"rem", L"ren", L"rd", L"rmdir",
        L"set", L"setlocal", L"shift", L"start", L"time", L"title", L"type", L"ver", L"verify", L"vol"
    };

    const WCHAR* dir_only[] = {
        L"cd", L"chdir", L"md", L"mkdir", L"pushd"
    };

    const WCHAR* git_cmds[] = {
        L"clone", L"init", L"add", L"mv", L"reset", L"rm", L"bisect", L"grep", L"log", L"show", L"status"
        L"branch", L"checkout", L"commit", L"diff", L"merge", L"rebase", L"tag", L"fetch", L"pull", L"push", L"help"
    };

    const WCHAR* reg_cmds[] = {
        L"query", L"add", L"delete", L"copy", L"save", L"load",
        L"unload", L"restore", L"compare", L"export", L"import", L"flags"
    };

    template <size_t size>
    inline void filter(std::vector<std::wstring>& all, const std::wstring& s, const WCHAR*(&words)[size])
    {
        for (const wchar_t* w : words)
        {
            if (Match(s, w))
                all.push_back(w);
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
            GetCurrentDirectoryA(ARRAYSIZE(FullNameS), FullNameS);
            lua_pushstring(lua, FullNameS);
        }
        else
        {
            char v[1024] = "";
            GetEnvironmentVariableA(s, v, ARRAYSIZE(v));
            lua_pushstring(lua, v);
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
        // TODO Handle quotes already on params
        std::vector<std::wstring> all;

#if 1
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
        char* const pFileName = PathFindName(strFile);
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
                    /*std::wstring*/ msg = LuaPopString(L.get());
                    //DebugOut(L"RadLine %s\n", msg.c_str());
                    //MessageBox(NULL, msg.c_str(), L"RadLine", MB_OK | MB_ICONERROR);
                    return all;
                }
            }
            assert(lua_gettop(L.get()) == 0);
        }

        //int fi = 0;
        //int di = 0;
        if (lua_getglobal(L.get(), "FindPotential") != LUA_TFUNCTION)
        {
            /*std::wstring*/ msg = L"FindPotential is not a function";
            //DebugOut(L"RadLine %s\n", msg.c_str());
            //MessageBox(NULL, msg.c_str(), L"RadLine", MB_OK | MB_ICONERROR);
            lua_pop(L.get(), 1);
        }
        else if (//fi = lua_absindex(L.get(), 0),
                LuaPush(L.get(), line, params),
                lua_pushinteger(L.get(), std::distance(params.begin(), p) + 1),
                lua_pushinteger(L.get(), s.length()),
                //di = lua_absindex(L.get(), 0),
                lua_pcall(L.get(), 3, 2, 0) != LUA_OK)
        {
            /*std::wstring*/ msg = LuaPopString(L.get());
            //DebugOut(L"pcall %s\n", msg.c_str());
            //MessageBox(NULL, msg.c_str(), L"RadLine", MB_OK | MB_ICONERROR);
        }
        else
        {
            *i = (size_t) (lua_isnil(L.get(), -1) ? (lua_pop(L.get(), 1), 0) : LuaPopInteger(L.get()) - 1);
            std::vector<std::wstring> vs = LuaPopVectorOfStrings(L.get());
            append(all, vs);
        }
        assert(lua_gettop(L.get()) == 0);
#else
        size_t envBegin = findEnvBegin(s);
        if (envBegin != std::wstring::npos)
        {
            *i = envBegin;
            append(all, findEnv(s.substr(envBegin + 1)));
        }
        else if (isFirstCommand(line, params, p))
        {
            const wchar_t* pName = PathFindName(s[0] == '"' ? s.c_str() + 1 : s.c_str());
            *i = pName - s.c_str();

            if (*i == 0)
            {
                filter(all, s, internal_cmds);
                append(all, findPath(s));
                append(all, findAlias(s));
            }
            else
            {
                append(all, findExeFiles(s));
            }
        }
        else
        {
            const std::vector<string_range>::const_iterator f = getFirstCommand(line, params, p);
            const std::wstring sfirst = str(unquote(f->substr(line)));
            const wchar_t* pFirstName = PathFindName(sfirst.c_str());
            const std::wstring_view first(pFirstName);

            const wchar_t* pName = PathFindName(s[0] == '"' ? s.c_str() + 1 : s.c_str());
            *i = pName - s.c_str();

            // TODO copy and adjust *f to point to file name

            if (p > f && (p - 1)->substr(line) == L">" || (p - 1)->substr(line) == L"<")
            {
                append(all, findFiles(s + L"*", FindFilesE::All));
            }
            else if (first.compare(L"alias") == 0 || first.compare(L"alias.bat") == 0)
            {
                if (std::distance(f, p) == 1)
                    append(all, findAlias(s));
                else
                    append(all, findFiles(s + L"*", FindFilesE::All));
            }
            else if (first.compare(L"where") == 0 || first.compare(L"where.exe") == 0)
            {
                if (std::distance(f, p) == 1)   // TODO Skip over options
                    append(all, findPath(s));
                else
                    append(all, findFiles(s + L"*", FindFilesE::All));
            }
            else if (first.compare(L"git") == 0 || first.compare(L"git.exe") == 0)
            {
                if (std::distance(f, p) == 1)   // TODO Skip over options
                    filter(all, s, git_cmds);
                else
                    append(all, findFiles(s + L"*", FindFilesE::All));
            }
            else if (first.compare(L"reg") == 0 || first.compare(L"reg.exe") == 0)
            {
                if (std::distance(f, p) == 1)
                    filter(all, s, reg_cmds);
                else if (std::distance(f, p) == 2)
                    append(all, findRegKey(s));
                else
                    append(all, findFiles(s + L"*", FindFilesE::All));
            }
            else
            {
                FindFilesE filter = FindFilesE::All;
                for (const wchar_t* w : dir_only)
                {
                    if (first.compare(w) == 0)
                    {
                        filter = FindFilesE::DirOnly;
                        break;
                    }
                }

                append(all, findFiles(s + L"*", filter));
            }
        }
#endif

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
        i = msg.find(L'\n', i);
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
        //? line.substr(p->begin() - line.begin(), std::min(const_cast<const wchar_t*>(line.begin() + *i), p->end()) - p->begin())
        //? str(std::wstring_view(p->begin, std::min(*i - (p->begin - line.begin()), p->length())))
        ? str(::substr(line, p->begin, std::min(line.begin() + *i, p->end)))
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

            // TODO fix up handling inserting quotes
            bool openquote = r.begin < line.end() && r.front() != L'"' && match.find(L' ') != std::wstring::npos;
            if (openquote)
            {
                line.insert(r.begin, L'"');
                refresh = std::min(refresh, r.begin);
                ++r.begin;
                ++* i;
            }

            if (substr.compare(rp, std::wstring::npos, match) != 0)
            {
                // TODO Make sure replace doesn't go over nSize
                string_range nr = r;
                nr.begin += rp;
                line.replace(nr.begin, nr.length(), match);
                refresh = std::min(refresh, nr.begin);
                *i -= nr.length();
                *i += match.length();
                r.end = nr.begin + match.length();
            }

            bool closequote = (openquote || r.front() == L'"') && r.back() != L'"';
            if (closequote)
            {
                line.insert(r.end, L'\"');
                refresh = std::min(refresh, r.end);
                ++r.end;
                ++* i;
            }

            if (list.size() == 1 && match.back() != L'\\' && *i == line.length())
            {
                refresh = std::min(refresh, line.end());
                line += L' ';
                ++* i;
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
    std::wstring ae = getAlias(str(pb->substr(line)));
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
                        std::wstring s;
                        if (std::distance(params.begin(), pb) + j <= std::distance(params.begin(), pe))
                            s = str((pb + j)->substr(line));
                        ae.replace(i - 1, 2, s);
                        i += s.length() - 1;
                    }
                    break;
                case L'*':
                    {
                        std::wstring s;
                        if ((pb + 1) <= pe)
                            s = str(substr(line, (pb + 1)->begin, pe->end));
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
