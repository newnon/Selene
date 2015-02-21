#pragma once

#include "BaseFun.h"
#include <string>

namespace sel {

template <int N, typename Ret, typename... Args>
class ObjFun : public BaseFun {
private:
    using _fun_type = std::function<Ret(Args...)>;
    _fun_type _fun;
    const detail::StateBlock &_state;

public:
    ObjFun(const detail::StateBlock &l,
           const std::string &name,
           Ret(*fun)(Args...))
        : ObjFun(l, name, _fun_type{fun}) {}

    ObjFun(const detail::StateBlock &l,
           const std::string &name,
           _fun_type fun) : _fun(fun), _state(l) {
        lua_pushlightuserdata(l.GetState(), (void *)static_cast<BaseFun *>(this));
        lua_pushcclosure(l.GetState(), &detail::_lua_dispatcher, 1);
        lua_setfield(l.GetState(), -2, name.c_str());
    }

    // Each application of a function receives a new Lua context so
    // this argument is necessary.
    int Apply() override {
        std::tuple<Args...> args = detail::_get_args<Args...>(_state);
        Ret value = detail::_lift(_fun, args);
        detail::_push(_state, std::forward<Ret>(value));
        return N;
    }
};

template <typename... Args>
class ObjFun<0, void, Args...> : public BaseFun {
private:
    using _fun_type = std::function<void(Args...)>;
    _fun_type _fun;
    const detail::StateBlock &_state;

public:
    ObjFun(const detail::StateBlock &l,
           const std::string &name,
           void(*fun)(Args...))
        : ObjFun(l, name, _fun_type{fun}) {}

    ObjFun(const detail::StateBlock &l,
           const std::string &name,
           _fun_type fun) : _fun(fun), _state(l) {
        lua_pushlightuserdata(l.GetState(), (void *)static_cast<BaseFun *>(this));
        lua_pushcclosure(l.GetState(), &detail::_lua_dispatcher, 1);
        lua_setfield(l.GetState(), -2, name.c_str());
    }

    // Each application of a function receives a new Lua context so
    // this argument is necessary.
    int Apply() override {
        std::tuple<Args...> args = detail::_get_args<Args...>(_state);
        detail::_lift(_fun, args);
        return 0;
    }
};
}
