#pragma once

#include <memory>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace sel {
namespace detail {
class LuaRefDeleter {
private:
    std::shared_ptr<lua_State> _state;
public:
    LuaRefDeleter(const std::shared_ptr<lua_State> &state) : _state{state} {}
    void operator()(int *ref) const {
        luaL_unref(_state.get(), LUA_REGISTRYINDEX, *ref);
        delete ref;
    }
};
}
class LuaRef {
private:
    std::shared_ptr<int> _ref;
    std::shared_ptr<lua_State> _state;
public:
    LuaRef(const std::shared_ptr<lua_State> &state, int ref)
        : _ref(new int{ref}, detail::LuaRefDeleter{state}), _state(state) {}
    
    LuaRef(const std::shared_ptr<lua_State> &state, const std::shared_ptr<int> &ref)
    : _ref(ref), _state(state) {}

    void Push() const {
        lua_rawgeti(_state.get(), LUA_REGISTRYINDEX, *_ref);
    }
    const std::shared_ptr<lua_State>& GetState() const { return _state;}
};
}
