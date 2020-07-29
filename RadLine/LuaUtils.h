#pragma once

extern "C"
{
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <locale>
#include <codecvt>

class LuaCloser
{
public:
    void operator()(lua_State* lua)
    {
        lua_close(lua);
    }
};

void check(bool b, const char* msg)
{
    if (!b)
        throw std::exception(msg);
}

inline std::wstring From_utf8(std::string_view str)
{
    wchar_t res[1024];
    int len = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int) str.length(), res, ARRAYSIZE(res));
    return std::wstring(res, len);
}

inline std::string To_utf8(std::wstring_view wstr)
{
    char res[1024];
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int) wstr.length(), res, ARRAYSIZE(res), nullptr, nullptr);
    return std::string(res, len);
}

inline void LuaPush(lua_State* lua, std::wstring_view w)
{
    std::string s(To_utf8(w));
    lua_pushstring(lua, s.c_str());
}

inline void LuaPush(lua_State* lua, const std::wstring& w)
{
    std::string s(To_utf8(w));
    lua_pushstring(lua, s.c_str());
}

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
