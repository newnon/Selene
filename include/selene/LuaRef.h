#pragma once

#include <memory>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace sel {

class Registry;

namespace detail {

class StateBlock: public std::enable_shared_from_this<StateBlock>
{
public:
    StateBlock(lua_State *state, bool owned);
    ~StateBlock();
    inline lua_State *GetState() const {
        return _state;
    }
    inline Registry *GetRegistry() const {
        return _registry;
    }
private:
    bool _owned;
    lua_State *_state;
    Registry *_registry;
};
    
class LuaRefDeleter {
private:
    const detail::StateBlock &_state;
public:
    LuaRefDeleter(const detail::StateBlock &state) : _state{state} {}
    void operator()(int *ref) const {
        luaL_unref(_state.GetState(), LUA_REGISTRYINDEX, *ref);
        delete ref;
    }
};
}
class LuaRef {
private:
    std::shared_ptr<int> _ref;
    std::shared_ptr<const detail::StateBlock> _state;
public:
    LuaRef(const detail::StateBlock &state, int ref)
        : _ref(new int{ref}, detail::LuaRefDeleter{state}), _state(state.shared_from_this()) {}
    
    LuaRef(const detail::StateBlock &state, const std::shared_ptr<int> &ref)
    : _ref(ref), _state(state.shared_from_this()) {}

    void Push() const {
        lua_rawgeti(_state->GetState(), LUA_REGISTRYINDEX, *_ref);
    }
    inline const std::shared_ptr<const detail::StateBlock>& GetStateBlock() const { return _state;}
};
}
