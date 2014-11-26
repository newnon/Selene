#pragma once

#include "exotics.h"
#include <functional>
#include "Registry.h"
#include "Value.h"
#include <string>
#include <tuple>
#include <vector>

#include "util.h"

namespace sel {
class State;
class Selector {
    friend class State;
private:
    std::shared_ptr<lua_State> _state;
    Registry &_registry;
    std::string _name;
    using Fun = std::function<void()>;
    using PFun = std::function<void(Fun)>;

    // Traverses the structure up to this element
    std::vector<Fun> _traversal;

    // Pushes this element to the stack
    Fun _get;
    // Sets this element from a function that pushes a value to the
    // stack.
    PFun _put;

    // Functor is stored when the () operator is invoked. The argument
    // is used to indicate how many return values are expected
    using Functor = std::function<void(int)>;
    mutable Functor _functor;

    Selector(const std::shared_ptr<lua_State> &s, Registry &r, const std::string &name,
             std::vector<Fun> traversal, Fun get, PFun put)
        : _state(s), _registry(r), _name(name), _traversal{traversal},
          _get(get), _put(put) {}
    
    Selector(const std::shared_ptr<lua_State> &s, Registry &r, const std::string &name)
    : _state(s), _registry(r), _name(name) {
        _get = [this, name]() {
            lua_getglobal(_state.get(), name.c_str());
        };
        _put = [this, name](Fun fun) {
            fun();
            lua_setglobal(_state.get(), name.c_str());
        };
    }

    template<size_t SIZE>
    Selector(const std::shared_ptr<lua_State> &s, Registry &r, const char (&name)[SIZE])
        : _state(s), _registry(r), _name(name) {
        _get = [this, name]() {
            lua_getglobal(_state.get(), name);
        };
        _put = [this, name](Fun fun) {
            fun();
            lua_setglobal(_state.get(), name);
        };
    }

    void _check_create_table() const {
        _traverse();
        _get();
        if (lua_istable(_state.get(), -1) == 0 ) { // not table
            lua_pop(_state.get(), 1); // flush the stack
            auto put = [this]() {
                lua_newtable(_state.get());
            };
            _put(put);
        } else {
            lua_pop(_state.get(), 1);
        }
    }

    void _traverse() const {
        for (auto &fun : _traversal) {
            fun();
        }
    }
    
    template <typename T, typename std::enable_if<std::is_class<T>::value>::type* = nullptr, typename std::enable_if<detail::is_callable<T>::value>::type* = nullptr>
    void _put_val(const T &functor) {
        _traverse();
        auto push = [this, functor]() {
            _registry.Register(functor);
        };
        _put(push);
        lua_settop(_state.get(), 0);
    }
    
    template <typename Ret, typename... Args>
    void _put_val(Ret (*fun)(Args...)) {
        _traverse();
        auto push = [this, fun]() {
            _registry.Register(fun);
        };
        _put(push);
    }
    
    template <typename Ret, typename... Args>
    void _put_val(const std::function<Ret(Args...)> &fun) {
        _traverse();
        auto push = [this, fun]() {
            _registry.Register(fun);
        };
        _put(push);
    }
    
    template <typename T, typename std::enable_if<std::is_integral<T>::value >::type* = 0>
    void _put_val(T i) {
        _traverse();
        auto push = [this, i]() {
            detail::_push(_state.get(), i);
        };
        _put(push);
        lua_settop(_state.get(), 0);
    }
    
    void _put_val(const std::string &str) {
        _traverse();
        auto push = [this, str]() {
            detail::_push(_state, str);
        };
        _put(push);
        lua_settop(_state.get(), 0);
    }
    
    void _put_val(const Value &value) {
        _traverse();
        auto push = [this, value]() {
            detail::_push(_state, value);
        };
        _put(push);
        lua_settop(_state.get(), 0);
    }
    
    template <typename T>
    T _get_val() const {
        _traverse();
        _get();
        if (_functor) {
            _functor(1);
            _functor = nullptr;
        }
        auto ret = detail::_pop(detail::_id<T>{}, _state);
        lua_settop(_state.get(), 0);
        return ret;
    }
    
public:
    enum class Type
    {
        None = LUA_TNONE,
        Nil = LUA_TNIL,
        Boolean = LUA_TBOOLEAN,
        LightUserData = LUA_TLIGHTUSERDATA,
        Number = LUA_TNUMBER,
        String = LUA_TSTRING,
        Table = LUA_TTABLE,
        Function = LUA_TFUNCTION,
        UserData = LUA_TUSERDATA,
        Thread = LUA_TTHREAD,
    };
    
    Type getType() const {
        _traverse();
        _get();
        Type ret = static_cast<Type>(lua_type(_state.get(), -1));
        return ret;
    }
    
    bool is(Type type) const
    {
        return getType() == type;
    }

    Selector(const Selector &other)
        : _state(other._state),
          _registry(other._registry),
          _name{other._name},
          _traversal{other._traversal},
          _get{other._get},
          _put{other._put},
          _functor(other._functor)
        {}

    ~Selector() {
        // If there is a functor is not empty, execute it and collect no args
        if (_functor) {
            _traverse();
            _get();
            _functor(0);
        }
        lua_settop(_state.get(), 0);
    }

    // Allow automatic casting when used in comparisons
    bool operator==(Selector &other) = delete;

    template <typename... Args>
    const Selector operator()(Args... args) const {
        auto tuple_args = std::make_tuple(std::forward<Args>(args)...);
        constexpr int num_args = sizeof...(Args);
        Selector copy{*this};
        copy._functor = [this, tuple_args, num_args](int num_ret) {
            // install handler, and swap(handler, function) on lua stack
            int handler_index = SetErrorHandler(_state.get());
            int func_index = handler_index - 1;
#if LUA_VERSION_NUM >= 502
            lua_pushvalue(_state.get(), func_index);
            lua_copy(_state.get(), handler_index, func_index);
            lua_replace(_state.get(), handler_index);
#else
            lua_pushvalue(_state.get(), func_index);
            lua_push_value(_state.get(), handler_index);
            lua_replace(_state.get(), func_index);
            lua_replace(_state.get(), handler_index);
#endif
            // call lua function with error handler
            detail::_push(_state, tuple_args);
            lua_pcall(_state.get(), num_args, num_ret, handler_index - 1);

            // remove error handler
            lua_remove(_state.get(), handler_index - 1);
        };
        return copy;
    }
    
    template <typename T, typename std::enable_if<std::is_class<T>::value>::type* = nullptr, typename std::enable_if<detail::is_callable<T>::value>::type* = nullptr>
    void operator=(const T &functor) {
        _put_val(functor);
    }
    
    template <typename Ret, typename... Args>
    void operator=(Ret (*fun)(Args...)) {
        _put_val(fun);
    }
    
    template <typename Ret, typename... Args>
    void operator=(const std::function<Ret(Args...)> &fun) {
        _put_val(fun);
    }
    
    template <typename T, typename std::enable_if<std::is_integral<T>::value >::type* = 0>
    void operator=(T i) {
        _put_val(i);
    }
    
    void operator=(const std::string &str) {
        _put_val(str);
    }
    
    void operator=(const Value &value) {
        _put_val(value);
    }
    void operator=(const char *s) {
        _put_val(std::string(s));
    }

    template <typename T, typename... Funs>
    void SetObj(T &t, Funs... funs) {
        _traverse();
        auto fun_tuple = std::make_tuple(funs...);
        auto push = [this, &t, &fun_tuple]() {
            _registry.Register(t, fun_tuple);
        };
        _put(push);
        lua_settop(_state.get(), 0);
    }

    template <typename T, typename... Args, typename... Funs>
    void SetClass(Funs... funs) {
        _traverse();
        auto fun_tuple = std::make_tuple(funs...);
        auto push = [this, &fun_tuple]() {
            typename detail::_indices_builder<sizeof...(Funs)>::type d;
            _registry.RegisterClass<T, Args...>(_name, fun_tuple, d);
        };
        _put(push);
        lua_settop(_state.get(), 0);
    }

    template <typename... Ret>
    std::tuple<Ret...> GetTuple() const {
        _traverse();
        _get();
        _functor(sizeof...(Ret));
        return detail::_pop_n_reset<Ret...>(_state.get());
    }

    template <typename T>
    operator T&() const {
        return *_get_val<T*>();
    }

    template <typename T>
    operator T*() const {
        return _get_val<T*>();
    }

    operator bool() const {
        return _get_val<bool>();
    }

    operator int() const {
        return _get_val<int>();
    }

    operator unsigned int() const {
        return _get_val<unsigned int>();
    }

    operator long() const {
        return _get_val<long>();
    }

    operator unsigned long() const {
        return _get_val<unsigned long>();
    }

    operator float() const {
        return _get_val<float>();
    }
    
    operator double() const {
        return _get_val<double>();
    }

    operator std::string() const {
        return _get_val<std::string>();
    }

    operator Value() const {
        return _get_val<Value>();
    }

    template <typename R, typename... Args>
    operator sel::function<R(Args...)>() {
        _traverse();
        _get();
        if (_functor) {
            _functor(1);
            _functor = nullptr;
        }
        auto ret = detail::_pop(detail::_id<sel::function<R(Args...)>>{},
                                _state);
        lua_settop(_state.get(), 0);
        return ret;
    }
    
    std::vector<std::pair<const Selector,Selector>> GetChildren() const {
        _traverse();
        _get();
        
        std::vector<std::pair<const Selector,Selector>> ret;
        if(lua_type(_state.get(), -1) != LUA_TTABLE)
            return ret;
        
        auto traversal = _traversal;
        traversal.push_back(_get);
        std::vector<std::string> names;
        std::vector<Fun> get1;
        std::vector<Fun> get2;
        std::vector<PFun> put1;
        std::vector<PFun> put2;
        
        lua_pushvalue(_state.get(), -1);
        // stack now contains: -1 => table
        lua_pushnil(_state.get());
        // stack now contains: -1 => nil; -2 => table
        int counter = 0;
        while (lua_next(_state.get(), -2))
        {
            // stack now contains: -1 => value; -2 => key; -3 => table
            names.push_back(_name + "." + std::to_string(counter));
            get1.push_back([this, counter]() {
                lua_pushvalue(_state.get(), -1);
                lua_pushnil(_state.get());
                lua_pushnil(_state.get());
                for(int i=0;i<counter+1;++i)
                {
                    lua_pop(_state.get(), 1);
                    lua_next(_state.get(), -2);
                }
                lua_pushvalue(_state.get(), -2);
            });
            put1.push_back([this, counter](Fun fun) {
            });
            
            switch(lua_type(_state.get(), -2))
            {
                case LUA_TNUMBER:
                    {
                        lua_Number index = lua_tonumber(_state.get(), -2);
                        get2.push_back([this, index]() {
                            lua_pushnumber(_state.get(), index);
                            lua_gettable(_state.get(), -2);
                        });
                        put2.push_back([this, index](Fun fun) {
                            lua_pushnumber(_state.get(), index);
                            fun();
                            lua_settable(_state.get(), -3);
                            lua_pop(_state.get(), 1);
                        });
                    }
                    break;
                case LUA_TSTRING:
                    {
                        std::string name = lua_tostring(_state.get(), -2);
                        get2.push_back([this, name]() {
                            lua_getfield(_state.get(), -1, name.c_str());
                        });
                        put2.push_back([this, name](Fun fun) {
                            fun();
                            lua_setfield(_state.get(), -2, name.c_str());
                            lua_pop(_state.get(), 1);
                        });
                    }
                    break;
                default:
                    get2.push_back([this, counter]() {
                        lua_pushvalue(_state.get(), -1);
                        lua_pushnil(_state.get());
                        lua_pushnil(_state.get());
                        for(int i=0;i<counter+1;++i)
                        {
                            lua_pop(_state.get(), 1);
                            lua_next(_state.get(), -2);
                        }
                    });
                    put2.push_back([this, counter](Fun fun) {
                        
                        lua_pushvalue(_state.get(), -1);
                        lua_pushnil(_state.get());
                        lua_pushnil(_state.get());
                        for(int i=0;i<counter+1;++i)
                        {
                            lua_pop(_state.get(), 1);
                            lua_next(_state.get(), -2);
                        }
                        lua_pushvalue(_state.get(), -2);
                        fun();
                        lua_settable(_state.get(), -6);
                        lua_pop(_state.get(), 1);
                    });
                    break;
            }
            
            ++counter;
            
            // pop value, leaving original key
            lua_pop(_state.get(), 1);
            // stack now contains: -1 => key; -2 => table
        }
        // stack now contains: -1 => table (when lua_next returns 0 it pops the key
        // but does not push anything.)
        // Pop table
        lua_pop(_state.get(), 1);
        
        for(size_t i=0;i<names.size();++i)
        {
            ret.emplace_back(Selector{_state, _registry, names[i], traversal, get1[i], put1[i]}, Selector{_state, _registry, names[i], traversal, get2[i], put2[i]});
        }
        return ret;
    }

    // Chaining operators. If the selector is an rvalue, modify in
    // place. Otherwise, create a new Selector and return it.
    template<size_t SIZE>
    Selector&& operator[](const char (&name)[SIZE]) && {
        _name += std::string(".") + name;
        _check_create_table();
        _traversal.push_back(_get);
        _get = [this, name]() {
            lua_getfield(_state.get(), -1, name);
        };
        _put = [this, name](Fun fun) {
            fun();
            lua_setfield(_state.get(), -2, name);
            lua_pop(_state.get(), 1);
        };
        return std::move(*this);
    }
    Selector&& operator[](const std::string &name) && {
        _name += std::string(".") + name;
        _check_create_table();
        _traversal.push_back(_get);
        _get = [this, name]() {
            lua_getfield(_state.get(), -1, name.c_str());
        };
        _put = [this, name](Fun fun) {
            fun();
            lua_setfield(_state.get(), -2, name.c_str());
            lua_pop(_state.get(), 1);
        };
        return std::move(*this);
    }
    Selector&& operator[](const double index) && {
        _name += std::string(".") + std::to_string(index);
        _check_create_table();
        _traversal.push_back(_get);
        _get = [this, index]() {
            lua_pushnumber(_state.get(), index);
            lua_gettable(_state.get(), -2);
        };
        _put = [this, index](Fun fun) {
            lua_pushnumber(_state.get(), index);
            fun();
            lua_settable(_state.get(), -3);
            lua_pop(_state.get(), 1);
        };
        return std::move(*this);
    }
    template<size_t SIZE>
    Selector operator[](const char (&name)[SIZE]) const & {
        auto n = _name + "." + name;
        _check_create_table();
        auto traversal = _traversal;
        traversal.push_back(_get);
        Fun get = [this, name]() {
            lua_getfield(_state.get(), -1, name);
        };
        PFun put = [this, name](Fun fun) {
            fun();
            lua_setfield(_state.get(), -2, name);
            lua_pop(_state.get(), 1);
        };
        return Selector{_state, _registry, n, traversal, get, put};
    }
    Selector operator[](const std::string &name) const & {
        auto n = _name + "." + name;
        _check_create_table();
        auto traversal = _traversal;
        traversal.push_back(_get);
        Fun get = [this, name]() {
            lua_getfield(_state.get(), -1, name.c_str());
        };
        PFun put = [this, name](Fun fun) {
            fun();
            lua_setfield(_state.get(), -2, name.c_str());
            lua_pop(_state.get(), 1);
        };
        return Selector{_state, _registry, n, traversal, get, put};
    }
    Selector operator[](const double index) const & {
        auto name = _name + "." + std::to_string(index);
        _check_create_table();
        auto traversal = _traversal;
        traversal.push_back(_get);
        Fun get = [this, index]() {
            lua_pushnumber(_state.get(), index);
            lua_gettable(_state.get(), -2);
        };
        PFun put = [this, index](Fun fun) {
            lua_pushnumber(_state.get(), index);
            fun();
            lua_settable(_state.get(), -3);
            lua_pop(_state.get(), 1);
        };
        return Selector{_state, _registry, name, traversal, get, put};
    }

    friend bool operator==(const Selector &, const char *);

    friend bool operator==(const char *, const Selector &);

private:
    std::string ToString() const {
        _traverse();
        _get();
        if (_functor) {
            _functor(1);
            _functor = nullptr;
        }
        auto ret =  detail::_pop(detail::_id<std::string>{}, _state);
        lua_settop(_state.get(), 0);
        return ret;
    }
};

inline bool operator==(const Selector &s, const char *c) {
    return std::string{c} == s.ToString();
}

inline bool operator==(const char *c, const Selector &s) {
    return std::string{c} == s.ToString();
}

template <typename T>
inline bool operator==(const Selector &s, T&& t) {
    return T(s) == t;
}

template <typename T>
inline bool operator==(T &&t, const Selector &s) {
    return T(s) == t;
}

}
