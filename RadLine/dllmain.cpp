#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "Debug.h"
#include "RadReadConsole.h"
#include "DynEnv.h"
#include "..\minhook\include\MinHook.h"

HMODULE g_hModule = NULL;
DWORD WINAPI PipeThread(LPVOID lpvParam);

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID /*lpReserved*/
)
{
    g_hModule = hModule;

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        {
            OutputDebugString(TEXT("RadLine DLL_PROCESS_ATTACH\n"));

            HMODULE h = LoadLibrary(L"KernelBase.dll");
            DebugOut(TEXT("RadLine KernelBase 0x%x\n"), HandleToULong(h));

            MH_STATUS status = MH_OK;
            status = MH_Initialize();
            DebugOut(TEXT("RadLine MH_Initialize %d\n"), status);

            if (true && h != NULL)
            {
                FARPROC pTargetReadConsoleW = GetProcAddress(h, "ReadConsoleW");
                DebugOut(TEXT("RadLine GetProcAddress pTargetReadConsoleW 0x%0p\n"), pTargetReadConsoleW);
                status = MH_CreateHook(pTargetReadConsoleW, RadLineReadConsoleW, (LPVOID*) &pOrigReadConsoleW);
                DebugOut(TEXT("RadLine MH_CreateHook ReadConsoleW %d 0x%0p 0x%0p 0x%0p\n"), status, pTargetReadConsoleW, RadLineReadConsoleW, pOrigReadConsoleW);
                status = MH_EnableHook(pTargetReadConsoleW);
                DebugOut(TEXT("RadLine MH_EnableHook ReadConsoleW %d\n"), status);
            }

            if (true && h != NULL)
            {
                FARPROC pTargetGetEnvironmentVariableW = GetProcAddress(h, "GetEnvironmentVariableW");
                DebugOut(TEXT("RadLine GetProcAddress pTargetGetEnvironmentVariableW 0x%0p\n"), pTargetGetEnvironmentVariableW);
                status = MH_CreateHook(pTargetGetEnvironmentVariableW, RadGetEnvironmentVariableW, (LPVOID*) &pOrigGetEnvironmentVariableW);
                DebugOut(TEXT("RadLine MH_CreateHook GetEnvironmentVariableW %d 0x%0p 0x%0p 0x%0p\n"), status, pTargetGetEnvironmentVariableW, RadGetEnvironmentVariableW, pOrigGetEnvironmentVariableW);
                status = MH_EnableHook(pTargetGetEnvironmentVariableW);
                DebugOut(TEXT("RadLine MH_EnableHook GetEnvironmentVariableW %d\n"), status);
            }

            HANDLE hThread = CreateThread(nullptr, 0, PipeThread, nullptr, 0, nullptr);
            if (hThread)
                CloseHandle(hThread);
            else
                DebugOut(TEXT("RadLine Error CreateThread %d\n"), GetLastError());

            OutputDebugString(TEXT("RadLine DLL_PROCESS_ATTACH done\n"));
        }
        break;
    case DLL_PROCESS_DETACH:
        {
            OutputDebugString(TEXT("RadLine DLL_PROCESS_DETACH\n"));
            MH_STATUS status = MH_OK;
            status = MH_DisableHook(ReadConsoleW);
            DebugOut(TEXT("RadLine MH_DisableHook ReadConsoleW %d\n"), status);
            status = MH_DisableHook(GetEnvironmentVariableW);
            DebugOut(TEXT("RadLine MH_DisableHook GetEnvironmentVariableW %d\n"), status);
            status = MH_Uninitialize();
            DebugOut(TEXT("RadLine MH_Uninitialize %d\n"), status);
            // TODO Shutdown PipeThread
            OutputDebugString(TEXT("RadLine DLL_PROCESS_DETACH done\n"));
        }
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}
