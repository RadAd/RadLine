#include "Completion.h"

#undef min
#undef max

#include <vector>
#include <algorithm>
#include <cctype>

#include "bufstring.h"
#include "WinHelpers.h"

namespace {

    struct range
    {
        std::size_t begin, end;

        std::size_t length() const
        {
            return end - begin;
        }
    };

    inline std::wstring substr(const bufstring& s, range r)
    {
        return s.substr(r.begin, r.end - r.begin);
    }

    inline int compare(const bufstring& s, range r, const wchar_t* cmp)
    {
        return s.compare(r.begin, r.end - r.begin, cmp);
    }

    size_t findEnvBegin(const std::wstring& s)
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

    bool isCommandSeparator(const bufstring& line, range r)
    {
        return compare(line, r, L"&") == 0
            || compare(line, r, L"|") == 0
            || compare(line, r, L"&&") == 0
            || compare(line, r, L"||") == 0;
    }

    bool isFirstCommand(const bufstring& line, const std::vector<range>& params, std::vector<range>::const_iterator p)
    {
        return p == params.begin() || isCommandSeparator(line, *(p - 1));
    }

    inline bool CharCaseInsensitiveEqual(wchar_t a, wchar_t b)
    {
        return std::toupper(a) == std::toupper(b);
    }

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

    std::vector<range> findParam(const bufstring& line)
    {
        std::vector<range> rs;
        range r;
        r.begin = 0;
        while (r.begin < line.length())
        {
            while (r.begin < line.length() && line[r.begin] == L' ')
                ++r.begin;
            if (r.begin < line.length())
            {
                if (line[r.begin] == L'\"')
                {
                    bool inquotes = true;
                    ++r.begin;
                    r.end = r.begin + 1;
                    while (r.end < line.length() && (inquotes || line[r.end] != L' '))
                    {
                        ++r.end;
                        if (r.end < line.length() && line[r.end] == L'\"')
                            inquotes = !inquotes;
                    }
                }
                else
                {
                    r.end = r.begin + 1;
                    while (r.end < line.length() && line[r.end] != L' ')
                        ++r.end;
                }
                rs.push_back(r);
                r.begin = r.end;
            }
        }
        return rs;
    }

    std::vector<std::wstring> findPotential(const bufstring& line, const std::vector<range>& params, std::vector<range>::const_iterator p, const std::wstring& s, std::size_t* i)
    {
        // TODO Handle quotes already on params

        std::vector<std::wstring> all;
        if (s.find('\\') == std::wstring::npos && isFirstCommand(line, params, p))
        {
            *i = 0;
            // TODO append(all, findInternal(s));
            append(all, findPath(s));
            // TODO append(all, findAlias(s, i));
        }

        size_t envBegin = findEnvBegin(s);
        if (envBegin != std::wstring::npos)
        {
            *i = envBegin;
            append(all, findEnv(s.substr(envBegin)));
        }
        else
        {
            const wchar_t* pName = PathFindName(s.c_str());
            *i = pName - s.c_str();
            append(all, findFiles(s));
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
    // TODO handle spaces

    std::vector<range> params = findParam(line);

    std::vector<range>::const_iterator p = params.begin();
    while (p != params.end() && *i > p->end)
        ++p;
    if (p == params.end())
        return;

    const std::wstring substr = line.substr(p->begin, *i - p->begin);

    std::size_t rp = 0;
    const std::vector<std::wstring> list = findPotential(line, params, p, substr, &rp);

    const COORD posstart = Add(GetConsoleCursorPosition(hConsoleOutput), -(SHORT) *i, size.X);

    if (!list.empty())
    {
        const std::wstring match = getLongestMatch(list);

        if (match.length() > (substr.length() - rp) || list.size() == 1)
        {
            range r = *p;

            bool openquote = (r.begin == 0 || line[r.begin - 1] != L'"') && match.find(L' ') != std::wstring::npos;
            if (openquote)
            {
                line.insert(r.begin, L'"');
                ++r.begin;
                ++r.end;
                ++*i;
            }

            bool closequote = (r.begin > 0 && line[r.begin - 1] == L'"') && (r.end >= line.length() || line[r.end] != L'"');
            if (closequote)
            {
                line.insert(r.end, L'\"');
            }

            if (substr.compare(rp, std::wstring::npos, match) != 0)
            {
                // TODO Make sure replace doesn't go over nSize
                line.replace(r.begin + rp, r.length() - rp, match);

                COORD pos = GetConsoleCursorPosition(hConsoleOutput);
                if (openquote)
                {
                    pos = Add(pos, (SHORT) r.begin - (SHORT) *i, size.X);
                    assert(pos == Add(posstart, (SHORT) r.begin - 1, size.X));
                    SetConsoleCursorPosition(hConsoleOutput, pos);
                    EasyWriteString(hConsoleOutput, line.begin() + r.begin - 1);
                }
                else
                {
                    pos = Add(pos, (SHORT) (r.begin + rp) - (SHORT) *i, size.X);
                    assert(pos == Add(posstart, (SHORT) (r.begin + rp), size.X));
                    SetConsoleCursorPosition(hConsoleOutput, pos);
                    EasyWriteString(hConsoleOutput, line.begin() + r.begin + rp);
                }

                *i = r.begin + rp + match.length();

                if (line.length() > *i)
                {
                    pos = GetConsoleCursorPosition(hConsoleOutput);
                    pos = Add(pos, (SHORT) *i - (SHORT) line.length(), size.X);
                    SetConsoleCursorPosition(hConsoleOutput, pos);
                }

                assert(GetConsoleCursorPosition(hConsoleOutput) == Add(posstart, (SHORT) *i, size.X));
            }
            // TODO else if openquote || close quote --- need to redraw the changes

            if (list.size() == 1 && match.back() != L'\\' && *i == line.length())
            {
                line += L' ';
                EasyWriteChar(hConsoleOutput, L' ');
                ++*i;
            }

            assert(GetConsoleCursorPosition(hConsoleOutput) == Add(posstart, (SHORT) *i, size.X));
#if _DEBUG
            std::vector<WCHAR> buf(line.length());
            DWORD read = 0;
            ReadConsoleOutputCharacterW(hConsoleOutput, buf.data(), (DWORD) buf.size(), posstart, &read);
            assert(line.compare(0, line.length(), buf.data()) == 0);
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
        FillConsoleOutputCharacter(hConsoleOutput, L' ', extra->length * csbi.dwSize.Y, pos, &written);

        extra->length = 0;
    }
}

void ExpandAlias(bufstring& line)
{
    std::vector<range> params = findParam(line);
    if (params.empty())
        return;

    // TODO Repeat for all alias ie use isFirstCommand

    std::vector<range>::iterator pb = params.begin();
    std::wstring ae = getAlias(substr(line, *pb));
    if (!ae.empty())
    {
        std::vector<range>::iterator pe = pb;
        if (pe != params.end())
        {
            while ((pe + 1) != params.end() && !isCommandSeparator(line, *(pe + 1)))
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
                            s = substr(line, *(pb + j));
                        ae.replace(i - 1, 2, s);
                        i += s.length() - 1;
                    }
                    break;
                case L'*':
                    {
                        std::wstring s;
                        if ((pb + 1) <= pe)
                            s = line.substr((pb + 1)->begin, pe->end - (pb + 1)->begin);
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

        line.replace(pb->begin, pe->end, ae);
    }
}
