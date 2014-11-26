#pragma once

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include "Registry.h"
#include "Selector.h"
#include <tuple>
#include "util.h"
#include <vector>

namespace sel {
inline int atpanic(lua_State* l)
{
  std::string err = lua_tostring(l, -1);
  throw std::runtime_error(err);
}

class State {
private:
    std::shared_ptr<lua_State> _l;
    Registry *_registry;

public:
    State() : State(false) {}
    State(bool should_open_libs):_registry(new Registry(_l)) {
        _l = std::shared_ptr<lua_State>(luaL_newstate(), LuaStateOwnerDeleter(_registry));
        if (_l == nullptr) throw 0;
        if (should_open_libs) luaL_openlibs(_l.get());
        lua_atpanic(_l.get(), atpanic);
    }
    State(lua_State *l): _registry(new Registry(_l)) {
        _l = std::shared_ptr<lua_State>(l, LuaStateDeleter(_registry));
        lua_atpanic(_l.get(), atpanic);
    }
    State(const State &other) = delete;
    State &operator=(const State &other) = delete;
    State(State &&other)
        : _l(other._l),
          _registry(std::move(other._registry)) {
        other._l = nullptr;
        lua_atpanic(_l.get(), atpanic);
    }
    State &operator=(State &&other) {
        if (&other == this) return *this;
        _l = other._l;
        _registry = std::move(other._registry);
        other._l = nullptr;
        lua_atpanic(_l.get(), atpanic);
        return *this;
    }
    ~State() {
    }

    int Size() const {
        return lua_gettop(_l.get());
    }

    bool Load(const std::string &file) {
        int status = luaL_loadfile(_l.get(), file.c_str());
#if LUA_VERSION_NUM >= 502
        if (status != LUA_OK) {
#else
        if (status != 0) {
#endif
            if (status == LUA_ERRSYNTAX) {
                const char *msg = lua_tostring(_l.get(), -1);
                _print(msg ? msg : (file + ": syntax error").c_str());
            } else if (status == LUA_ERRFILE) {
                const char *msg = lua_tostring(_l.get(), -1);
                _print(msg ? msg : (file + ": file error").c_str());
            }
            lua_remove(_l.get() , -1);
            return false;
        }
        if (!lua_pcall(_l.get(), 0, LUA_MULTRET, 0))
            return true;

        const char *msg = lua_tostring(_l.get(), -1);
        _print(msg ? msg : (file + ": dofile failed").c_str());
        lua_remove(_l.get(), -1);
        return false;
    }

    void OpenLib(const std::string& modname, lua_CFunction openf) {
#if LUA_VERSION_NUM >= 502
        luaL_requiref(_l.get(), modname.c_str(), openf, 1);
#else
        lua_pushcfunction(_l.get(), openf);
        lua_pushstring(_l.get(), modname.c_str());
        lua_call(_l.get(), 1, 0);
#endif
    }

    void Push() {} // Base case

    template <typename T, typename... Ts>
    void Push(T &&value, Ts&&... values) {
        detail::_push(_l, std::forward<T>(value));
        Push(std::forward<Ts>(values)...);
    }

    // Lua stacks are 1 indexed from the bottom and -1 indexed from
    // the top
    template <typename T>
    T Read(const int index) const {
        return detail::_get(detail::_id<T>{}, _l.get(), index);
    }

    bool CheckNil(const std::string &global) {
        lua_getglobal(_l.get(), global.c_str());
        const bool result = lua_isnil(_l.get(), -1);
        lua_pop(_l.get(), 1);
        return result;
    }
public:
    template<size_t SIZE>
    Selector operator[](const char (&name)[SIZE]) {
        return Selector(_l, *_registry, name);
    }
    
    Selector operator[](const std::string &name) {
        return Selector(_l, *_registry, name);
    }

    bool operator()(const char *code) {
        bool result = !luaL_dostring(_l.get(), code);
        if (result) lua_settop(_l.get(), 0);
        return result;
    }
    bool operator()(const std::string &code) {
        bool result = !luaL_dostring(_l.get(), code.c_str());
        if (result) lua_settop(_l.get(), 0);
        return result;
    }
    void ForceGC() {
        lua_gc(_l.get(), LUA_GCCOLLECT, 0);
    }

    void InteractiveDebug() {
        luaL_dostring(_l.get(), "debug.debug()");
    }

    friend std::ostream &operator<<(std::ostream &os, const State &state);
};

inline std::ostream &operator<<(std::ostream &os, const State &state) {
    os << "sel::State - " << state._l;
    return os;
}
}
