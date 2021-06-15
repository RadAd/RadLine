#include <Windows.h>
#include <stdio.h>

DWORD RadExpandEnvironmentStrings(_In_ LPWSTR lpSrc, _In_ DWORD nSize)
{
    WCHAR strBuffer[2048];
    wcscpy_s(strBuffer, lpSrc);
    return ExpandEnvironmentStrings(strBuffer, lpSrc, nSize);
}

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

extern "C" {

    decltype(&GetEnvironmentVariableW) pOrigGetEnvironmentVariableW = nullptr;

    __declspec(dllexport)
    DWORD WINAPI RadGetEnvironmentVariableW(
        _In_opt_ LPCWSTR lpName,
        _Out_writes_to_opt_(nSize, return +1) LPWSTR lpBuffer,
        _In_ DWORD nSize
    )
    {
        DWORD ret = 0;
        const size_t len = lpName != nullptr ? wcslen(lpName) : 0;

        if (lpBuffer == nullptr)
            // TODO Handle the case when lpBuffer == nullptr
            ret = pOrigGetEnvironmentVariableW(lpName, lpBuffer, nSize);
        else if (lpName != nullptr && lpName[0] == L'(' && lpName[len - 1] == L')')
        {
            WCHAR strCmdLine[2048];
            wcsncpy_s(strCmdLine, ARRAYSIZE(strCmdLine), lpName + 1, len - 2);
            for (LPWSTR p = strCmdLine; *p != L'\0'; ++p)
                if (*p == L'!')
                    *p = L'%';
            RadExpandEnvironmentStrings(strCmdLine, ARRAYSIZE(strCmdLine));

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
            }
        }
        else if (lpName != nullptr && _wcsicmp(lpName, L"__PID__") == 0)
        {
            DWORD pid = GetCurrentProcessId();
            ret = wsprintf(lpBuffer, L"%d", pid);
        }
        else
        {
            ret = pOrigGetEnvironmentVariableW(lpName, lpBuffer, nSize);
            if (lpName != nullptr && lpBuffer != nullptr && wcscmp(lpName, L"PROMPT") == 0)
                RadExpandEnvironmentStrings(lpBuffer, nSize);
        }
        return ret;
    }
};
