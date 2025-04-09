#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "LuaHelpers.h"

#include <Windows.h>
#include <shlwapi.h>

#include "WinHelpers.h"

extern HMODULE g_hModule;

void SetLuaPath(lua_State* lua)
{
    lua_getglobal(lua, "package");
    {
        char strRadLinePath[MAX_PATH];
        DWORD len = GetModuleFileNameA(g_hModule, strRadLinePath, ARRAYSIZE(strRadLinePath));
        char* strFilename = PathFindFileNameA(strRadLinePath);
        strFilename[-1] = '\0';

        std::string cur_path;
#if 0
        lua_getfield(lua, -1, "path");
        cur_path = LuaPopString(lua);
#endif

        const char* paths[] = {
            R"(%LOCALAPPDATA%\RadSoft\RadLine)",
            R"(%ProgramData%\RadSoft\RadLine)",
            strRadLinePath,
        };
        for (const char* p : paths)
        {
            char strPath[MAX_PATH] = "";
            ExpandEnvironmentStringsA(p, strPath);
            if (!cur_path.empty())
                cur_path += L';';
            cur_path += strPath;
            cur_path += R"(\?;)";
            cur_path += strPath;
            cur_path += R"(\?.lua)";
        }
        lua_pushstring(lua, cur_path.c_str());
        lua_setfield(lua, -2, "path");
    }

    {
        HMODULE hLua = GetModuleHandle(L"lua.dll");
        char strPath[MAX_PATH];
        DWORD len = GetModuleFileNameA(hLua, strPath, ARRAYSIZE(strPath));
        char* strFilename = PathFindFileNameA(strPath);
        strcpy_s(strFilename, ARRAYSIZE(strPath) - len, "?.dll");
        lua_pushstring(lua, strPath);
        lua_setfield(lua, -2, "cpath");
    }

    lua_pop(lua, 1);
}
