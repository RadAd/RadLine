#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "Debug.h"
#include "RadLineReadConsole.h"
#include "DynEnv.h"
#include <MinHook.h>

HMODULE g_hModule = NULL;
DWORD WINAPI PipeThread(LPVOID lpvParam);

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID /*lpReserved*/
)
{
    g_hModule = hModule;

    const struct HookSpec
    {
        LPCTSTR strModule;
        LPCSTR strFunction;
        LPVOID pTarget;
        LPVOID pDetour;
        LPVOID* ppOriginal;
    } hooks[] = {
        { TEXT("KernelBase.dll"),   "ReadConsoleW",             ReadConsoleW,               RadLineReadConsoleW,        (LPVOID*) &pOrigReadConsoleW },
        { TEXT("KernelBase.dll"),   "GetEnvironmentVariableW",  GetEnvironmentVariableW,    RadGetEnvironmentVariableW, (LPVOID*) &pOrigGetEnvironmentVariableW },
    };

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        {
            OutputDebugString(TEXT("RadLine DLL_PROCESS_ATTACH\n"));

            MH_STATUS status = MH_OK;
            status = MH_Initialize();
            DebugOut(TEXT("RadLine MH_Initialize %d\n"), status);

            for (const HookSpec& hs : hooks)
            {
#if 1
                const HMODULE h = GetModuleHandle(hs.strModule);
                DebugOut(TEXT("RadLine %s 0x%x\n"), hs.strModule, HandleToULong(h));
                if (h == NULL)
                    continue;
                const FARPROC pTarget = GetProcAddress(h, hs.strFunction);
                DebugOut(TEXT("RadLine GetProcAddress %S 0x%0p\n"), hs.strFunction, pTarget);
#else
                const LPVOID pTarget = hs.pTarget;
#endif
                if (pTarget == nullptr)
                    continue;
                status = MH_CreateHook(pTarget, hs.pDetour, hs.ppOriginal);
                DebugOut(TEXT("RadLine MH_CreateHook %S %d 0x%0p 0x%0p\n"), hs.strFunction, status, hs.pDetour, *hs.ppOriginal);
                status = MH_EnableHook(pTarget);
                DebugOut(TEXT("RadLine MH_EnableHook %S %d\n"), hs.strFunction, status);
                //status = MH_EnableHook(hs.pTarget);
                //DebugOut(TEXT("RadLine MH_EnableHook %S %d\n"), hs.strFunction, status);
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

            for (const HookSpec& hs : hooks)
            {
                const HMODULE h = GetModuleHandle(hs.strModule);
                DebugOut(TEXT("RadLine %s 0x%x\n"), hs.strModule, HandleToULong(h));
                if (h == NULL)
                    continue;
                const FARPROC pTarget = GetProcAddress(h, hs.strFunction);
                if (pTarget == nullptr)
                    continue;
                status = MH_DisableHook(pTarget);
                DebugOut(TEXT("RadLine MH_DisableHook %S %d\n"), hs.strFunction, status);
            }

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
