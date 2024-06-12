#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <tchar.h>

#include "Debug.h"

#include "..\RadReadConsole\RadReadConsole.h"

DWORD WINAPI PipeThread(LPVOID lpvParam)
{
    DebugOut(TEXT("RadLine PipeThread Started\n"));

    TCHAR pipename[100];
    _stprintf_s(pipename, TEXT("\\\\.\\pipe\\radline.%d"), GetCurrentProcessId());

    HANDLE hPipe = CreateNamedPipe(
        pipename,
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        PIPE_UNLIMITED_INSTANCES,
        512,  // nOutBufferSize
        512,  // nInBufferSize
        NMPWAIT_USE_DEFAULT_WAIT,
        nullptr);

    if (!hPipe)
    {
        DebugOut(TEXT("RadLine PipeThread Error CreateNamedPipe %d\n"), GetLastError());
        return 0;
    }

    while (true)
    {
        BOOL fConnected = ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (!fConnected)
        {
            DebugOut(TEXT("RadLine PipeThread Error ConnectNamedPipe %d\n"), GetLastError());
            continue;
        }

        TCHAR buffer[1024];
        DWORD cbBytesRead = 0;
        BOOL fSuccess = ReadFile(hPipe, buffer, ARRAYSIZE(buffer) * sizeof(TCHAR), &cbBytesRead, nullptr);

        if (!fSuccess || cbBytesRead == 0)
        {
            DebugOut(TEXT("RadLine PipeThread Error ReadFile %d\n"), GetLastError());
            DisconnectNamedPipe(hPipe);
            continue;
        }

        buffer[cbBytesRead / sizeof(TCHAR)] = TEXT('\0');

        if (_tcscmp(buffer, TEXT("hist\n")) == 0)
            WriteHistory(hPipe);

        FlushFileBuffers(hPipe);
        DisconnectNamedPipe(hPipe);
    }

    CloseHandle(hPipe);
    return 0;
}
