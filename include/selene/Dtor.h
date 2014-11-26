#pragma once

#include "BaseFun.h"

namespace sel {

template <typename T>
class Dtor : public BaseFun {
private:
    std::string _metatable_name;
    const std::shared_ptr<lua_State>& _state;
public:
    Dtor(const std::shared_ptr<lua_State> &l,
         const std::string &metatable_name)
        : _metatable_name(metatable_name), _state(l) {
        lua_pushlightuserdata(l.get(), (void *)(this));
        lua_pushcclosure(l.get(), &detail::_lua_dispatcher, 1);
        lua_setfield(l.get(), -2, "__gc");
    }

    int Apply() override {
        T *t = (T *)luaL_checkudata(_state.get(), 1, _metatable_name.c_str());
        t->~T();
        return 0;
    }
};
}
