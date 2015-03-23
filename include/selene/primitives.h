#pragma once

#include <string>
#include <map>
#include <vector>
#include "traits.h"
#include "MetatableRegistry.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

/* The purpose of this header is to handle pushing and retrieving
 * primitives from the stack
 */

namespace sel {
    
class LuaValue;

enum class LuaType
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

class Value {
public:
    
    typedef LuaType Type;
    
    Value();
    Value(const Value &other);
    Value(Value &&other);
    
    explicit Value(bool value);
    explicit Value(void* value);
    explicit Value(short value);
    explicit Value(unsigned short value);
    explicit Value(int value);
    explicit Value(unsigned int value);
    explicit Value(long value);
    explicit Value(unsigned long value);
    explicit Value(long long value);
    explicit Value(unsigned long long value);
    explicit Value(float value);
    explicit Value(double value);
    explicit Value(long double value);
    explicit Value(const char* value);
    explicit Value(const std::string &value);
    explicit Value(std::string &&value);
    explicit Value(const std::map<Value, Value> &value);
    explicit Value(std::map<Value, Value> &&value);
    explicit Value(const std::vector<Value> &value);
    explicit Value(std::vector<Value> &&value);
    template <typename Ret, typename... Args>
    explicit Value(const std::function<Ret(Args...)> &fun);
    template <typename Ret, typename... Args>
    explicit Value(Ret (*value)(Args...));
    explicit Value(const LuaRef& ref);
    explicit Value(LuaRef&& ref);
    explicit Value(const std::vector<unsigned char> &value);
    
    inline bool Is(Type type) const { return this->type() == type; }
    inline Type type() const;
    template <typename T>
    const T cast() const;
    
    Value& operator=(const Value &other);
    Value& operator=(Value &&other);
    Value& operator=(bool value);
    Value& operator=(void* value);
    Value& operator=(short value);
    Value& operator=(unsigned short value);
    Value& operator=(int value);
    Value& operator=(unsigned int value);
    Value& operator=(long value);
    Value& operator=(unsigned long value);
    Value& operator=(long long value);
    Value& operator=(unsigned long long value);
    Value& operator=(float value);
    Value& operator=(double value);
    Value& operator=(long double value);
    Value& operator=(const char* value);
    Value& operator=(const std::string &value);
    Value& operator=(std::string &&value);
    Value& operator=(const std::map<Value, Value> &value);
    Value& operator=(std::map<Value, Value> &&value);
    Value& operator=(const std::vector<Value> &value);
    Value& operator=(std::vector<Value> &&value);
    template <typename Ret, typename... Args>
    Value& operator=(const std::function<Ret(Args...)> &fun);
    template <typename Ret, typename... Args>
    Value& operator=(Ret (*value)(Args...));
    Value& operator=(const LuaRef& ref);
    Value& operator=(const std::vector<unsigned char> &value);

    
    template <typename T>
    const Value& operator[] (const T &value) const;
    
    bool bool_value() const;
    lua_Number number_value() const;
    short short_value() const;
    int int_value() const;
    long long_value() const;
    float float_value() const;
    double double_value() const;
    const std::string& string_value() const;
    const std::map<Value, Value>& table_value() const;
    template <typename Ret, typename... Args>
    const std::function<Ret(Args...)> function_value() const;
    
    explicit operator bool() const;
    explicit operator short() const;
    explicit operator unsigned short() const;
    explicit operator int() const;
    explicit operator unsigned int() const;
    explicit operator long() const;
    explicit operator unsigned long() const;
    explicit operator long long() const;
    explicit operator unsigned long long() const;
    explicit operator float() const;
    explicit operator double() const;
    explicit operator long double() const;
    explicit operator const std::string&() const;
    explicit operator const std::map<Value, Value>&() const;
    template <typename... Args>
    Value operator()(Args... args) const;
    bool operator <(const Value &other) const;
    void push(const detail::StateBlock &l) const;
private:
    std::unique_ptr<LuaValue> _value;
};

class Registry;
    
namespace detail {

template <typename T>
struct is_primitive {
    static constexpr bool value = false;
};
template <>
struct is_primitive<char> {
    static constexpr bool value = true;
};
template <>
struct is_primitive<short> {
    static constexpr bool value = true;
};
template <>
struct is_primitive<unsigned short> {
    static constexpr bool value = true;
};
template <>
struct is_primitive<int> {
    static constexpr bool value = true;
};
template <>
struct is_primitive<unsigned int> {
    static constexpr bool value = true;
};
template <>
struct is_primitive<long> {
    static constexpr bool value = true;
};
template <>
struct is_primitive<unsigned long> {
    static constexpr bool value = true;
};
template <>
struct is_primitive<long long> {
    static constexpr bool value = true;
};
template <>
struct is_primitive<unsigned long long> {
    static constexpr bool value = true;
};
template <>
struct is_primitive<bool> {
    static constexpr bool value = true;
};
template <>
struct is_primitive<float> {
    static constexpr bool result = true;
};
template <>
struct is_primitive<double> {
    static constexpr bool result = true;
};
template <>
struct is_primitive<long double> {
    static constexpr bool result = true;
};
template <>
struct is_primitive<std::string> {
    static constexpr bool value = true;
};

/* getters */
template <typename T>
inline T* _get(_id<T*>, const detail::StateBlock &l, const int index) {
    auto t = lua_topointer(l.GetState(), index);
    return (T*)(t);
}

inline bool _get(_id<bool>, const detail::StateBlock &l, const int index) {
    return lua_toboolean(l.GetState(), index) != 0;
}

inline char _get(_id<char>, const detail::StateBlock &l, const int index) {
    return *lua_tostring(l.GetState(), index);
}

inline int _get(_id<int>, const detail::StateBlock &l, const int index) {
    return static_cast<int>(lua_tointeger(l.GetState(), index));
}

inline unsigned int _get(_id<unsigned int>, const detail::StateBlock &l, const int index) {
#if LUA_VERSION_NUM >= 503
    return static_cast<unsigned>(lua_tointeger(l.GetState(), index));
#elif LUA_VERSION_NUM >= 502
    return static_cast<unsigned>(lua_tounsigned(l.GetState(), index));
#else
    return static_cast<unsigned>(lua_tointeger(l.GetState(), index));
#endif
}

inline long _get(_id<long>, const detail::StateBlock &l, const int index) {
  return static_cast<long>(lua_tointeger(l.GetState(), index));
}

inline unsigned long _get(_id<unsigned long>, const detail::StateBlock &l, const int index) {
#if LUA_VERSION_NUM >= 503
    return static_cast<unsigned long>(lua_tointeger(l.GetState(), index));
#elif LUA_VERSION_NUM >= 502
    return static_cast<unsigned long>(lua_tounsigned(l.GetState(), index));
#else
    return static_cast<unsigned long>(lua_tointeger(l.GetState(), index));
#endif
}
    
inline long long _get(_id<long long>, const detail::StateBlock &l, const int index) {
    return lua_tointeger(l.GetState(), index);
}

inline unsigned long long _get(_id<unsigned long long>, const detail::StateBlock &l, const int index) {
#if LUA_VERSION_NUM >= 503
    return static_cast<unsigned long long>(lua_tointeger(l.GetState(), index));
#elif LUA_VERSION_NUM >= 502
    return static_cast<unsigned long long>(lua_tounsigned(l.GetState(), index));
#else
    return static_cast<unsigned long long>(lua_tointeger(l.GetState(), index));
#endif
}

inline float _get(_id<float>, const detail::StateBlock &l, const int index) {
    return lua_tonumber(l.GetState(), index);
}
    
inline double _get(_id<double>, const detail::StateBlock &l, const int index) {
    return lua_tonumber(l.GetState(), index);
}

inline std::string _get(_id<std::string>, const detail::StateBlock &l, const int index) {
    size_t size;
    const char *buff = lua_tolstring(l.GetState(), index, &size);
    return std::string{buff, size};
}
    
inline Value _get(_id<Value>, const detail::StateBlock &l, const int index);

template <typename T>
inline T* _check_get(_id<T*>, const detail::StateBlock &l, const int index) {
    return (T *)lua_topointer(l.GetState(), index);
};

template <typename T>
inline T& _check_get(_id<T&>, const detail::StateBlock &l, const int index) {
    static_assert(!is_primitive<T>::value,
                  "Reference types must not be primitives.");
    return *(T *)lua_topointer(l.GetState(), index);
};

template <typename T>
inline T _check_get(_id<T&&>, const detail::StateBlock &l, const int index) {
    return _check_get(_id<T>{}, l.GetState(), index);
};

inline int _check_get(_id<int>, const detail::StateBlock &l, const int index) {
    return luaL_checkinteger(l.GetState(), index);
};

inline unsigned int _check_get(_id<unsigned int>, const detail::StateBlock &l, const int index) {
#if LUA_VERSION_NUM >= 503
    return static_cast<unsigned>(luaL_checkinteger(l.GetState(), index));
#elif LUA_VERSION_NUM >= 502
    return static_cast<unsigned>(luaL_checkunsigned(l.GetState(), index));
#else
    return static_cast<unsigned>(luaL_checkint(l.GetState(), index));
#endif
}

inline long _check_get(_id<long>, const detail::StateBlock &l, const int index) {
#if LUA_VERSION_NUM >= 503
    return static_cast<long>(luaL_checkinteger(l.GetState(), index));
#else
    return luaL_checklong(l.GetState(), index);
#endif
}

inline unsigned long _check_get(_id<unsigned long>, const detail::StateBlock &l, const int index) {
#if LUA_VERSION_NUM >= 503
    return static_cast<unsigned long>(luaL_checkinteger(l.GetState(), index));
#else
    return static_cast<unsigned long>(luaL_checklong(l.GetState(), index));
#endif
}
    
inline long long _check_get(_id<long long>, const detail::StateBlock &l, const int index) {
#if LUA_VERSION_NUM >= 503
    return static_cast<long long>(luaL_checkinteger(l.GetState(), index));
#else
    return static_cast<long long>(luaL_checklong(l.GetState(), index));
#endif
}

inline unsigned long long _check_get(_id<unsigned long long>, const detail::StateBlock &l, const int index) {
#if LUA_VERSION_NUM >= 503
    return static_cast<unsigned long long>(luaL_checkinteger(l.GetState(), index));
#else
    return static_cast<unsigned long long>(luaL_checklong(l.GetState(), index));
#endif
}

inline lua_Number _check_get(_id<float>, const detail::StateBlock &l, const int index) {
    return luaL_checknumber(l.GetState(), index);
}
    
inline lua_Number _check_get(_id<double>, const detail::StateBlock &l, const int index) {
    return luaL_checknumber(l.GetState(), index);
}

inline bool _check_get(_id<bool>, const detail::StateBlock &l, const int index) {
    return lua_toboolean(l.GetState(), index) != 0;
}

inline std::string _check_get(_id<std::string>, const detail::StateBlock &l, const int index) {
    size_t size;
    const char *buff = luaL_checklstring(l.GetState(), index, &size);
    return std::string{buff, size};
}
    
inline Value _check_get(_id<Value>, const detail::StateBlock &l, const int index);

// Worker type-trait struct to _pop_n
// Popping multiple elements returns a tuple
template <std::size_t S, typename... Ts> // First template argument denotes
                                         // the sizeof(Ts...)
struct _pop_n_impl {
    using type =  std::tuple<Ts...>;

    template <std::size_t... N>
    static type worker(const detail::StateBlock &l,
                       _indices<N...>) {
        return std::make_tuple(_get(_id<Ts>{}, l, N + 1)...);
    }

    static type apply(const detail::StateBlock &l) {
        auto ret = worker(l, typename _indices_builder<S>::type());
        lua_pop(l.GetState(), S);
        return ret;
    }
};

// Popping nothing returns void
template <typename... Ts>
struct _pop_n_impl<0, Ts...> {
    using type = void;
    static type apply(const detail::StateBlock &) {}
};

// Popping one element returns an unboxed value
template <typename T>
struct _pop_n_impl<1, T> {
    using type = T;
    static type apply(const detail::StateBlock &l) {
        T ret = _get(_id<T>{}, l, -1);
        lua_pop(l.GetState(), 1);
        return ret;
    }
};

template <typename... T>
typename _pop_n_impl<sizeof...(T), T...>::type _pop_n(const detail::StateBlock &l) {
    return _pop_n_impl<sizeof...(T), T...>::apply(l);
}

template <std::size_t S, typename... Ts>
struct _pop_n_reset_impl {
    using type =  std::tuple<Ts...>;

    template <std::size_t... N>
    static type worker(const detail::StateBlock &l,
                       _indices<N...>) {
        return std::make_tuple(_get(_id<Ts>{}, l, N + 1)...);
    }

    static type apply(const detail::StateBlock &l) {
        auto ret = worker(l, typename _indices_builder<S>::type());
        lua_settop(l.GetState(), 0);
        return ret;
    }
};

// Popping nothing returns void
template <typename... Ts>
struct _pop_n_reset_impl<0, Ts...> {
    using type = void;
    static type apply(const detail::StateBlock &l) {
        lua_settop(l.GetState(), 0);
    }
};

// Popping one element returns an unboxed value
template <typename T>
struct _pop_n_reset_impl<1, T> {
    using type = T;
    static type apply(const detail::StateBlock &l) {
        T ret = _get(_id<T>{}, l, -1);
        lua_settop(l.GetState(), 0);
        return ret;
    }
};

template <typename... T>
typename _pop_n_reset_impl<sizeof...(T), T...>::type
_pop_n_reset(const detail::StateBlock &l) {
    return _pop_n_reset_impl<sizeof...(T), T...>::apply(l);
}

template <typename T>
T _pop(_id<T> t, const detail::StateBlock &l) {
    T ret =  _get(t, l, -1);
    lua_pop(l.GetState(), 1);
    return ret;
}

/* Setters */

inline void _push(const detail::StateBlock &l) {}

template <typename T>
inline void _push(const detail::StateBlock &l, MetatableRegistry &m, T* t) {
	if(t == nullptr) {
		lua_pushnil(l.GetState());
	}
	else {
		lua_pushlightuserdata(l, t);
		if (const std::string* metatable = m.Find(typeid(T))) {
			luaL_setmetatable(l.GetState(), metatable->c_str());
		}
	}
}

template <typename T>
inline void _push(const detail::StateBlock &l, MetatableRegistry &m, T& t) {
    lua_pushlightuserdata(l.GetState(), &t);
    if (const std::string* metatable = m.Find(typeid(T))) {
        luaL_setmetatable(l.GetState(), metatable->c_str());
    }
}

inline void _push(const detail::StateBlock &l, MetatableRegistry &, bool b) {
    lua_pushboolean(l.GetState(), b);
}

inline void _push(const detail::StateBlock &l, MetatableRegistry &, char c) {
    lua_pushlstring(l.GetState(), &c, 1);
}

inline void _push(const detail::StateBlock &l, MetatableRegistry &, int i) {
    lua_pushinteger(l.GetState(), i);
}

inline void _push(const detail::StateBlock &l, MetatableRegistry &, unsigned int u) {
#if LUA_VERSION_NUM >= 503
    lua_pushinteger(l.GetState(), u);
#elif LUA_VERSION_NUM >= 502
    lua_pushunsigned(l.GetState(), u);
#else
    lua_pushinteger(l.GetState(), static_cast<lua_Integer>(u));
#endif
}

inline void _push(const detail::StateBlock &l, MetatableRegistry &, long i) {
    lua_pushinteger(l.GetState(), i);
}

inline void _push(const detail::StateBlock &l, MetatableRegistry &, unsigned long u) {
#if LUA_VERSION_NUM >= 503
    lua_pushinteger(l.GetState(), u);
#elif LUA_VERSION_NUM >= 502
    lua_pushunsigned(l.GetState(), static_cast<lua_Unsigned>(u));
#else
    lua_pushinteger(l.GetState(), static_cast<lua_Integer>(u));
#endif
}
    
inline void _push(const detail::StateBlock &l, MetatableRegistry &, long long i) {
    lua_pushinteger(l.GetState(), i);
}

inline void _push(const detail::StateBlock &l, MetatableRegistry &, unsigned long long u) {
#if LUA_VERSION_NUM >= 503
    lua_pushinteger(l.GetState(), u);
#elif LUA_VERSION_NUM >= 502
    lua_pushunsigned(l.GetState(), static_cast<lua_Unsigned>(u));
#else
    lua_pushinteger(l.GetState(), static_cast<lua_Integer>(u));
#endif
}

inline void _push(const detail::StateBlock &l, MetatableRegistry &, float f) {
    lua_pushnumber(l.GetState(), f);
}
    
inline void _push(const detail::StateBlock &l, MetatableRegistry &, double f) {
    lua_pushnumber(l.GetState(), f);
}

inline void _push(const detail::StateBlock &l, MetatableRegistry &, const std::string &s) {
    lua_pushlstring(l.GetState(), s.c_str(), s.size());
}

inline void _push(const detail::StateBlock &l, MetatableRegistry &, const char *s) {
    lua_pushstring(l.GetState(), s);
}
    
inline void _push(const detail::StateBlock &l, MetatableRegistry &, const Value& value);

template <typename T>
inline void _push(const detail::StateBlock &l, T* t) {
	if(t == nullptr) {
		lua_pushnil(l.GetState());
	}
	else {
		lua_pushlightuserdata(l.GetState(), t);
	}
}

template <typename T>
inline void _push(const detail::StateBlock &l, T& t) {
    lua_pushlightuserdata(l.GetState(), &t);
}

inline void _push(const detail::StateBlock &l, bool b) {
    lua_pushboolean(l.GetState(), b);
}

inline void _push(const detail::StateBlock &l, int i) {
    lua_pushinteger(l.GetState(), i);
}

inline void _push(const detail::StateBlock &l, unsigned int u) {
#if LUA_VERSION_NUM >= 503
    lua_pushinteger(l.GetState(), u);
#elif LUA_VERSION_NUM >= 502
    lua_pushunsigned(l.GetState(), u);
#else
    lua_pushinteger(l.GetState(), static_cast<lua_Integer>(u));
#endif
}

inline void _push(const detail::StateBlock &l, long i) {
    lua_pushinteger(l.GetState(), i);
}

inline void _push(const detail::StateBlock &l, unsigned long u) {
#if LUA_VERSION_NUM >= 503
    lua_pushinteger(l.GetState(), u);
#elif LUA_VERSION_NUM >= 502
    lua_pushunsigned(l.GetState(), static_cast<lua_Unsigned>(u));
#else
    lua_pushinteger(l.GetState(), static_cast<lua_Integer>(u));
#endif
}
    
inline void _push(const detail::StateBlock &l, long long i) {
    lua_pushinteger(l.GetState(), i);
}

inline void _push(const detail::StateBlock &l, unsigned long long u) {
#if LUA_VERSION_NUM >= 503
    lua_pushinteger(l.GetState(), u);
#elif LUA_VERSION_NUM >= 502
    lua_pushunsigned(l.GetState(), static_cast<lua_Unsigned>(u));
#else
    lua_pushinteger(l.GetState(), static_cast<lua_Integer>(u));
#endif
}

inline void _push(const detail::StateBlock &l, float f) {
    lua_pushnumber(l.GetState(), f);
}
    
inline void _push(const detail::StateBlock &l, double f) {
    lua_pushnumber(l.GetState(), f);
}

inline void _push(const detail::StateBlock &l, const std::string &s) {
    lua_pushlstring(l.GetState(), s.c_str(), s.size());
}

inline void _push(const detail::StateBlock &l, const char *s) {
    lua_pushstring(l.GetState(), s);
}
    
inline void _push(const detail::StateBlock &l, const Value& value);

template <typename T>
inline void _set(const detail::StateBlock &l, T &&value, const int index) {
    _push(l, std::forward<T>(value));
    lua_replace(l.GetState(), index);
}

inline void _push_n(const detail::StateBlock &, MetatableRegistry &) {}

template <typename T, typename... Rest>
inline void _push_n(const detail::StateBlock &l, MetatableRegistry &m, T value, Rest... rest) {
    _push(l, m, std::forward<T>(value));
    _push_n(l, m, rest...);
}

template <typename... T, std::size_t... N>
inline void _push_dispatcher(const detail::StateBlock &l,
                             MetatableRegistry &m,
                             const std::tuple<T...> &values,
                             _indices<N...>) {
    _push_n(l, m, std::get<N>(values)...);
}

inline void _push(const detail::StateBlock &l, MetatableRegistry &, std::tuple<>) {}

template <typename... T>
inline void _push(const detail::StateBlock &l, MetatableRegistry &m, const std::tuple<T...> &values) {
    constexpr int num_values = sizeof...(T);
    _push_dispatcher(l, m, values,
                     typename _indices_builder<num_values>::type());
}

inline void _push_n(const detail::StateBlock &) {}

template <typename T, typename... Rest>
inline void _push_n(const detail::StateBlock &l, T value, Rest... rest) {
    _push(l, std::forward<T>(value));
    _push_n(l, rest...);
}

template <typename... T, std::size_t... N>
inline void _push_dispatcher(const detail::StateBlock &l,
                             const std::tuple<T...> &values,
                             _indices<N...>) {
    _push_n(l, std::get<N>(values)...);
}

inline void _push(const detail::StateBlock &l, std::tuple<>) {}

template <typename... T>
inline void _push(const detail::StateBlock &l, const std::tuple<T...> &values) {
    constexpr int num_values = sizeof...(T);
    _push_dispatcher(l, values,
                     typename _indices_builder<num_values>::type());
}
}
}
