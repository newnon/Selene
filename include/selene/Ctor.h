#pragma once

#include "BaseFun.h"

namespace sel {

template <typename T, typename... Args>
class Ctor : public BaseFun {
private:
    using _ctor_type = std::function<void(const std::shared_ptr<lua_State> &, Args...)>;
    _ctor_type _ctor;
    const std::weak_ptr<lua_State> _state;
public:
    Ctor(const std::shared_ptr<lua_State> &l,
         const std::string &metatable_name):_state(l) {
        _ctor = [metatable_name](const std::shared_ptr<lua_State> &state, Args... args) {
            void *addr = lua_newuserdata(state.get(), sizeof(T));
            new(addr) T(args...);
            luaL_setmetatable(state.get(), metatable_name.c_str());
        };
        lua_pushlightuserdata(l.get(), (void *)static_cast<BaseFun *>(this));
        lua_pushcclosure(l.get(), &detail::_lua_dispatcher, 1);
        lua_setfield(l.get(), -2, "new");
    }

    int Apply() override {
        std::tuple<Args...> args = detail::_get_args<Args...>(_state.lock());
        auto pack = std::tuple_cat(std::make_tuple(std::cref(_state.lock())), args);
        detail::_lift(_ctor, pack);
        // The constructor will leave a single userdata entry on the stack
        return 1;
    }
};
}
