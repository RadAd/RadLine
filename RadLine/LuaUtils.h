#pragma once

extern "C"
{
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

//#include <assert.h>
#include "StringUtils.h"
#include "WinHelpers.h"

class LuaCloser
{
public:
    void operator()(lua_State* lua)
    {
        lua_close(lua);
    }
};

inline void check(bool b, const char* msg)
{
    if (!b)
        throw std::exception(msg);
}

inline void LuaPush(lua_State* lua, std::wstring_view w)
{
    std::string s(To_utf8(w));
    lua_pushstring(lua, s.c_str());
}

#if 0
inline void LuaPush(lua_State* lua, const std::wstring& w)
{
    std::string s(To_utf8(w));
    lua_pushstring(lua, s.c_str());
}
#endif

inline void LuaPush(lua_State* lua, const std::vector<std::wstring_view>& v)
{
    lua_createtable(lua, (int) v.size(), 0);
    int i = 1;
    for (std::wstring_view w : v)
    {
        LuaPush(lua, w);
        lua_rawseti(lua, -2, i);
        ++i;
    }
}

inline void LuaPush(lua_State* lua, const std::vector<std::wstring>& v)
{
    lua_createtable(lua, (int)v.size(), 0);
    int i = 1;
    for (const std::wstring& w : v)
    {
        LuaPush(lua, w);
        lua_rawseti(lua, -2, i);
        ++i;
    }
}

inline std::wstring LuaPopString(lua_State* lua)
{
    check(lua_isstring(lua, -1), "string exepcted");
    std::wstring s = From_utf8(lua_tostring(lua, -1));
    lua_pop(lua, 1);
    return s;
}

inline lua_Integer LuaPopInteger(lua_State* lua)
{
    check(lua_isinteger(lua, -1), "integer expected");
    lua_Integer i = lua_tointeger(lua, -1);
    lua_pop(lua, 1);
    return i;
}

inline std::vector<std::wstring> LuaPopVectorOfStrings(lua_State* lua)
{
    check(lua_istable(lua, -1), "table expected");
    // TODO Check indexes are integers and not strings
    std::vector<std::wstring> vs;
    size_t len = lua_rawlen(lua, -1);
    vs.reserve(len);
    for (size_t i = 1; i <= len; ++i)
    {
        lua_rawgeti(lua, -1, i);
        vs.emplace_back(LuaPopString(lua));
    }
    lua_pop(lua, 1);
    return vs;
}

inline std::wstring LuaGetModuleFileName(lua_State* lua, std::wstring_view module)
{
    assert(lua_gettop(lua) == 0);
    lua_getglobal(lua, "package");
    lua_getfield(lua, -1, "path");
    std::wstring cur_path = LuaPopString(lua);
    lua_pop(lua, 1);
    assert(lua_gettop(lua) == 0);

    const std::vector<std::wstring_view> paths = Split(cur_path, L';');
    for (std::wstring_view p : paths)
    {
        std::wstring strFile(p);
        Replace(strFile, L"?", module);
        if (FileExists(strFile.c_str()))
            return strFile;
    }

    return {};
}

inline int LuaLoadModule(lua_State* lua)
{
    std::wstring module = LuaPopString(lua);
    assert(lua_gettop(lua) == 0);
    std::wstring file = LuaGetModuleFileName(lua, module);
    return luaL_dofile(lua, To_utf8(file).c_str());
}

inline bool LuaRequire(lua_State* lua, const char* modname, std::wstring& msg)
{
    assert(lua_gettop(lua) == 0);
    luaL_requiref(lua, modname, LuaLoadModule, TRUE);
    assert(lua_gettop(lua) == 1);
    if (lua_isnil(lua, -1))
        lua_pop(lua, 1);
    else if (lua_isstring(lua, -1))
        msg = LuaPopString(lua);
    else
    {
        msg = L"Unexpected type from luaL_requiref";
        lua_pop(lua, 1);
    }
    assert(lua_gettop(lua) == 0);

    return msg.empty();
}
