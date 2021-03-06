#pragma once
#include "RadReadConsole.h"

#include "Completion.h"
#include "RadLine.h"
#include "WinHelpers.h"
#include "Debug.h"
#include "bufstring.h"

#include <wchar.h>

extern "C" {
    decltype(&ReadConsoleW) pOrigReadConsoleW = nullptr;

    // Designed to replace ReadConsoleW
    __declspec(dllexport)
    BOOL WINAPI RadReadConsoleW(
        _In_     HANDLE  hConsoleInput,
        _Out_    LPVOID  lpBuffer,
        _In_     DWORD   nNumberOfCharsToRead,
        _Out_    LPDWORD lpNumberOfCharsRead,
        _In_opt_ PCONSOLE_READCONSOLE_CONTROL pInputControl
    )
    {
        DebugOut(TEXT("RadLine RadReadConsoleW 0x%08p 0x%p %d 0x%p 0x%p\n"), hConsoleInput, lpBuffer, nNumberOfCharsToRead, lpNumberOfCharsRead, pInputControl);
        if (pInputControl != nullptr)
            DebugOut(TEXT("RadLine PCONSOLE_READCONSOLE_CONTROL %d %d 0x%08x 0x%08x\n"), pInputControl->nLength, pInputControl->nInitialChars, pInputControl->dwCtrlWakeupMask, pInputControl->dwControlKeyState);

        wchar_t enabled[100] = L"";
        GetEnvironmentVariableW(L"RADLINE", enabled);

        const HANDLE hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);

        if (GetFileType(hConsoleInput) != FILE_TYPE_CHAR || GetFileType(hStdOutput) != FILE_TYPE_CHAR)
        {
            return pOrigReadConsoleW(hConsoleInput, lpBuffer, nNumberOfCharsToRead, lpNumberOfCharsRead, pInputControl);
        }
        else if (nNumberOfCharsToRead == 1)
        {
            DebugOut(TEXT("RadLine RadReadConsoleW Orig\n"));

            static const WCHAR response[] = L"\n\rY";
            static int response_count = 0;

            if (response_count == 0)
            {
                wchar_t terminate[100] = L"";
                GetEnvironmentVariableW(L"RADLINE_AUTO_TERMINATE_BATCH", terminate);
                if (_wcsicmp(terminate, L"1") == 0)
                {
                    CONSOLE_SCREEN_BUFFER_INFO csbi = {};
                    GetConsoleScreenBufferInfo(hStdOutput, &csbi);

                    std::vector<WCHAR> buf(27);

                    COORD pos = GetConsoleCursorPosition(hStdOutput);
                    pos = Add(pos, -(SHORT) buf.size(), csbi.dwSize.X);

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
        else if ((_wcsicmp(enabled, L"") == 0 || _wcsicmp(enabled, L"1") == 0)
            && pInputControl != nullptr && pInputControl->dwCtrlWakeupMask != 0
            && lpNumberOfCharsRead != nullptr)
        {
            DebugOut(TEXT("RadLine RadReadConsoleW 2\n"));

            unsigned long t = 0;
            _BitScanForward(&t, pInputControl->dwCtrlWakeupMask);
            const WCHAR c = (WCHAR) t;

            BOOL r = TRUE;
            BOOL repeat = TRUE;
            Extra extra = {};

            while (repeat)
            {
                r = pOrigReadConsoleW(hConsoleInput, lpBuffer, nNumberOfCharsToRead, lpNumberOfCharsRead, pInputControl);

                if (r)
                {
                    WCHAR* const pStr = reinterpret_cast<TCHAR*>(lpBuffer);
                    WCHAR* const pTab = wmemchr(pStr, c, *lpNumberOfCharsRead);
                    if (pTab != nullptr)
                    {
                        // Looks like a bug but the tab character overwrites instead of inserts, but the length is still increased by one
                        CONSOLE_SCREEN_BUFFER_INFO csbi = {};
                        GetConsoleScreenBufferInfo(hStdOutput, &csbi);
                        DWORD read = 0;
                        ReadConsoleOutputCharacter(hStdOutput, pTab, 1, csbi.dwCursorPosition, &read);
                        --*lpNumberOfCharsRead;

                        CleanUpExtra(hStdOutput, &extra);
                        pInputControl->nInitialChars = (ULONG) Complete(hStdOutput, pStr, nNumberOfCharsToRead, *lpNumberOfCharsRead, pTab - pStr, &extra, csbi.dwSize);
                    }
                    else
                        repeat = false;
                }
                else
                    repeat = false;
            }

            // nNumberOfCharsToRead == 1023 when used for "set /p"
            if (nNumberOfCharsToRead != 1023 && *lpNumberOfCharsRead > 2)
            {
                wchar_t post[100] = L"";
                GetEnvironmentVariableW(L"RADLINE_POST", post);
                if (post[0] != TEXT('\0'))
                {
                    bufstring cmd(reinterpret_cast<TCHAR*>(lpBuffer), nNumberOfCharsToRead, *lpNumberOfCharsRead);
                    const wchar_t* w = cmd.begin() + *lpNumberOfCharsRead - 2;
                    cmd.insert(w, post);
                    cmd.insert(w, L"& ");
                    *lpNumberOfCharsRead = static_cast<DWORD>(cmd.length());
                }
            }

            CleanUpExtra(hStdOutput, &extra);

            return r;
        }
        else if (_wcsicmp(enabled, L"2") == 0
            && (pInputControl == nullptr || pInputControl->nInitialChars == 0))
        {
            DebugOut(TEXT("RadLine RadReadConsoleW 1\n"));
            DWORD modeout = 0;
            GetConsoleMode(hStdOutput, &modeout);
            SetConsoleMode(hStdOutput, modeout & ~ENABLE_PROCESSED_OUTPUT); // Needed so that cursor moves on to next line

            size_t length = RadLine(hConsoleInput, hStdOutput, (wchar_t*) lpBuffer, nNumberOfCharsToRead);

            SetConsoleMode(hStdOutput, modeout);

            if (lpNumberOfCharsRead != nullptr)
                *lpNumberOfCharsRead = static_cast<DWORD>(length);

            return TRUE;
        }
        else
        {
            DebugOut(TEXT("RadLine RadReadConsoleW Orig\n"));
            return pOrigReadConsoleW(hConsoleInput, lpBuffer, nNumberOfCharsToRead, lpNumberOfCharsRead, pInputControl);
        }
    }
}
