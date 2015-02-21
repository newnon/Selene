#pragma once

#include "BaseFun.h"

namespace sel {

template <typename T>
class Dtor : public BaseFun {
private:
    std::string _metatable_name;
    const detail::StateBlock &_state;
public:
    Dtor(const detail::StateBlock &l,
         const std::string &metatable_name)
        : _metatable_name(metatable_name), _state(l) {
        lua_pushlightuserdata(l.GetState(), (void *)(this));
        lua_pushcclosure(l.GetState(), &detail::_lua_dispatcher, 1);
        lua_setfield(l.GetState(), -2, "__gc");
    }

    int Apply() override {
        T *t = (T *)luaL_checkudata(_state.GetState(), 1, _metatable_name.c_str());
        t->~T();
        return 0;
    }
};
}
