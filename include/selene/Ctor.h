#pragma once

#include "BaseFun.h"

namespace sel {

template <typename T, typename... Args>
class Ctor : public BaseFun {
private:
    using _ctor_type = std::function<void(const detail::StateBlock &, Args...)>;
    _ctor_type _ctor;
    const detail::StateBlock &_state;
public:
    Ctor(const detail::StateBlock &l,
         const std::string &metatable_name):_state(l) {
        _ctor = [metatable_name](const detail::StateBlock &state, Args... args) {
            void *addr = lua_newuserdata(state.GetState(), sizeof(T));
            new(addr) T(args...);
            luaL_setmetatable(state.GetState(), metatable_name.c_str());
        };
        lua_pushlightuserdata(l.GetState(), (void *)static_cast<BaseFun *>(this));
        lua_pushcclosure(l.GetState(), &detail::_lua_dispatcher, 1);
        lua_setfield(l.GetState(), -2, "new");
    }

    int Apply() override {
        std::tuple<Args...> args = detail::_get_args<Args...>(_state);
        auto pack = std::tuple_cat(std::make_tuple(std::cref(_state)), args);
        detail::_lift(_ctor, pack);
        // The constructor will leave a single userdata entry on the stack
        return 1;
    }
};
}
