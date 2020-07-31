#include <cstdio>
#include <Windows.h>
#include <winternl.h>
#include <Shlwapi.h>
#include <tchar.h>
#include <memory>

#ifdef _UNICODE
#define LOAD_LIBRARY        "LoadLibraryW"
#define SET_DLL_DIRECTORY   "SetDllDirectoryW"
#define GET_MODULE_HANDLE   "GetModuleHandleW"
#else
#define LOAD_LIBRARY        "LoadLibraryA"
#define SET_DLL_DIRECTORY   "SetDllDirectoryA"
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

class VirtualAllocDeleter
{
public:
    VirtualAllocDeleter(HANDLE hProcess)
        : hProcess(hProcess)
    {
    }

    void operator()(void* p) const
    {
        if (p != nullptr && VirtualFreeEx(hProcess, p, 0, MEM_RELEASE) == 0)
            _ftprintf(stderr, _T("Error: VirtualFreeEx 0x%08x\n"), GetLastError());
    }

private:
    const HANDLE hProcess;
};

int _tmain(int argc, LPCTSTR* argv)
{
    BOOL bSetDirectory = FALSE;
    LPCTSTR strPid = nullptr;
    LPCTSTR strLib = nullptr;

    for (int argi = 1; argi < argc; ++argi)
    {
        LPCTSTR arg = argv[argi];
        if (arg[0] == _T('/'))
        {
            if (_tccmp(arg, _T("/d")) == 0)
                bSetDirectory = TRUE;
            else
                _ftprintf(stderr, _T("Unknown option: %s\n"), arg);
        }
        else if (strPid == nullptr)
            strPid = arg;
        else if (strLib == nullptr)
            strLib = arg;
        else
            _ftprintf(stderr, _T("Too many parameters: %s\n"), arg);
    }

    if (strPid == nullptr || strLib == nullptr)
    {
        _putts(_T("RLoadDll [/d] <pid> <dll> - load a dll into a another process."));
        _putts(_T("     /d - Set dll directory in order to load dependent dlls."));
        return 0;
    }

    HANDLE hProcess = FindProcess(strPid);

    if (hProcess == NULL)
    {
        _ftprintf(stderr, _T("Process not found: %s\n"), strPid);
        return EXIT_FAILURE;
    }

    TCHAR  szLibPath[_MAX_PATH];    // module (including full path!);
    {
        TCHAR  szCurrentDir[_MAX_PATH];
        GetCurrentDirectory(ARRAYSIZE(szCurrentDir), szCurrentDir);
        PathCombine(szLibPath, szCurrentDir, strLib);
    }

    if (!PathFileExists(szLibPath))
    {
        _ftprintf(stderr, _T("File not found: %s\n"), strLib);
        return EXIT_FAILURE;
    }

    std::unique_ptr<void, VirtualAllocDeleter> pLibRemote(VirtualAllocEx(hProcess, NULL, sizeof(szLibPath), MEM_COMMIT, PAGE_READWRITE), VirtualAllocDeleter(hProcess));
    if (pLibRemote == nullptr)
    {
        _ftprintf(stderr, _T("Error: VirtualAllocEx 0x%08x\n"), GetLastError());
        return EXIT_FAILURE;
    }

    if (WriteProcessMemory(hProcess, pLibRemote.get(), (void*) szLibPath, sizeof(szLibPath), NULL) == 0)
    //if (WriteProcessMemory(hProcess, pLibRemote.get(), (void*)pFileName, 20, NULL) == 0)
    {
        _ftprintf(stderr, _T("Error: WriteProcessMemory 0x%08x\n"), GetLastError());
        return EXIT_FAILURE;
    }

    DWORD hLibModule = NULL;
    if (!CallRemoteFunc(hProcess, GET_MODULE_HANDLE, pLibRemote.get(), &hLibModule))
    {
        return EXIT_FAILURE;
    }

    if (hLibModule != NULL)
    {
        _ftprintf(stderr, _T("Error: Module already loaded.\n"));
        return EXIT_FAILURE;
    }

    if (bSetDirectory)
    {
        TCHAR* pFileName = PathFindFileName(szLibPath);

        TCHAR bSave = _T('\0');
        std::swap(bSave, *(pFileName - 1));

        if (WriteProcessMemory(hProcess, pLibRemote.get(), (void*)szLibPath, sizeof(szLibPath), NULL) == 0)
        {
            _ftprintf(stderr, _T("Error: WriteProcessMemory 0x%08x\n"), GetLastError());
            return EXIT_FAILURE;
        }

        DWORD bRet = FALSE;
        if (!CallRemoteFunc(hProcess, SET_DLL_DIRECTORY, pLibRemote.get(), &bRet))
        {
            return EXIT_FAILURE;
        }

        if (!bRet)
        {
            _ftprintf(stderr, _T("Error: SetDllDirectory\n"));
            return EXIT_FAILURE;
        }

        std::swap(bSave, *(pFileName - 1));
    }

    if (WriteProcessMemory(hProcess, pLibRemote.get(), (void*)szLibPath, sizeof(szLibPath), NULL) == 0)
    {
        _ftprintf(stderr, _T("Error: WriteProcessMemory 0x%08x\n"), GetLastError());
        return EXIT_FAILURE;
    }

    if (!CallRemoteFunc(hProcess, LOAD_LIBRARY, pLibRemote.get(), &hLibModule))
    {
        return EXIT_FAILURE;
    }

    if (hLibModule == NULL)
        _ftprintf(stderr, _T("Error: LoadModule Failed\n"));
    else
        _ftprintf(stdout, _T("Loaded: 0x%08x\n"), hLibModule);

    if (bSetDirectory)
    {
        // TODO Should put back the original dll directory
        // Can't get the original using current method
        // Could create a dll that gets injected and then call a method on that to inject another dll

        DWORD bRet = FALSE;
        if (!CallRemoteFunc(hProcess, SET_DLL_DIRECTORY, nullptr, &bRet))
        {
            return EXIT_FAILURE;
        }

        if (!bRet)
        {
            _ftprintf(stderr, _T("Error: SetDllDirectory\n"));
            return EXIT_FAILURE;
        }
    }

    return hLibModule != NULL ? EXIT_SUCCESS : EXIT_FAILURE;
}
