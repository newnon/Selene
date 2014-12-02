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
    Value(const Value& v);
    
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
    explicit Value(const std::map<Value, Value> &value);
    template <typename Ret, typename... Args>
    explicit Value(const std::function<Ret(Args...)> &fun);
    template <typename Ret, typename... Args>
    explicit Value(Ret (*value)(Args...));
    explicit Value(const LuaRef& ref);
    explicit Value(const std::vector<unsigned char> &value);
    
    inline bool Is(Type type) const { return this->type() == type; }
    inline Type type() const;
    template <typename T>
    const T& cast() const;
    
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
    Value& operator=(const std::map<Value, Value> &value);
    template <typename Ret, typename... Args>
    Value& operator=(const std::function<Ret(Args...)> &fun);
    template <typename Ret, typename... Args>
    Value& operator=(Ret (*value)(Args...));
    Value& operator=(const LuaRef& ref);
    Value& operator=(const std::vector<unsigned char> &value);

    
    template <typename T>
    const Value& operator[] (const T &value) const;
    
    bool bool_value() const;
    double number_value() const;
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
    void push(const std::shared_ptr<lua_State> &l) const;
private:
    std::shared_ptr<LuaValue> _value;
};

class Registry;
    
namespace detail {
    
struct RegistryStorage
{
    Registry *registry;
    void *data;
    lua_Alloc function;
};
    
void *lua_allocator(void *ud, void *ptr, size_t osize, size_t nsize)
{
    RegistryStorage *storage = reinterpret_cast<RegistryStorage*>(ud);
    return storage->function(storage->data, ptr, osize, nsize);
}
    
void store_registry(lua_State *l, Registry *r)
{
    void *data = nullptr;
    lua_Alloc func = lua_getallocf(l, &data);
    RegistryStorage *storage = new RegistryStorage{r, data, func};
    lua_setallocf(l, &lua_allocator, storage);
}
    
Registry * get_registry(lua_State *l)
{
    void *data = nullptr;
    lua_getallocf(l, &data);
    return reinterpret_cast<RegistryStorage*>(data)->registry;
}

template <typename T>
struct is_primitive {
    static constexpr bool value = false;
};
template <>
struct is_primitive<char> {
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
struct is_primitive<bool> {
    static constexpr bool value = true;
};
template <>
struct is_primitive<lua_Number> {
    static constexpr bool result = true;
};
template <>
struct is_primitive<std::string> {
    static constexpr bool value = true;
};

/* getters */
template <typename T>
inline T* _get(_id<T*>, const std::shared_ptr<lua_State> &l, const int index) {
    auto t = lua_topointer(l.get(), index);
    return (T*)(t);
}

inline bool _get(_id<bool>, const std::shared_ptr<lua_State> &l, const int index) {
    return lua_toboolean(l.get(), index) != 0;
}

inline char _get(_id<char>, const std::shared_ptr<lua_State> &l, const int index) {
    return *lua_tostring(l.get(), index);
}

inline int _get(_id<int>, const std::shared_ptr<lua_State> &l, const int index) {
    return static_cast<int>(lua_tointeger(l.get(), index));
}

inline unsigned int _get(_id<unsigned int>, const std::shared_ptr<lua_State> &l, const int index) {
#if LUA_VERSION_NUM >= 502
    return static_cast<unsigned>(lua_tounsigned(l.get(), index));
#else
    return static_cast<unsigned>(lua_tointeger(l.get(), index));
#endif
}

inline long _get(_id<long>, const std::shared_ptr<lua_State> &l, const int index) {
  return static_cast<long>(lua_tointeger(l.get(), index));
}

inline unsigned long _get(_id<unsigned long>, const std::shared_ptr<lua_State> &l, const int index) {
#if LUA_VERSION_NUM >= 502
    return static_cast<unsigned long>(lua_tounsigned(l.get(), index));
#else
    return static_cast<unsigned long>(lua_tointeger(l.get(), index));
#endif
}
    
inline long long _get(_id<long long>, const std::shared_ptr<lua_State> &l, const int index) {
    return lua_tointeger(l.get(), index);
}

inline unsigned long long _get(_id<unsigned long long>, const std::shared_ptr<lua_State> &l, const int index) {
#if LUA_VERSION_NUM >= 502
    return static_cast<unsigned long long>(lua_tounsigned(l.get(), index));
#else
    return static_cast<unsigned long long>(lua_tointeger(l.get(), index));
#endif
}

inline float _get(_id<float>, const std::shared_ptr<lua_State> &l, const int index) {
    return lua_tonumber(l.get(), index);
}
    
inline double _get(_id<double>, const std::shared_ptr<lua_State> &l, const int index) {
    return lua_tonumber(l.get(), index);
}

inline std::string _get(_id<std::string>, const std::shared_ptr<lua_State> &l, const int index) {
    size_t size;
    const char *buff = lua_tolstring(l.get(), index, &size);
    return std::string{buff, size};
}
    
inline Value _get(_id<Value>, const std::shared_ptr<lua_State> &l, const int index);

template <typename T>
inline T* _check_get(_id<T*>, const std::shared_ptr<lua_State> &l, const int index) {
    return (T *)lua_topointer(l.get(), index);
};

template <typename T>
inline T& _check_get(_id<T&>, const std::shared_ptr<lua_State> &l, const int index) {
    static_assert(!is_primitive<T>::value,
                  "Reference types must not be primitives.");
    return *(T *)lua_topointer(l.get(), index);
};

template <typename T>
inline T _check_get(_id<T&&>, const std::shared_ptr<lua_State> &l, const int index) {
    return _check_get(_id<T>{}, l.get(), index);
};

inline int _check_get(_id<int>, const std::shared_ptr<lua_State> &l, const int index) {
    return luaL_checkint(l.get(), index);
};

inline unsigned int _check_get(_id<unsigned int>, const std::shared_ptr<lua_State> &l, const int index) {
#if LUA_VERSION_NUM >= 502
    return static_cast<unsigned>(luaL_checkunsigned(l.get(), index));
#else
    return static_cast<unsigned>(luaL_checkint(l.get(), index));
#endif
}

inline long _check_get(_id<long>, const std::shared_ptr<lua_State> &l, const int index) {
    return luaL_checklong(l.get(), index);
}

inline unsigned long _check_get(_id<unsigned long>, const std::shared_ptr<lua_State> &l, const int index) {
    return static_cast<unsigned long>(luaL_checklong(l.get(), index));
}
    
inline long long _check_get(_id<long long>, const std::shared_ptr<lua_State> &l, const int index) {
    return static_cast<long long>(luaL_checklong(l.get(), index));
}

inline unsigned long long _check_get(_id<unsigned long long>, const std::shared_ptr<lua_State> &l, const int index) {
    return static_cast<unsigned long long>(luaL_checklong(l.get(), index));
}

inline lua_Number _check_get(_id<float>, const std::shared_ptr<lua_State> &l, const int index) {
    return luaL_checknumber(l.get(), index);
}
    
inline lua_Number _check_get(_id<double>, const std::shared_ptr<lua_State> &l, const int index) {
    return luaL_checknumber(l.get(), index);
}

inline bool _check_get(_id<bool>, const std::shared_ptr<lua_State> &l, const int index) {
    return lua_toboolean(l.get(), index) != 0;
}

inline std::string _check_get(_id<std::string>, const std::shared_ptr<lua_State> &l, const int index) {
    size_t size;
    const char *buff = luaL_checklstring(l.get(), index, &size);
    return std::string{buff, size};
}
    
inline Value _check_get(_id<Value>, const std::shared_ptr<lua_State> &l, const int index);

// Worker type-trait struct to _pop_n
// Popping multiple elements returns a tuple
template <std::size_t S, typename... Ts> // First template argument denotes
                                         // the sizeof(Ts...)
struct _pop_n_impl {
    using type =  std::tuple<Ts...>;

    template <std::size_t... N>
    static type worker(const std::shared_ptr<lua_State> &l,
                       _indices<N...>) {
        return std::make_tuple(_get(_id<Ts>{}, l, N + 1)...);
    }

    static type apply(const std::shared_ptr<lua_State> &l) {
        auto ret = worker(l, typename _indices_builder<S>::type());
        lua_pop(l.get(), S);
        return ret;
    }
};

// Popping nothing returns void
template <typename... Ts>
struct _pop_n_impl<0, Ts...> {
    using type = void;
    static type apply(const std::shared_ptr<lua_State> &) {}
};

// Popping one element returns an unboxed value
template <typename T>
struct _pop_n_impl<1, T> {
    using type = T;
    static type apply(const std::shared_ptr<lua_State> &l) {
        T ret = _get(_id<T>{}, l, -1);
        lua_pop(l.get(), 1);
        return ret;
    }
};

template <typename... T>
typename _pop_n_impl<sizeof...(T), T...>::type _pop_n(const std::shared_ptr<lua_State> &l) {
    return _pop_n_impl<sizeof...(T), T...>::apply(l);
}

template <std::size_t S, typename... Ts>
struct _pop_n_reset_impl {
    using type =  std::tuple<Ts...>;

    template <std::size_t... N>
    static type worker(const std::shared_ptr<lua_State> &l,
                       _indices<N...>) {
        return std::make_tuple(_get(_id<Ts>{}, l, N + 1)...);
    }

    static type apply(const std::shared_ptr<lua_State> &l) {
        auto ret = worker(l, typename _indices_builder<S>::type());
        lua_settop(l.get(), 0);
        return ret;
    }
};

// Popping nothing returns void
template <typename... Ts>
struct _pop_n_reset_impl<0, Ts...> {
    using type = void;
    static type apply(const std::shared_ptr<lua_State> &l) {
        lua_settop(l.get(), 0);
    }
};

// Popping one element returns an unboxed value
template <typename T>
struct _pop_n_reset_impl<1, T> {
    using type = T;
    static type apply(const std::shared_ptr<lua_State> &l) {
        T ret = _get(_id<T>{}, l, -1);
        lua_settop(l.get(), 0);
        return ret;
    }
};

template <typename... T>
typename _pop_n_reset_impl<sizeof...(T), T...>::type
_pop_n_reset(const std::shared_ptr<lua_State> &l) {
    return _pop_n_reset_impl<sizeof...(T), T...>::apply(l);
}

template <typename T>
T _pop(_id<T> t, const std::shared_ptr<lua_State> &l) {
    T ret =  _get(t, l, -1);
    lua_pop(l.get(), 1);
    return ret;
}

/* Setters */

inline void _push(const std::shared_ptr<lua_State> &l) {}

template <typename T>
inline void _push(const std::shared_ptr<lua_State> &l, MetatableRegistry &m, T* t) {
	if(t == nullptr) {
		lua_pushnil(l.get());
	}
	else {
		lua_pushlightuserdata(l, t);
		if (const std::string* metatable = m.Find(typeid(T))) {
			luaL_setmetatable(l.get(), metatable->c_str());
		}
	}
}

template <typename T>
inline void _push(const std::shared_ptr<lua_State> &l, MetatableRegistry &m, T& t) {
    lua_pushlightuserdata(l.get(), &t);
    if (const std::string* metatable = m.Find(typeid(T))) {
        luaL_setmetatable(l.get(), metatable->c_str());
    }
}

inline void _push(const std::shared_ptr<lua_State> &l, MetatableRegistry &, bool b) {
    lua_pushboolean(l.get(), b);
}

inline void _push(const std::shared_ptr<lua_State> &l, MetatableRegistry &, char c) {
    lua_pushlstring(l.get(), &c, 1);
}

inline void _push(const std::shared_ptr<lua_State> &l, MetatableRegistry &, int i) {
    lua_pushinteger(l.get(), i);
}

inline void _push(const std::shared_ptr<lua_State> &l, MetatableRegistry &, unsigned int u) {
#if LUA_VERSION_NUM >= 502
    lua_pushunsigned(l.get(), u);
#else
    lua_pushinteger(l.get(), static_cast<lua_Integer>(u));
#endif
}

inline void _push(const std::shared_ptr<lua_State> &l, MetatableRegistry &, long i) {
    lua_pushinteger(l.get(), i);
}

inline void _push(const std::shared_ptr<lua_State> &l, MetatableRegistry &, unsigned long u) {
#if LUA_VERSION_NUM >= 502
    lua_pushunsigned(l.get(), static_cast<lua_Unsigned>(u));
#else
    lua_pushinteger(l.get(), static_cast<lua_Integer>(u));
#endif
}

inline void _push(const std::shared_ptr<lua_State> &l, MetatableRegistry &, float f) {
    lua_pushnumber(l.get(), f);
}
    
inline void _push(const std::shared_ptr<lua_State> &l, MetatableRegistry &, double f) {
    lua_pushnumber(l.get(), f);
}

inline void _push(const std::shared_ptr<lua_State> &l, MetatableRegistry &, const std::string &s) {
    lua_pushlstring(l.get(), s.c_str(), s.size());
}

inline void _push(const std::shared_ptr<lua_State> &l, MetatableRegistry &, const char *s) {
    lua_pushstring(l.get(), s);
}
    
inline void _push(const std::shared_ptr<lua_State> &l, MetatableRegistry &, const Value& value);

template <typename T>
inline void _push(const std::shared_ptr<lua_State> &l, T* t) {
	if(t == nullptr) {
		lua_pushnil(l.get());
	}
	else {
		lua_pushlightuserdata(l.get(), t);
	}
}

template <typename T>
inline void _push(const std::shared_ptr<lua_State> &l, T& t) {
    lua_pushlightuserdata(l.get(), &t);
}

inline void _push(const std::shared_ptr<lua_State> &l, bool b) {
    lua_pushboolean(l.get(), b);
}

inline void _push(const std::shared_ptr<lua_State> &l, int i) {
    lua_pushinteger(l.get(), i);
}

inline void _push(const std::shared_ptr<lua_State> &l, unsigned int u) {
#if LUA_VERSION_NUM >= 502
    lua_pushunsigned(l.get(), u);
#else
    lua_pushinteger(l.get(), static_cast<lua_Integer>(u));
#endif
}

inline void _push(const std::shared_ptr<lua_State> &l, long i) {
    lua_pushinteger(l.get(), i);
}

inline void _push(const std::shared_ptr<lua_State> &l, unsigned long u) {
#if LUA_VERSION_NUM >= 502
    lua_pushunsigned(l.get(), static_cast<lua_Unsigned>(u));
#else
    lua_pushinteger(l.get(), static_cast<lua_Integer>(u));
#endif
}
    
inline void _push(const std::shared_ptr<lua_State> &l, long long i) {
    lua_pushinteger(l.get(), i);
}

inline void _push(const std::shared_ptr<lua_State> &l, unsigned long long u) {
#if LUA_VERSION_NUM >= 502
    lua_pushunsigned(l.get(), static_cast<lua_Unsigned>(u));
#else
    lua_pushinteger(l.get(), static_cast<lua_Integer>(u));
#endif
}

inline void _push(const std::shared_ptr<lua_State> &l, float f) {
    lua_pushnumber(l.get(), f);
}
    
inline void _push(const std::shared_ptr<lua_State> &l, double f) {
    lua_pushnumber(l.get(), f);
}

inline void _push(const std::shared_ptr<lua_State> &l, const std::string &s) {
    lua_pushlstring(l.get(), s.c_str(), s.size());
}

inline void _push(const std::shared_ptr<lua_State> &l, const char *s) {
    lua_pushstring(l.get(), s);
}
    
inline void _push(const std::shared_ptr<lua_State> &l, const Value& value);

template <typename T>
inline void _set(const std::shared_ptr<lua_State> &l, T &&value, const int index) {
    _push(l, std::forward<T>(value));
    lua_replace(l.get(), index);
}

inline void _push_n(const std::shared_ptr<lua_State>&, MetatableRegistry &) {}

template <typename T, typename... Rest>
inline void _push_n(const std::shared_ptr<lua_State> &l, MetatableRegistry &m, T value, Rest... rest) {
    _push(l, m, std::forward<T>(value));
    _push_n(l, m, rest...);
}

template <typename... T, std::size_t... N>
inline void _push_dispatcher(const std::shared_ptr<lua_State> &l,
                             MetatableRegistry &m,
                             const std::tuple<T...> &values,
                             _indices<N...>) {
    _push_n(l, m, std::get<N>(values)...);
}

inline void _push(const std::shared_ptr<lua_State> &l, MetatableRegistry &, std::tuple<>) {}

template <typename... T>
inline void _push(const std::shared_ptr<lua_State> &l, MetatableRegistry &m, const std::tuple<T...> &values) {
    constexpr int num_values = sizeof...(T);
    _push_dispatcher(l, m, values,
                     typename _indices_builder<num_values>::type());
}

inline void _push_n(const std::shared_ptr<lua_State> &) {}

template <typename T, typename... Rest>
inline void _push_n(const std::shared_ptr<lua_State> &l, T value, Rest... rest) {
    _push(l, std::forward<T>(value));
    _push_n(l, rest...);
}

template <typename... T, std::size_t... N>
inline void _push_dispatcher(const std::shared_ptr<lua_State> &l,
                             const std::tuple<T...> &values,
                             _indices<N...>) {
    _push_n(l, std::get<N>(values)...);
}

inline void _push(const std::shared_ptr<lua_State> &l, std::tuple<>) {}

template <typename... T>
inline void _push(const std::shared_ptr<lua_State> &l, const std::tuple<T...> &values) {
    constexpr int num_values = sizeof...(T);
    _push_dispatcher(l, values,
                     typename _indices_builder<num_values>::type());
}
}
}
