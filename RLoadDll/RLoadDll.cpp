#include <cstdio>
#include <Windows.h>
#include <winternl.h>
#include <Shlwapi.h>
#include <tchar.h>

#ifdef _UNICODE
#define LOAD_LIBRARY        "LoadLibraryW"
#define GET_MODULE_HANDLE   "GetModuleHandleW"
#else
#define LOAD_LIBRARY        "LoadLibraryA"
#define GET_MODULE_HANDLE   "GetModuleHandleA"
#endif

typedef LONG KPRIORITY;

// Should be the same as PROCESS_BASIC_INFORMATION
typedef struct _PROCESS_BASIC_INFORMATION_X
{
    NTSTATUS ExitStatus;
    PPEB PebBaseAddress;
    ULONG_PTR AffinityMask;
    KPRIORITY BasePriority;
    HANDLE UniqueProcessId;
    HANDLE InheritedFromUniqueProcessId;
} PROCESS_BASIC_INFORMATION_X, *PPROCESS_BASIC_INFORMATION_X;

DWORD GetParentProcessID(HANDLE hProcess)
{
    PROCESS_BASIC_INFORMATION_X     pbi;
    ULONG                           ulRetLen;
    NTSTATUS ntStatus = NtQueryInformationProcess(hProcess,
        ProcessBasicInformation,
        (void*) &pbi,
        sizeof(PROCESS_BASIC_INFORMATION),
        &ulRetLen
    );

    return !ntStatus ? (DWORD) (INT_PTR) pbi.InheritedFromUniqueProcessId : MAXDWORD;
}

DWORD ToProcessId(LPCTSTR pid)
{
    if (_tcscmp(pid, _T("-")) == 0)
        return GetParentProcessID(GetCurrentProcess());
    else
        return _tstoi(pid);
}

HANDLE FindProcess(LPCTSTR pid)
{
    return OpenProcess(PROCESS_ALL_ACCESS, FALSE, ToProcessId(pid));
}

BOOL CallRemoteFunc(HANDLE hProcess, LPCSTR pProcName, void* pLibRemote, DWORD* pRet)
{
    LPTHREAD_START_ROUTINE pRoutine = (LPTHREAD_START_ROUTINE) GetProcAddress(GetModuleHandle(TEXT("Kernel32")), pProcName);
    if (pRoutine == NULL)
    {
        _ftprintf(stderr, _T("Error: GetProcAddress 0x%08x\n"), GetLastError());
        return FALSE;
    }

    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, pRoutine, pLibRemote, 0, NULL);
    if (hThread == NULL)
    {
        _ftprintf(stderr, _T("Error: CreateRemoteThread 0x%08x\n"), GetLastError());
        CloseHandle(hThread);
        return FALSE;
    }
    if (WaitForSingleObject(hThread, INFINITE) == WAIT_FAILED)
        _ftprintf(stderr, _T("Error: WaitForSingleObject 0x%08x\n"), GetLastError());

    if (!GetExitCodeThread(hThread, pRet))
    {
        _ftprintf(stderr, _T("Error: GetExitCodeThread 0x%08x\n"), GetLastError());
    }

    CloseHandle(hThread);

    return TRUE;
}

int _tmain(int argc, LPCTSTR* argv)
{
    if (argc != 3)
    {
        _putts(_T("RLoadDll <pid> <dll> - load a dll into a another process."));
        return 0;
    }

    HANDLE hProcess = FindProcess(argv[1]);
    TCHAR  szCurrentDir[_MAX_PATH];
    GetCurrentDirectory(ARRAYSIZE(szCurrentDir), szCurrentDir);
    TCHAR  szLibPath[_MAX_PATH];    // module (including full path!);
    PathCombine(szLibPath, szCurrentDir, argv[2]);

    if (!PathFileExists(szLibPath))
    {
        _ftprintf(stderr, _T("File not found: %s\n"), argv[2]);
        return EXIT_FAILURE;
    }

    void* pLibRemote = VirtualAllocEx(hProcess, NULL, sizeof(szLibPath), MEM_COMMIT, PAGE_READWRITE);
    if (pLibRemote == nullptr)
    {
        _ftprintf(stderr, _T("Error: VirtualAllocEx 0x%08x\n"), GetLastError());
        return EXIT_FAILURE;
    }

    if (WriteProcessMemory(hProcess, pLibRemote, (void*) szLibPath, sizeof(szLibPath), NULL) == 0)
    {
        _ftprintf(stderr, _T("Error: WriteProcessMemory 0x%08x\n"), GetLastError());
        VirtualFreeEx(hProcess, pLibRemote, sizeof(szLibPath), MEM_RELEASE);
        return EXIT_FAILURE;
    }

    DWORD hLibModule = NULL;
    if (!CallRemoteFunc(hProcess, GET_MODULE_HANDLE, pLibRemote, &hLibModule))
    {
        VirtualFreeEx(hProcess, pLibRemote, sizeof(szLibPath), MEM_RELEASE);
        return EXIT_FAILURE;
    }

    if (hLibModule != NULL)
    {
        _ftprintf(stderr, _T("Error: Module already loaded.\n"));
        VirtualFreeEx(hProcess, pLibRemote, sizeof(szLibPath), MEM_RELEASE);
        return EXIT_FAILURE;
    }

    if (!CallRemoteFunc(hProcess, LOAD_LIBRARY, pLibRemote, &hLibModule))
    {
        VirtualFreeEx(hProcess, pLibRemote, sizeof(szLibPath), MEM_RELEASE);
        return EXIT_FAILURE;
    }

    if (hLibModule == NULL)
        _ftprintf(stderr, _T("Error: LoadModule Failed\n"));
    else
        _ftprintf(stdout, _T("Loaded: 0x%08x\n"), hLibModule);

    VirtualFreeEx(hProcess, pLibRemote, sizeof(szLibPath), MEM_RELEASE);

    return hLibModule != NULL ? EXIT_SUCCESS : EXIT_FAILURE;
}
