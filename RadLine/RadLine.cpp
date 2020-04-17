#include "RadLine.h"

#include <string>
#include <vector>
#include <assert.h>

#include "WinHelpers.h"
#include "Completion.h"
#include "bufstring.h"

KEY_EVENT_RECORD EasyReadInput(const HANDLE hStdInput)
{
    INPUT_RECORD ir = {};
    while (ir.EventType != KEY_EVENT || !ir.Event.KeyEvent.bKeyDown)
    {
        DWORD read;
        ReadConsoleInputW(hStdInput, &ir, 1, &read);
    }
    return ir.Event.KeyEvent;
}

wchar_t WordBreak[] = L" \\/";

std::size_t moveWordLeft(const bufstring& line, std::size_t i)
{
    assert(i >= 0);
    while (i > 0 && line[i - 1] == L' ')
        --i;
    if (i < line.length() && wcschr(WordBreak, line[i]) == nullptr)
    {
        while (i > 0 && wcschr(WordBreak, line[i - 1]) != nullptr)
            --i;
    }
    else
    {
        while (i > 0 && wcschr(WordBreak, line[i - 1]) == nullptr)
            --i;
    }
    return i;
}

std::size_t moveWordRight(const bufstring& line, std::size_t i)
{
    assert(i >= 0);
    while (i < line.length() && wcschr(WordBreak, line[i]) == nullptr)
        ++i;
    while (i < line.length() && wcschr(WordBreak, line[i]) != nullptr)
        ++i;
    return i;
}

static std::vector<std::wstring> history;
// TODO Limit history

size_t RadLine(const HANDLE hStdInput, const HANDLE hStdOutput, wchar_t* lpBuffer, size_t nSize)
{
    // TODO
    // Insert mode
    // Paste
    // Selection + Copy
    // Ctrl characters displaying two character output ie Ctrl+G

    COORD start = GetConsoleCursorPosition(hStdOutput);
    Extra extra = {};

    std::vector<std::wstring>::const_iterator hit = history.end();

    bool exit = false;
    bufstring line(lpBuffer, nSize);
    std::size_t i = 0;
    while (!exit)
    {
        CONSOLE_SCREEN_BUFFER_INFO csbi = {};
        GetConsoleScreenBufferInfo(hStdOutput, &csbi);
        const COORD size = csbi.dwSize;

        const KEY_EVENT_RECORD ir = EasyReadInput(hStdInput);
        switch (ir.wVirtualKeyCode)
        {
        case VK_ESCAPE:
            i = 0;
            SetConsoleCursorPosition(hStdOutput, start);
            {
                DWORD written = 0;
                FillConsoleOutputCharacter(hStdOutput, L' ', (SHORT) line.length(), start, &written);
            }
            line.clear();
            break;

        case VK_BACK:
            if (i > 0)
            {
                --i;
                line.erase(i);

                csbi.dwCursorPosition = Add(csbi.dwCursorPosition, -1, size.X);
                SetConsoleCursorPosition(hStdOutput, csbi.dwCursorPosition);

                EasyWriteString(hStdOutput, line.c_str() + i);
                EasyWriteChar(hStdOutput, L' ');

                SetConsoleCursorPosition(hStdOutput, csbi.dwCursorPosition);
            }
            break;

        case VK_TAB:
            {
                CleanUpExtra(hStdOutput, &extra);
                assert(GetConsoleCursorPosition(hStdOutput) == Add(start, (SHORT) (int) i, size.X));
                Complete(hStdOutput, line, &i, &extra, size);
                assert(GetConsoleCursorPosition(hStdOutput) == Add(start, (SHORT) (int) i, size.X));

                // TODO Need to detect if screen has scrolled to adjust start
            }
            break;

#if 0
        case L'\x16':    // Ctrl+V
                        // TODO Get clipboard text
                        // paste into line or paste into keyboard - ie what happens with '\r' - passed on to next readline???
            break;
#endif

        case VK_RETURN:
            exit = true;
            break;

        case VK_LEFT:
            if (i > 0)
            {
                if (ir.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))
                {
                    size_t ni = moveWordLeft(line, i);
                    if (i != ni)
                    {
                        csbi.dwCursorPosition = Add(csbi.dwCursorPosition, (SHORT) (i - ni), size.X);
                        SetConsoleCursorPosition(hStdOutput, csbi.dwCursorPosition);
                        i = ni;
                    }
                }
                else
                {
                    --i;
                    csbi.dwCursorPosition = Add(csbi.dwCursorPosition, -1, size.X);
                    SetConsoleCursorPosition(hStdOutput, csbi.dwCursorPosition);
                }
            }
            break;

        case VK_RIGHT:
            if (i < line.length())
            {
                if (ir.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))
                {
                    std::size_t ni = moveWordRight(line, i);
                    if (i != ni)
                    {
                        csbi.dwCursorPosition = Add(csbi.dwCursorPosition, (SHORT) (ni - i), size.X);
                        SetConsoleCursorPosition(hStdOutput, csbi.dwCursorPosition);
                        i = ni;
                    }
                }
                else
                {
                    ++i;
                    csbi.dwCursorPosition = Add(csbi.dwCursorPosition, 1, size.X);
                    SetConsoleCursorPosition(hStdOutput, csbi.dwCursorPosition);
                }
            }
            break;

        case VK_UP:
            if (hit != history.begin())
            {
                --hit;
                line.clear();
                line += *hit;
                i = line.length();
                SetConsoleCursorPosition(hStdOutput, start);
                EasyWriteString(hStdOutput, line.begin());
            }
            break;

        case VK_DOWN:
            if ((hit + 1) != history.end())
            {
                ++hit;
                line.clear();
                line += *hit;
                i = line.length();
                SetConsoleCursorPosition(hStdOutput, start);
                EasyWriteString(hStdOutput, line.begin());
            }
            break;

        case VK_HOME:
            if (i > 0)
            {
                i = 0;
                SetConsoleCursorPosition(hStdOutput, start);
            }
            break;

        case VK_END:
            if (i < line.length())
            {
                i = line.length();
                SetConsoleCursorPosition(hStdOutput, Add(start, (SHORT) (int) i, size.X));
            }
            break;

        case VK_DELETE:
            if (i < line.length())
            {
                line.erase(i);

                const COORD pos = GetConsoleCursorPosition(hStdOutput);
                EasyWriteString(hStdOutput, line.c_str() + i);
                EasyWriteChar(hStdOutput, L' ');
                SetConsoleCursorPosition(hStdOutput, pos);
            }
            break;

        default:
            if (isprint(ir.uChar.UnicodeChar))
            {
                const WCHAR c = ir.uChar.UnicodeChar;

                line.insert(i, c);
                ++i;

                // TODO Need to detect if screen has scrolled to adjust start

                EasyWriteChar(hStdOutput, c);

                if (i < line.length())
                {
                    const COORD pos = GetConsoleCursorPosition(hStdOutput);
                    EasyWriteString(hStdOutput, line.c_str() + i);
                    SetConsoleCursorPosition(hStdOutput, pos);
                }
            }
            break;
        }
        assert(i >= 0);
        assert(i <= line.length());
        assert(GetConsoleCursorPosition(hStdOutput) == Add(start, (SHORT) (int) i, size.X));
    }

    history.push_back(line.str());

    ExpandAlias(line);

    line += L"\r\n";

    COORD pos = GetConsoleCursorPosition(hStdOutput);
    pos.X = 0;
    ++pos.Y;
    SetConsoleCursorPosition(hStdOutput, pos);

    CleanUpExtra(hStdOutput, &extra);

    return line.length();
}
