#pragma once
#include "RadReadConsole.h"

#include "Completion.h"
#include "WinHelpers.h"
#include "Debug.h"
#include "bufstring.h"

#include "..\RadReadConsole\RadReadConsole.h"

#include <wchar.h>

WCHAR* FindMaskedChar(WCHAR* s, DWORD nSize, ULONG uMask)
{
    for (DWORD i = 0; i < nSize; ++i)
    {
        if (s[i] <= 0x1F && (uMask & (1 << s[i])))
            return s + i;
    }
    return nullptr;
}

extern "C" {
    decltype(&ReadConsoleW) pOrigReadConsoleW = nullptr;

    // Designed to replace ReadConsoleW
    __declspec(dllexport)
        BOOL WINAPI RadLineReadConsoleW(
            _In_     HANDLE  hConsoleInput,
            _Out_    LPVOID  lpBuffer,
            _In_     DWORD   nNumberOfCharsToRead,
            _Out_    LPDWORD lpNumberOfCharsRead,
            _In_opt_ PCONSOLE_READCONSOLE_CONTROL pInputControl
        )
    {
        DebugOut(TEXT("RadLine RadLineReadConsoleW 0x%08p 0x%p %d 0x%p 0x%p\n"), hConsoleInput, lpBuffer, nNumberOfCharsToRead, lpNumberOfCharsRead, pInputControl);
        if (lpNumberOfCharsRead != nullptr)
            DebugOut(TEXT("RadLine   NumberOfCharsRead %u\n"), *lpNumberOfCharsRead);
        if (pInputControl != nullptr)
            DebugOut(TEXT("RadLine   PCONSOLE_READCONSOLE_CONTROL %d %d 0x%08x 0x%08x\n"), pInputControl->nLength, pInputControl->nInitialChars, pInputControl->dwCtrlWakeupMask, pInputControl->dwControlKeyState);

        const int enabled = GetEnvironmentInt(L"RADLINE", 1);

        const HANDLE hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);

        if (GetFileType(hConsoleInput) != FILE_TYPE_CHAR || GetFileType(hStdOutput) != FILE_TYPE_CHAR)
        {
            return pOrigReadConsoleW(hConsoleInput, lpBuffer, nNumberOfCharsToRead, lpNumberOfCharsRead, pInputControl);
        }
        else if (nNumberOfCharsToRead == 1)
        {
            DebugOut(TEXT("RadLine   RadLineReadConsoleW Orig\n"));

            static const WCHAR response[] = L"\n\rY";
            static int response_count = 0;

            if (response_count == 0)
            {
                if (GetEnvironmentInt(L"RADLINE_AUTO_TERMINATE_BATCH"))
                {
                    const CONSOLE_SCREEN_BUFFER_INFO csbi = GetConsoleScreenBufferInfo(hStdOutput);

                    std::vector<WCHAR> buf(27);

                    const COORD pos = Add(csbi.dwCursorPosition, -(SHORT) buf.size(), csbi.dwSize.X);

                    DWORD read = 0;
                    ReadConsoleOutputCharacterW(hStdOutput, buf.data(), (DWORD) buf.size(), pos, &read);

                    if (wcsncmp(buf.data(), L"Terminate batch job (Y/N)? ", buf.size()) == 0)
                        response_count = ARRAYSIZE(response) - 1;
                }
            }

            if (response_count > 0)
            {
                WCHAR* pStr = reinterpret_cast<TCHAR*>(lpBuffer);
                --response_count;
                pStr[0] = response[response_count];
                *lpNumberOfCharsRead = 1;

                DWORD written = 0;
                WriteConsoleW(hStdOutput, lpBuffer, 1, &written, nullptr);

                return TRUE;
            }
            else
                return pOrigReadConsoleW(hConsoleInput, lpBuffer, nNumberOfCharsToRead, lpNumberOfCharsRead, pInputControl);
        }
        else if (enabled >= 1
            && pInputControl != nullptr && pInputControl->dwCtrlWakeupMask != 0
            && lpNumberOfCharsRead != nullptr)
        {
            DebugOut(TEXT("RadLine   RadLineReadConsoleW 1\n"));

            bool ismore = false;
            {
                std::vector<WCHAR> buf(6);

                const CONSOLE_SCREEN_BUFFER_INFO csbi = GetConsoleScreenBufferInfo(hStdOutput);
                const COORD pos = Add(csbi.dwCursorPosition, -(SHORT) buf.size(), csbi.dwSize.X);

                DWORD read = 0;
                ReadConsoleOutputCharacterW(hStdOutput, buf.data(), (DWORD) buf.size(), pos, &read);

                if (wcsncmp(buf.data(), L"More? ", buf.size()) == 0)
                    ismore = true;
            }

            // Assume first character is the completion character
            unsigned long t = 0;
            _BitScanForward(&t, pInputControl->dwCtrlWakeupMask);
            const WCHAR c = (WCHAR) t;

            BOOL r = TRUE;
            BOOL complete = FALSE;
            Extra extra = {};

            CONSOLE_READCONSOLE_CONTROL LocalInputControl = *pInputControl;
            const struct {} pInputControl = {}; // Hide variable
            LocalInputControl.dwCtrlWakeupMask |= 1 << ('e' - 'a' + 1); // Ctrl+e

            while (TRUE)
            {
                if (enabled == 2)
                    r = RadReadConsole(hConsoleInput, lpBuffer, nNumberOfCharsToRead, lpNumberOfCharsRead, &LocalInputControl);
                else
                    r = pOrigReadConsoleW(hConsoleInput, lpBuffer, nNumberOfCharsToRead, lpNumberOfCharsRead, &LocalInputControl);

                CleanUpExtra(hStdOutput, &extra);

                if (!r)
                    break;

                WCHAR* const pStr = reinterpret_cast<TCHAR*>(lpBuffer);

                size_t CursorOffset = 0;
                while (CursorOffset < *lpNumberOfCharsRead)
                {
                    if (pStr[CursorOffset] < 32 && (1 << pStr[CursorOffset]) & LocalInputControl.dwCtrlWakeupMask)
                        break;
                    ++CursorOffset;
                }

                if (CursorOffset >= *lpNumberOfCharsRead)
                {
                    complete = TRUE;
                    break;
                }

                WCHAR* const pChar = pStr + CursorOffset;
                const WCHAR cChar = *pChar;

                // Looks like a bug but the tab character overwrites instead of inserts, but the length is still increased by one
                const CONSOLE_SCREEN_BUFFER_INFO csbi = GetConsoleScreenBufferInfo(hStdOutput);
                const COORD startpos = Add(csbi.dwCursorPosition, -(SHORT) CursorOffset, csbi.dwSize.X);
                DWORD read = 0;
                ReadConsoleOutputCharacter(hStdOutput, pChar, 1, csbi.dwCursorPosition, &read);
                --*lpNumberOfCharsRead;

                if (cChar == c)
                {
                    bufstring line(pStr, nNumberOfCharsToRead, *lpNumberOfCharsRead);
                    Complete(hStdOutput, line, &CursorOffset, &extra, csbi.dwSize);
                    LocalInputControl.nInitialChars = (ULONG) line.length();
                }
                else if (cChar == ('e' - 'a' + 1))
                {
                    wchar_t CurrentDirectory[MAX_PATH] = L"";
                    GetCurrentDirectoryW(CurrentDirectory);
                    SetEnvironmentVariableW(L"CD", CurrentDirectory);

                    pStr[*lpNumberOfCharsRead] = L'\0';
                    LocalInputControl.nInitialChars = ExpandEnvironmentStrings(pStr, nNumberOfCharsToRead) - 1;

                    SetEnvironmentVariableW(L"CD", nullptr);

                    if (CursorOffset != 0)
                        SetConsoleCursorPosition(hStdOutput, startpos);

                    WriteConsole(hStdOutput, pStr, LocalInputControl.nInitialChars, nullptr, 0);
                    CursorOffset = LocalInputControl.nInitialChars;
                }
                else
                {
                    ++*lpNumberOfCharsRead;
                    LocalInputControl.nInitialChars = *lpNumberOfCharsRead;
                    *pChar = cChar;
                    break;
                }

                // Leave cursor at end of line, pOrigReadConsoleW has no way of starting with the cursor in the middle
                if (LocalInputControl.nInitialChars != CursorOffset)
                {
                    const COORD pos = Add(startpos, (SHORT) LocalInputControl.nInitialChars, csbi.dwSize.X);
                    SetConsoleCursorPosition(hStdOutput, pos);
                }
                assert(GetConsoleCursorPosition(hStdOutput) == Add(startpos, (SHORT) LocalInputControl.nInitialChars, csbi.dwSize.X));

                if (LocalInputControl.nInitialChars != CursorOffset)
                {
                    // Insert left key into input queue to return cursor to correct position
                    const HANDLE hStdInput = GetStdHandle(STD_INPUT_HANDLE);

                    INPUT_RECORD ir[2] = {};
                    ir[0].EventType = { KEY_EVENT };
                    ir[0].Event.KeyEvent.bKeyDown = TRUE;
                    ir[0].Event.KeyEvent.wVirtualKeyCode = VK_LEFT;
                    ir[1].EventType = { KEY_EVENT };
                    ir[1].Event.KeyEvent.bKeyDown = FALSE;
                    ir[1].Event.KeyEvent.wVirtualKeyCode = VK_LEFT;

                    DWORD written = 0;
                    for (size_t i = CursorOffset; i < LocalInputControl.nInitialChars; ++i)
                        WriteConsoleInput(hStdInput, ir, 2, &written);
                }
            }

            // nNumberOfCharsToRead == 1023 when used for "set /p"
            if (complete && nNumberOfCharsToRead != 1023 && *lpNumberOfCharsRead > 2 && GetEnvironmentInt(L"RADLINE_TILDE", 1))
            {
                const wchar_t replace[] = L"%USERPROFILE%";
                TCHAR* start = reinterpret_cast<TCHAR*>(lpBuffer);
                TCHAR* found;
                while ((found = wcschr(start, L'~')) != nullptr
                    && static_cast<DWORD>(found - reinterpret_cast<TCHAR*>(lpBuffer)) < *lpNumberOfCharsRead)
                {
                    const ptrdiff_t i = found - reinterpret_cast<TCHAR*>(lpBuffer);
                    if ((i == 0 || (i >= 1 && wcschr(L" =", found[-1]) != nullptr) || (i >= 2 && found[-1] == L'\"' && wcschr(L" =", found[-2]) != nullptr))
                        && wcschr(L"\r\\ \"", found[1]) != nullptr)
                    {
                        bufstring cmd(reinterpret_cast<TCHAR*>(lpBuffer), nNumberOfCharsToRead, *lpNumberOfCharsRead);
                        cmd.replace(found, 1, replace);
                        *lpNumberOfCharsRead = static_cast<DWORD>(cmd.length());
                        start = found + ARRAYSIZE(replace) - 1;
                    }
                    else
                        start = found + 1;
                }
            }

            // nNumberOfCharsToRead == 1023 when used for "set /p"
            if (complete && nNumberOfCharsToRead != 1023 && *lpNumberOfCharsRead > 2 && !ismore)
            {
                wchar_t pre[100] = L"";
                GetEnvironmentVariableW(L"RADLINE_PRE", pre);
                wchar_t post[100] = L"";
                GetEnvironmentVariableW(L"RADLINE_POST", post);
                if ((pre[0] != TEXT('\0') || post[0] != TEXT('\0')) && *reinterpret_cast<TCHAR*>(lpBuffer) != L' ' && *reinterpret_cast<TCHAR*>(lpBuffer) != L'(')
                {
                    bufstring cmd(reinterpret_cast<TCHAR*>(lpBuffer), nNumberOfCharsToRead, *lpNumberOfCharsRead);

#if 0
                    for (const wchar_t* w = cmd.begin(); w != cmd.end(); ++w)
                    {
                        if (*w == L')' && w[-1] != L'^')
                        {
                            cmd.insert(w, L'^');
                            w++;
                        }
                    }
#endif

                    if (pre[0] != TEXT('\0'))
                    {
                        const wchar_t* w = cmd.begin();
                        cmd.insert(w, L")&(");
                        cmd.insert(w, pre);
                        cmd.insert(w, L"(");
                    }
                    else
                        cmd.insert(cmd.begin(), L"(");
                    if (post[0] != TEXT('\0'))
                    {
                        const wchar_t* w = cmd.end() - 2;
                        cmd.insert(w, L")");
                        cmd.insert(w, post);
                        cmd.insert(w, L")&(");
                    }
                    else
                        cmd.insert(cmd.end() - 2, L")");
                    *lpNumberOfCharsRead = static_cast<DWORD>(cmd.length());
                }
            }

            return r;
        }
        else
        {
            DebugOut(TEXT("RadLine   RadLineReadConsoleW Orig\n"));
            return pOrigReadConsoleW(hConsoleInput, lpBuffer, nNumberOfCharsToRead, lpNumberOfCharsRead, pInputControl);
        }
    }
}
