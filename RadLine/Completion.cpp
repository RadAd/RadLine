#include "Completion.h"
#include "string_view.h"

#undef min
#undef max

#include <vector>
#include <algorithm>
#include <cctype>

#include "bufstring.h"
#include "WinHelpers.h"

namespace {
    std::wstring str(nonstd::wstring_view s)
    {
        return std::wstring(s.begin(), s.end());
    }

    size_t findEnvBegin(nonstd::wstring_view s)
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

    bool isCommandSeparator(nonstd::wstring_view s)
    {
        return s.compare(L"&") == 0
            || s.compare(L"|") == 0
            || s.compare(L"&&") == 0
            || s.compare(L"||") == 0;
    }

    bool isFirstCommand(const std::vector<nonstd::wstring_view>& params, std::vector<nonstd::wstring_view>::const_iterator p)
    {
        return p == params.begin() || isCommandSeparator(*(p - 1));
    }

    std::vector<nonstd::wstring_view>::const_iterator getFirstCommand(const std::vector<nonstd::wstring_view>& params, std::vector<nonstd::wstring_view>::const_iterator p)
    {
        std::vector<nonstd::wstring_view>::const_iterator f = p;
        while (f != params.begin() && !isCommandSeparator(*(f - 1)))
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

    std::vector<nonstd::wstring_view> findParam(nonstd::wstring_view line)
    {
        std::vector<nonstd::wstring_view> rs;
        nonstd::wstring_view::const_iterator begin = line.begin();
        while (begin < line.end())
        {
            while (begin < line.end() && isWhiteSpace(*begin))
                ++begin;
            if (begin < line.end())
            {
                nonstd::wstring_view::const_iterator end = begin;
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
                rs.push_back(nonstd::wstring_view(begin, end - begin));
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

    std::vector<std::wstring> findPotential(const std::vector<nonstd::wstring_view>& params, std::vector<nonstd::wstring_view>::const_iterator p, const std::wstring& s, std::size_t* i)
    {
        // TODO Handle quotes already on params

        std::vector<std::wstring> all;

        size_t envBegin = findEnvBegin(s);
        if (envBegin != std::wstring::npos)
        {
            *i = envBegin;
            append(all, findEnv(s.substr(envBegin + 1)));
        }
        else if (isFirstCommand(params, p))
        {
            if (s.find('\\') == std::wstring::npos)
            {
                *i = 0;
                filter(all, s, internal_cmds);
                append(all, findPath(s));
                append(all, findAlias(s));
            }
            else
            {
                const wchar_t* pName = PathFindName(s[0] == '"' ? s.c_str() + 1 : s.c_str());
                *i = pName - s.c_str();
                append(all, findExeFiles(s));
            }
            //append(all, findFiles(s + L"*", false));
        }
        else
        {
            const std::vector<nonstd::wstring_view>::const_iterator f = getFirstCommand(params, p);

            const wchar_t* pName = PathFindName(s[0] == '"' ? s.c_str() + 1 : s.c_str());
            *i = pName - s.c_str();

            // TODO copy and adjust *f to point to file name

            if (f->compare(L"alias") == 0 || f->compare(L"alias.bat") == 0)
            {
                if (std::distance(f, p) == 1)
                    append(all, findAlias(s));
                else
                    append(all, findFiles(s + L"*", FindFilesE::All));
            }
            else if (f->compare(L"where") == 0 || f->compare(L"where.exe") == 0)
            {
                if (std::distance(f, p) == 1)   // TODO Skip over options
                    append(all, findPath(s));
                else
                    append(all, findFiles(s + L"*", FindFilesE::All));
            }
            else if (f->compare(L"git") == 0 || f->compare(L"git.exe") == 0)
            {
                if (std::distance(f, p) == 1)   // TODO Skip over options
                    filter(all, s, git_cmds);
                else
                    append(all, findFiles(s + L"*", FindFilesE::All));
            }
            else if (f->compare(L"reg") == 0 || f->compare(L"reg.exe") == 0)
            {
                if (std::distance(f, p) == 1)
                    filter(all, s, reg_cmds);
                else
                    append(all, findFiles(s + L"*", FindFilesE::All));
            }
            else
            {
                FindFilesE filter = FindFilesE::All;
                for (const wchar_t* w : dir_only)
                {
                    if (f->compare(w) == 0)
                    {
                        filter = FindFilesE::DirOnly;
                        break;
                    }
                }

                append(all, findFiles(s + L"*", filter));
            }
        }

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

void Complete(const HANDLE hConsoleOutput, bufstring& line, size_t* i, Extra* extra, const COORD size)
{
    std::vector<nonstd::wstring_view> params = findParam(nonstd::wstring_view(line.begin(), line.length()));

    std::vector<nonstd::wstring_view>::const_iterator p = params.begin();
    while (p != params.end() && (line.begin() + *i) > p->end())
        ++p;

    const std::wstring substr = p != params.end() && (line.begin() + *i) >= p->begin()
        //? line.substr(p->begin() - line.begin(), std::min(const_cast<const wchar_t*>(line.begin() + *i), p->end()) - p->begin())
        ? str(nonstd::wstring_view(p->begin(), std::min(p->begin() - line.begin() + *i, p->length())))
        : std::wstring();

    std::size_t rp = 0;
    const std::vector<std::wstring> list = findPotential(params, p, substr, &rp);

    const COORD posstart = Add(GetConsoleCursorPosition(hConsoleOutput), - (SHORT) *i, size.X);

    if (!list.empty())
    {
        const std::wstring match = getLongestMatch(list);
        const wchar_t* refresh = line.end();

        if (match.length() > (substr.length() - rp) || list.size() == 1)
        {
            nonstd::wstring_view r = p != params.end() ? *p : nonstd::wstring_view(line.end(), 0);

            // TODO fix up handling inserting quotes
            bool openquote = r.begin() < line.end() && r.front() != L'"' && match.find(L' ') != std::wstring::npos;
            if (openquote)
            {
                line.insert(r.begin(), L'"');
                refresh = std::min(refresh, r.begin());
                r = nonstd::wstring_view(r.begin() + 1, r.length());
                ++*i;
            }

            if (substr.compare(rp, std::wstring::npos, match) != 0)
            {
                // TODO Make sure replace doesn't go over nSize
                nonstd::wstring_view nr = r;
                nr.remove_prefix(rp);
                line.replace(nr.begin(), nr.length(), match);
                refresh = std::min(refresh, nr.begin());
                *i -= nr.length();
                *i += match.length();
                r = nonstd::wstring_view(r.begin(), rp + match.length());
            }

            bool closequote = (openquote || r.front() == L'"') && r.back() != L'"';
            if (closequote)
            {
                line.insert(r.end(), L'\"');
                refresh = std::min(refresh, r.end());
                r = nonstd::wstring_view(r.begin(), r.length() + 1);
                ++*i;
            }

            if (list.size() == 1 && match.back() != L'\\' && *i == line.length())
            {
                refresh = std::min(refresh, const_cast<const wchar_t*>(line.end()));
                line += L' ';
                ++*i;
            }

            {
                const COORD pos = Add(posstart, (SHORT) (refresh - line.begin()), size.X);
                SetConsoleCursorPosition(hConsoleOutput, pos);
                EasyWriteString(hConsoleOutput, refresh);
                // TODO This could cause the screen to scroll shifting posstart
            }
            const int scroll = std::max(Add(posstart, (SHORT) line.length(), size.X).Y - size.Y + 1, 0);
            const COORD newposstart = Add(posstart, (SHORT) -scroll * size.X, size.X);
            {
                const COORD pos = Add(newposstart, (SHORT) *i, size.X);
                SetConsoleCursorPosition(hConsoleOutput, pos);
            }

            assert(GetConsoleCursorPosition(hConsoleOutput) == Add(newposstart, (SHORT) *i, size.X));
#if _DEBUG
            std::vector<WCHAR> buf(line.length() + 1);
            DWORD read = 0;
            ReadConsoleOutputCharacterW(hConsoleOutput, buf.data(), (DWORD) buf.size(), newposstart, &read);
            buf[line.length()] = L'\0';
            assert(nonstd::wstring_view(line.begin(), line.length()).compare(buf.data()) == 0);
#endif
        }
        else
        {
            SHORT below = (SHORT) (line.length() / size.X - *i / size.X + 1);
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
    std::vector<nonstd::wstring_view> params = findParam(nonstd::wstring_view(line.begin(), line.length()));
    if (params.empty())
        return;

    // TODO Repeat for all alias ie use isFirstCommand

    std::vector<nonstd::wstring_view>::iterator pb = params.begin();
    std::wstring ae = getAlias(str(*pb));
    if (!ae.empty())
    {
        std::vector<nonstd::wstring_view>::iterator pe = pb;
        if (pe != params.end())
        {
            while ((pe + 1) != params.end() && !isCommandSeparator(*(pe + 1)))
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
                            s = str(*(pb + j));
                        ae.replace(i - 1, 2, s);
                        i += s.length() - 1;
                    }
                    break;
                case L'*':
                    {
                        std::wstring s;
                        if ((pb + 1) <= pe)
                            s = str(nonstd::wstring_view((pb + 1)->begin(), pe->end() - (pb + 1)->begin()));
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

        line.replace(pb->begin(), pe->length(), ae);
    }
}
