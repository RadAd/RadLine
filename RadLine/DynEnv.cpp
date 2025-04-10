#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <stdio.h>
#include <wchar.h>
#include <shlwapi.h>
#include <memory>

typedef decltype(&GetEnvironmentVariableW) FuncGetEnvironmentVariableW;

extern HMODULE g_hModule;

#include "bufstring.h"
#include "WinHelpers.h"
#include "LuaUtils.h"
#include "LuaHelpers.h"
//#include "Debug.h"

extern "C" {
    FuncGetEnvironmentVariableW pOrigGetEnvironmentVariableW = nullptr;
}

namespace
{
    int l_OrigGetEnvironmentVariable(lua_State* lua)
    {
        std::wstring name = From_utf8(luaL_checklstring(lua, -1, nullptr));
        std::vector<TCHAR> buffer(8192);
        const DWORD ret = pOrigGetEnvironmentVariableW(name.c_str(), buffer.data(), (DWORD) buffer.size());
        LuaPush(lua, std::wstring_view(buffer.data(), ret));
        return 1;  /* number of results */
    }

    lua_State* InitLua()
    {
        std::unique_ptr<lua_State, LuaCloser> L(luaL_newstate());
        luaL_openlibs(L.get());
        SetLuaPath(L.get());
        return L.release();
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

#if 0
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
#endif
}

extern "C" {
    __declspec(dllexport)
    DWORD WINAPI RadGetEnvironmentVariableW(
        _In_opt_ LPCWSTR lpName,
        _Out_writes_to_opt_(nSize, return +1) LPWSTR lpBuffer,
        _In_ DWORD nSize
    )
    {
        std::wstring msg;
        static std::unique_ptr<lua_State, LuaCloser> L;
        if (!L)
        {
            L.reset(InitLua());
            lua_register(L.get(), "OrigGetEnvironmentVariable", l_OrigGetEnvironmentVariable);
            LuaRequire(L.get(), "UserRadEnv", msg);
        }
        assert(!L || lua_gettop(L.get()) == 0);
        if (!msg.empty())
            EasyWriteFormat(GetStdHandle(STD_ERROR_HANDLE), TEXT("Error loading UserRadEnv: %s\n"), msg.c_str());

        DWORD ret = 0;
        const size_t len = lpName != nullptr ? wcslen(lpName) : 0;

        if (lpBuffer == nullptr || lpName == nullptr)
            // TODO Handle the case when lpBuffer == nullptr
            ret = pOrigGetEnvironmentVariableW(lpName, lpBuffer, nSize);
#if 0
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
#endif
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
#if 0
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
#endif
        else
        {
            if (lua_getglobal(L.get(), "GetEnvironmentVariable") != LUA_TFUNCTION)
            {
                msg = L"GetEnvironmentVariable is not a function";
                lua_pop(L.get(), 1);

                ret = pOrigGetEnvironmentVariableW(lpName, lpBuffer, nSize);
            }
            else if (
                LuaPush(L.get(), lpName);
                lua_pcall(L.get(), 1, 1, 0) != LUA_OK)
            {
                EasyWriteFormat(GetStdHandle(STD_ERROR_HANDLE), TEXT("Error calling GetEnvironmentVariable: %s\n"), LuaPopString(L.get()).c_str());
                ret = pOrigGetEnvironmentVariableW(lpName, lpBuffer, nSize);
            }
            else
            {
                const std::wstring result = LuaPopString(L.get());
                wcscpy_s(lpBuffer, nSize, result.c_str());
                ret = std::min((DWORD) result.size(), nSize);
            }

#if 0
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
#if 0
                    // Can't use wcscpy_s with overlapping strings
                    wcscpy_s(e + count, nSize - (e - lpBuffer) - count, e + 2);
                    wmemset(e, L'-', count);
                    ret += count - 2;
#else
                    bufstring str(lpBuffer, nSize, ret);
                    str.replacecount(e, 2, L'-', count);
                    ret = static_cast<DWORD>(str.length());
#endif
                }
                for (int i = 0; lpBuffer[i] != L'\0'; ++i)
                    if (lpBuffer[i] == L'!')
                        lpBuffer[i] = L'%';
                ret = ExpandEnvironmentStrings(lpBuffer, nSize); // Note doesn't use GetEnvironmentVariable
            }
            else
#endif
                if (ret == 0)
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
