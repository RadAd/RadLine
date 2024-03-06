#include <Windows.h>
#include <stdio.h>
#include <wchar.h>
#include <shlwapi.h>

typedef decltype(&GetEnvironmentVariableW) FuncGetEnvironmentVariableW;

extern HMODULE g_hModule;

#include "bufstring.h"
#include "WinHelpers.h"

DWORD ReadFromHandle(HANDLE hReadPipe, _Out_writes_to_opt_(nSize, return +1) LPWSTR lpBuffer, _In_ DWORD nSize)
{
    if (lpBuffer == nullptr)
        return 0;

    DWORD dwReadTotal = 0;
    //LPSTR lpBuffer2 = (LPSTR) lpBuffer;
    //nSize /= sizeof(WCHAR);

    for (;;)
    {
        BYTE Buffer[1024];
        DWORD dwRead;
        if (!ReadFile(hReadPipe, &Buffer, ARRAYSIZE(Buffer), &dwRead, NULL))
            break;

        // TODO Do this once after we get all text
        INT iResult = 0;
        if (IsTextUnicode(Buffer, dwRead, &iResult))
        {
            memcpy_s(lpBuffer + dwReadTotal, nSize - dwReadTotal, Buffer, dwRead);
            dwReadTotal += dwRead / sizeof(WCHAR);
        }
        else
            dwReadTotal += MultiByteToWideChar(CP_UTF8, 0, (LPCSTR) &Buffer, dwRead, lpBuffer + dwReadTotal, nSize - dwReadTotal);

        //if (!bSuccess) break;
    }
    lpBuffer[dwReadTotal] = L'\0';
    return dwReadTotal;
}

DWORD GetUserCurrentDirectory(_Out_writes_to_(nSize, return +1) LPWSTR lpBuffer, _In_ DWORD nSize)
{
    wchar_t UserProfile[MAX_PATH] = L"";
    DWORD len = GetEnvironmentVariableW(L"USERPROFILE", UserProfile);
    DWORD ret = GetCurrentDirectoryW(nSize, lpBuffer);
    if (ret >= len && _wcsnicmp(UserProfile, lpBuffer, len) == 0)
    {
        bufstring s(lpBuffer, nSize, ret);
        s.replace(s.begin(), len, L"~", 1);
        ret = static_cast<DWORD>(s.length());
    }
    return ret;
}

extern "C" {

    FuncGetEnvironmentVariableW pOrigGetEnvironmentVariableW = nullptr;

    __declspec(dllexport)
    DWORD WINAPI RadGetEnvironmentVariableW(
        _In_opt_ LPCWSTR lpName,
        _Out_writes_to_opt_(nSize, return +1) LPWSTR lpBuffer,
        _In_ DWORD nSize
    )
    {
        DWORD ret = 0;
        const size_t len = lpName != nullptr ? wcslen(lpName) : 0;

        if (lpBuffer == nullptr || lpName == nullptr)
            // TODO Handle the case when lpBuffer == nullptr
            ret = pOrigGetEnvironmentVariableW(lpName, lpBuffer, nSize);
        else if (lpName[0] == L'(' && lpName[len - 1] == L')')
        {
            WCHAR strCmdLine[2048];
            wcsncpy_s(strCmdLine, ARRAYSIZE(strCmdLine), lpName + 1, len - 2);
            for (LPWSTR p = strCmdLine; *p != L'\0'; ++p)
                if (*p == L'!')
                    *p = L'%';
            ExpandEnvironmentStrings(strCmdLine, ARRAYSIZE(strCmdLine));

            STARTUPINFO            siStartupInfo = { sizeof(siStartupInfo) };
            siStartupInfo.dwFlags = STARTF_USESTDHANDLES;
            siStartupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);   // TODO Should this be NULL?
            siStartupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
            siStartupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);

            SECURITY_ATTRIBUTES sa = { sizeof(sa) };
            sa.bInheritHandle = TRUE;

            HANDLE hReadPipe = NULL;
            CreatePipe(&hReadPipe, &siStartupInfo.hStdOutput, &sa, sizeof(sa));
            SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

            PROCESS_INFORMATION    piProcessInfo = {};
            if (CreateProcess(NULL, strCmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &siStartupInfo, &piProcessInfo) == FALSE)
            {
                CloseHandle(siStartupInfo.hStdOutput);
                CloseHandle(hReadPipe);
                //ret = wsprintf(lpBuffer, L"ERROR: Could not create new process (%d).", GetLastError()); // TODO Write to stderr?
                fwprintf(stderr, L"ERROR: Could not create new process (%d).", GetLastError());
            }
            else
            {
                CloseHandle(siStartupInfo.hStdOutput);
                ret = ReadFromHandle(hReadPipe, lpBuffer, nSize);
                CloseHandle(hReadPipe);

                WaitForSingleObject(piProcessInfo.hProcess, INFINITE);
                //DWORD psret;
                //GetExitCodeProcess(piProcessInfo.hProcess, &psret);

                CloseHandle(piProcessInfo.hThread);
                CloseHandle(piProcessInfo.hProcess);

                if (ret >= 2 && (lpBuffer[ret - 2] == L'\r' || lpBuffer[ret - 1] == L'\n'))
                    ret -= 2;

                for (DWORD i = 0; i < ret; ++i)
                    if (lpBuffer[i] == L'\r' || lpBuffer[i] == L'\n')
                        lpBuffer[i] = L' ';

                lpBuffer[ret] = L'\0';

                ret = ExpandEnvironmentStrings(lpBuffer, nSize);
            }
        }
        else if (_wcsicmp(lpName, L"RADLINE_LOADED") == 0)
        {
            wcscpy_s(lpBuffer, nSize, L"1");
            ret = static_cast<DWORD>(wcslen(lpBuffer));
        }
        else if (_wcsicmp(lpName, L"RADLINE_DIR") == 0)
        {
            GetModuleFileNameW(g_hModule, lpBuffer, nSize);
            wchar_t* const pFileName = PathFindFileNameW(lpBuffer) - 1;
            *pFileName = L'\0';
            ret = static_cast<DWORD>(pFileName - lpBuffer);
        }
        else if (_wcsicmp(lpName, L"__PID__") == 0)
        {
            DWORD pid = GetCurrentProcessId();
            ret = wsprintf(lpBuffer, L"%d", pid);
        }
        else if (_wcsicmp(lpName, L"__TICK__") == 0)
        {
            DWORD tick = GetTickCount();
            ret = wsprintf(lpBuffer, L"%d", tick);
        }
        else if (_wcsicmp(lpName, L"USERCD") == 0)
        {
            ret = GetUserCurrentDirectory(lpBuffer, nSize);
        }
        else if (_wcsicmp(lpName, L"RAWPROMPT") == 0)
        {
            ret = pOrigGetEnvironmentVariableW(L"PROMPT", lpBuffer, nSize);
        }
        else
        {
            ret = pOrigGetEnvironmentVariableW(lpName, lpBuffer, nSize);

            if (wcscmp(lpName, L"PROMPT") == 0)
            {
                LPWSTR e = wcsstr(lpBuffer, L"$U");
                if (e != nullptr)
                {
                    wchar_t UserDirectory[MAX_PATH];
                    DWORD len = GetUserCurrentDirectory(UserDirectory, ARRAYSIZE(UserDirectory));

                    bufstring str(lpBuffer, nSize, ret);
                    str.replace(e, 2, UserDirectory, len);
                    ret = static_cast<DWORD>(str.length());
                }
                e = wcsstr(lpBuffer, L"$-");
                if (e != nullptr)
                {
                    DWORD pid;
                    DWORD count = GetConsoleProcessList(&pid, 1);
                    wcscpy_s(e + count, nSize - (e - lpBuffer) - count, e + 2);
                    wmemset(e, L'-', count);
                    ret += count - 2;
                }
                for (int i = 0; lpBuffer[i] != L'\0'; ++i)
                    if (lpBuffer[i] == L'!')
                        lpBuffer[i] = L'%';
                ret = ExpandEnvironmentStrings(lpBuffer, nSize); // Note doesn't use GetEnvironmentVariable
            }
            else if (ret == 0)
            {
                if (const wchar_t* p = wcschr(lpName, L'?'))
                {
                    ret = static_cast<DWORD>(wcslen(p + 1));
                    wmemcpy_s(lpBuffer, nSize, p + 1, ret + 1);
                }
            }
        }
        return ret;
    }
};
