#pragma once

#include "Class.h"
#include "exotics.h"
#include "Fun.h"
#include "Obj.h"
#include <vector>

namespace sel {
namespace detail {
    
template<typename T>
struct is_callable {
private:
    typedef char(&yes)[1];
    typedef char(&no)[2];
    
    struct Fallback { void operator()(); };
    struct Derived : T, Fallback { };
    
    template<typename U, U> struct Check;
    
    template<typename>
    static yes test(...);
    
    template<typename C>
    static no test(Check<void (Fallback::*)(), &C::operator()>*);
    
public:
    static const bool value = sizeof(test<Derived>(0)) == sizeof(yes);
};
    
template <typename T>
struct lambda_traits : public lambda_traits<decltype(&T::operator())> {};

template <typename T, typename Ret, typename... Args>
struct lambda_traits<Ret(T::*)(Args...) const> {
    using Fun = std::function<Ret(Args...)>;
};

class StateBlock;

}
class Registry {
private:
    MetatableRegistry _metatables;
    std::vector<std::unique_ptr<BaseFun>> _funs;
    std::vector<std::unique_ptr<BaseObj>> _objs;
    std::vector<std::unique_ptr<BaseClass>> _classes;
    const detail::StateBlock &_stateBlock;
public:
    Registry(detail::StateBlock &stateBlock):_stateBlock(stateBlock) {}
    ~Registry() {}

    template <typename L>
    void Register(L lambda) {
        Register((typename detail::lambda_traits<L>::Fun)(lambda));
    }

    template <typename Ret, typename... Args>
    void Register(std::function<Ret(Args...)> fun) {
        constexpr int arity = detail::_arity<Ret>::value;
        auto tmp = std::unique_ptr<BaseFun>(
            new Fun<arity, Ret, Args...>{_stateBlock, _metatables, fun});
        _funs.push_back(std::move(tmp));
    }

    template <typename Ret, typename... Args>
    void Register(Ret (*fun)(Args...)) {
        constexpr int arity = detail::_arity<Ret>::value;
        auto tmp = std::unique_ptr<BaseFun>(
            new Fun<arity, Ret, Args...>{_stateBlock, _metatables, fun});
        _funs.push_back(std::move(tmp));
    }

    template <typename T, typename... Funs>
    void Register(T &t, std::tuple<Funs...> funs) {
        Register(t, funs,
                 typename detail::_indices_builder<sizeof...(Funs)>::type{});
    }

    template <typename T, typename... Funs, size_t... N>
    void Register(T &t, std::tuple<Funs...> funs, detail::_indices<N...>) {
        RegisterObj(t, std::get<N>(funs)...);
    }

    template <typename T, typename... Funs>
    void RegisterObj(T &t, Funs... funs) {
        auto tmp = std::unique_ptr<BaseObj>(
            new Obj<T, Funs...>{_stateBlock, &t, funs...});
        _objs.push_back(std::move(tmp));
    }

    template <typename T, typename... CtorArgs, typename... Funs, size_t... N>
    void RegisterClass(const std::string &name, std::tuple<Funs...> funs,
                       detail::_indices<N...>) {
        RegisterClassWorker<T, CtorArgs...>(name, std::get<N>(funs)...);
    }

    template <typename T, typename... CtorArgs, typename... Funs>
    void RegisterClassWorker(const std::string &name, Funs... funs) {
        auto tmp = std::unique_ptr<BaseClass>(
            new Class<T, Ctor<T, CtorArgs...>, Funs...>
            {_stateBlock, _metatables, name, funs...});
        _classes.push_back(std::move(tmp));
    }
};

namespace detail {
inline StateBlock::StateBlock(lua_State *state, bool owned):_state(state),_owned(owned) {
    _registry = new Registry(*this);
}
inline StateBlock::~StateBlock() {
    if(_owned) {
        lua_gc(_state, LUA_GCCOLLECT, 0);
        lua_close(_state);
    }
    delete _registry;
}
}

}
