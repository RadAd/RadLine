#include <Windows.h>

#include "Debug.h"
#include "RadReadConsole.h"
#include "..\minhook\include\MinHook.h"

// TODO Detect if already loaded

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        {
            OutputDebugString(TEXT("RadLine DLL_PROCESS_ATTACH\n"));

            HMODULE h = LoadLibrary(L"KernelBase.dll");
            DebugOut(TEXT("RadLine KernelBase 0x%x\n"), h);

            MH_STATUS status = MH_OK;
            status = MH_Initialize();
            DebugOut(TEXT("RadLine MH_Initialize %d\n"), status);

            if (true)
            {
                FARPROC pTargetReadConsoleW = GetProcAddress(h, "ReadConsoleW");
                DebugOut(TEXT("RadLine GetProcAddress pTargetReadConsoleW 0x%0p\n"), pTargetReadConsoleW);
                status = MH_CreateHook(pTargetReadConsoleW, RadReadConsoleW, (LPVOID*) &pOrigReadConsoleW);
                DebugOut(TEXT("RadLine MH_CreateHook ReadConsoleW %d 0x%0p 0x%0p 0x%0p\n"), status, pTargetReadConsoleW, RadReadConsoleW, pOrigReadConsoleW);
                status = MH_EnableHook(pTargetReadConsoleW);
                DebugOut(TEXT("RadLine MH_EnableHook ReadConsoleW %d\n"), status);
            }

            OutputDebugString(TEXT("RadLine DLL_PROCESS_ATTACH done\n"));
        }
        break;
    case DLL_PROCESS_DETACH:
        {
            OutputDebugString(TEXT("RadLine DLL_PROCESS_DETACH\n"));
            MH_STATUS status = MH_OK;
            status = MH_DisableHook(ReadConsoleW);
            DebugOut(TEXT("RadLine MH_DisableHook ReadConsoleW %d\n"), status);
            status = MH_Uninitialize();
            DebugOut(TEXT("RadLine MH_Uninitialize %d\n"), status);
            OutputDebugString(TEXT("RadLine DLL_PROCESS_DETACH done\n"));
        }
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}
