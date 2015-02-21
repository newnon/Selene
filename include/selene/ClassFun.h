#pragma once

#include "BaseFun.h"
#include <string>

namespace sel {

template <int N, typename T, typename Ret, typename... Args>
class ClassFun : public BaseFun {
private:
    using _fun_type = std::function<Ret(T*, Args...)>;
    _fun_type _fun;
    std::string _name;
    std::string _metatable_name;
    const std::weak_ptr<lua_State> _state;

    T *_get(const std::shared_ptr<lua_State> &state) {
        T *ret = (T *)luaL_checkudata(state.get(), 1, _metatable_name.c_str());
        lua_remove(state.get(), 1);
        return ret;
    }

public:
    ClassFun(const std::shared_ptr<lua_State> &l,
             const std::string &name,
             const std::string &metatable_name,
             Ret(*fun)(Args...))
        : ClassFun(l, name, _fun_type{fun}) {}

    ClassFun(const std::shared_ptr<lua_State> &l,
             const std::string &name,
             const std::string &metatable_name,
             _fun_type fun)
        : _fun(fun), _name(name), _metatable_name(metatable_name), _state(l) {
        lua_pushlightuserdata(l.get(), (void *)static_cast<BaseFun *>(this));
        lua_pushcclosure(l.get(), &detail::_lua_dispatcher, 1);
        lua_setfield(l.get(), -2, name.c_str());
    }

    int Apply() override {
        std::tuple<T*> t = std::make_tuple(_get(_state.lock()));
        std::tuple<Args...> args = detail::_get_args<Args...>((_state.lock()));
        std::tuple<T*, Args...> pack = std::tuple_cat(t, args);
        Ret value = detail::_lift(_fun, pack);
        detail::_push(_state, std::forward<Ret>(value));
        return N;
    }
};

template <typename T, typename... Args>
class ClassFun<0, T, void, Args...> : public BaseFun {
private:
    using _fun_type = std::function<void(T*, Args...)>;
    _fun_type _fun;
    std::string _name;
    std::string _metatable_name;
    const std::weak_ptr<lua_State> _state;

    T *_get(const std::shared_ptr<lua_State> &state) {
        T *ret = (T *)luaL_checkudata(state.get(), 1, _metatable_name.c_str());
        lua_remove(state.get(), 1);
        return ret;
    }

public:
    ClassFun(const std::shared_ptr<lua_State> &l,
             const std::string &name,
             const std::string &metatable_name,
             void(*fun)(Args...))
        : ClassFun(l, name, metatable_name, _fun_type{fun}) {}

    ClassFun(const std::shared_ptr<lua_State> &l,
             const std::string &name,
             const std::string &metatable_name,
             _fun_type fun)
        : _fun(fun), _name(name), _metatable_name(metatable_name), _state(l) {
        lua_pushlightuserdata(l.get(), (void *)static_cast<BaseFun *>(this));
        lua_pushcclosure(l.get(), &detail::_lua_dispatcher, 1);
        lua_setfield(l.get(), -2, name.c_str());
    }

    int Apply() override {
        std::tuple<T*> t = std::make_tuple(_get(_state.lock()));
        std::tuple<Args...> args = detail::_get_args<Args...>(_state.lock());
        std::tuple<T*, Args...> pack = std::tuple_cat(t, args);
        detail::_lift(_fun, pack);
        return 0;
    }
};
}
