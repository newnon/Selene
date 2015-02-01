#pragma once

#include <functional>
#include "LuaRef.h"
#include <memory>
#include "primitives.h"
#include "util.h"

namespace sel {
/*
 * Similar to an std::function but refers to a lua function
 */
template <class>
class function {};

template <typename R, typename... Args>
class function<R(Args...)> {
private:
    LuaRef _ref;
public:
    function(int ref, const std::shared_ptr<lua_State> &state) : _ref(state, ref) {}
    function(const LuaRef &ref) : _ref(ref) {}

    R operator()(Args... args) {
        int handler_index = SetErrorHandler(_ref.GetState().get());
        _ref.Push();
        detail::_push_n(_ref.GetState(), args...);
        constexpr int num_args = sizeof...(Args);
        lua_pcall(_ref.GetState().get(), num_args, 1, handler_index);
        lua_remove(_ref.GetState().get(), handler_index);
        R ret = detail::_pop(detail::_id<R>{}, _ref.GetState()).get();
        lua_settop(_ref.GetState().get(), 0);
        return ret;
    }

    void Push() {
        _ref.Push();
    }
};

template <typename... Args>
class function<void(Args...)> {
private:
    LuaRef _ref;
public:
    function(int ref, const std::shared_ptr<lua_State> &state) : _ref(state, ref) {}
    function(const LuaRef &ref) : _ref(ref) {}

    void operator()(Args... args) {
        int handler_index = SetErrorHandler(_ref.GetState().get());
        _ref.Push();
        detail::_push_n(_ref.GetState(), args...);
        constexpr int num_args = sizeof...(Args);
        lua_pcall(_ref.GetState().get(), num_args, 1, handler_index);
        lua_remove(_ref.GetState().get(), handler_index);
        lua_settop(_ref.GetState().get(), 0);
    }

    void Push() {
        _ref.Push();
    }
};

// Specialization for multireturn types
template <typename... R, typename... Args>
class function<std::tuple<R...>(Args...)> {
private:
    LuaRef _ref;
public:
    function(int ref, const std::shared_ptr<lua_State> &state) : _ref(state, ref) {}
    function(const LuaRef &ref) : _ref(ref) {}

    std::tuple<R...> operator()(Args... args) {
        int handler_index = SetErrorHandler(_ref.GetState().get());
        _ref.Push();
        detail::_push_n(_ref.GetState(), args...);
        constexpr int num_args = sizeof...(Args);
        constexpr int num_ret = sizeof...(R);
        lua_pcall(_ref.GetState().get(), num_args, num_ret, handler_index);
        lua_remove(_ref.GetState().get(), handler_index);
        return detail::_pop_n_reset<R...>(_ref.GetState());
    }

    void Push() {
        _ref.Push();
    }
};
}
