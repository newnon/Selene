#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cassert>
#include "primitives.h"

namespace sel {
   
namespace detail {
    
inline Value _get(_id<Value>, const detail::StateBlock &l, const int index) {
    switch (lua_type(l.GetState(), index)) {
        case LUA_TNIL:
            return Value();
        case LUA_TBOOLEAN:
            return Value(_get(_id<bool>{}, l, index));
        case LUA_TLIGHTUSERDATA:
            return Value(lua_touserdata(l.GetState(), index));
        case LUA_TNUMBER:
            return Value(_get(_id<double>{}, l, index));
        case LUA_TSTRING:
            return Value(_get(_id<std::string>{}, l, index));
        case LUA_TTABLE: {
                std::map<Value, Value> t;
                luaL_checktype(l.GetState(), index, LUA_TTABLE);
                lua_pushnil(l.GetState());
                while (lua_next(l.GetState(), -2)) {
                    Value idx = _get(_id<Value>{}, l, -2);
                    t[idx] = _get(_id<Value>{}, l, -1);
                    lua_pop(l.GetState(), 1);
                }
                return Value(t);
            }
        case LUA_TFUNCTION:
            lua_pushvalue(l.GetState(), -1);
            return Value(LuaRef{l, luaL_ref(l.GetState(), LUA_REGISTRYINDEX)});
        case LUA_TUSERDATA: {
            std::vector<unsigned char> copy;
            unsigned char *data = (unsigned char *)lua_touserdata(l.GetState(), index);
            size_t len = lua_rawlen (l.GetState(), index);
            if(data && len){
                copy.resize(len);
                memcpy(&copy.front(),data,len);
            }
            return Value(copy);
        }
        case LUA_TTHREAD:
        default:
            return Value();
    }
}
    
inline Value _check_get(_id<Value>, const detail::StateBlock &l, const int index) {
    return _get(_id<Value>{}, l, index);
}
    
inline void _push(const detail::StateBlock &l, const Value &value) {
    value.push(l);
}

inline void _push(const detail::StateBlock &l, MetatableRegistry &, const Value& value) {
    _push(l, value);
}
    
template <typename Ret, typename... Args, std::size_t... N>
inline Ret _lift(std::function<Ret(Args...)> fun,
                 const std::vector<sel::Value> &params,
                 _indices<N...>) {
    return fun(N<params.size()?Args(params[N]):Args()...);
}

template <typename Ret, typename... Args>
inline Ret _lift(std::function<Ret(Args...)> fun,
                 const std::vector<sel::Value> &params) {
    return _lift(fun, params, typename _indices_builder<sizeof...(Args)>::type());
}
    
}

struct Statics {
    Value nil;
    std::string empty_string;
    std::map<Value, Value> empty_table;
};

inline Statics& statics() {
    static Statics s;
    return s;
}

class LuaValue {
public:
    virtual LuaValue* clone() const = 0;
    virtual ~LuaValue() {}
    virtual Value::Type type() const = 0;
    virtual bool bool_value() const { return '\0'; }
    virtual double number_value() const { return 0; }
    virtual const void* userdata_value() const { return nullptr; }
    virtual const std::string& string_value() const { return statics().empty_string; }
    virtual const std::map<Value, Value>& table_value() const { return statics().empty_table; }
    virtual const Value& operator[](const Value&) const { return statics().nil; }
    virtual Value execute(const std::vector<sel::Value> &params) const { return statics().nil; }
    virtual void push_value(const detail::StateBlock &l) const = 0;
};

template <Value::Type tag, typename T>
class BaseValue : public LuaValue {
public:
    explicit BaseValue(const T &value) : _value(value) {}
    explicit BaseValue(const T &&value) : _value(std::move(value)) {}
    virtual ~BaseValue() {}
    
    virtual Value::Type type() const override {
        return tag;
    }
    
protected:
    T _value;
};

class NilValue : public BaseValue<Value::Type::Nil, std::nullptr_t> {
public:
    explicit NilValue() : BaseValue(nullptr) {}
    virtual LuaValue* clone() const override { return new NilValue(); }
    virtual void push_value(const detail::StateBlock &l) const override {
        detail::_push(l, nullptr);
    }
};

class BoolValue : public BaseValue<Value::Type::Boolean, bool> {
public:
    explicit BoolValue(bool value) : BaseValue(value) {}
    virtual LuaValue* clone() const override { return new BoolValue(_value); }
    virtual inline bool bool_value() const override { return static_cast<bool>(_value); }
    virtual void push_value(const detail::StateBlock &l) const override {
        detail::_push(l, _value);
    }
};

class LightUserDataValue : public BaseValue<Value::Type::LightUserData, void*> {
public:
    explicit LightUserDataValue(void* value) : BaseValue(value) {}
    virtual LuaValue* clone() const override { return new LightUserDataValue(_value); }
    virtual inline bool bool_value() const override { return static_cast<bool>(_value); }
    virtual const void* userdata_value() const { return _value; }
    virtual void push_value(const detail::StateBlock &l) const override {
        detail::_push(l, _value);
    }
};

class NumberValue : public BaseValue<Value::Type::Number, double> {
public:
    explicit NumberValue(double value) : BaseValue(value) {}
    virtual LuaValue* clone() const override { return new NumberValue(_value); }
    virtual inline double number_value() const override { return static_cast<double>(_value); }
    virtual inline const std::string& string_value() const override { return _temp = std::to_string(_value); }
    virtual void push_value(const detail::StateBlock &l) const override {
        detail::_push(l, _value);
    }

private:
    mutable std::string _temp;
};

class StringValue : public BaseValue<Value::Type::String, std::string> {
public:
    explicit StringValue(const std::string &value) : BaseValue(value) {}
    explicit StringValue(std::string &&value) : BaseValue(std::move(value)) {}
    virtual LuaValue* clone() const override { return new StringValue(_value); }
    virtual inline double number_value() const override { return static_cast<double>(std::stod(_value)); }
    virtual inline const std::string& string_value() const override { return _value; }
    virtual void push_value(const detail::StateBlock &l) const override {
        detail::_push(l, _value);
    }
};

class TableValue : public BaseValue<Value::Type::Table, std::map<Value, Value>> {
public:
    explicit TableValue(const std::map<Value, Value> &value) : BaseValue(value) {}
    explicit TableValue(std::map<Value, Value> &&value) : BaseValue(std::move(value)) {}
    explicit TableValue(const std::vector<Value> &value) : BaseValue(std::map<Value, Value>())
    {
        int counter = 1;
        for(const auto &it:value)
        {
            _value.emplace(Value(counter), it);
        }
    }
    explicit TableValue(std::vector<Value> &&value) : BaseValue(std::map<Value, Value>())
    {
        int counter = 1;
        for(const auto &it:value)
        {
            _value.emplace(Value(counter), std::move(it));
        }
    }
    
    virtual LuaValue* clone() const override { return new TableValue(_value); }

    virtual inline const std::map<Value, Value>& table_value() const override { return _value; }
    virtual inline Value& operator[](const Value &value) { return this->_value[value]; }
    virtual void push_value(const detail::StateBlock &l) const override {
        lua_createtable(l.GetState(), 0, 0);
        for (const auto &item : _value) {
            if(!item.first.Is(Value::Type::Nil) && !item.second.Is(Value::Type::Nil))
            {
                detail::_push(l, item.first);
                detail::_push(l, item.second);
                lua_rawset(l.GetState(), -3);
            }
        }
    }
};

template <typename Ret, typename... Args>
class CFunctionValue: public BaseValue<Value::Type::Function, std::function<Ret(Args...)>>
{
public:
    explicit CFunctionValue(const std::function<Ret(Args...)> &function) : BaseValue<Value::Type::Function, std::function<Ret(Args...)>>(function) {}
    virtual LuaValue* clone() const override { return new CFunctionValue<Ret, Args...>(this->_value); }
    virtual sel::Value execute(const std::vector<sel::Value> &params) const
    {
        return sel::Value(detail::_lift(this->_value, params));
    }
    
    virtual void push_value(const detail::StateBlock &l) const override {
        Registry *registry = l.GetRegistry();
        if(registry)
            registry->Register(this->_value);
    }
};

class LuaFunctionValue: public BaseValue<Value::Type::Function, LuaRef>
{
public:
    explicit LuaFunctionValue(const LuaRef &value) : BaseValue(value) {}
    virtual LuaValue* clone() const override { return new LuaFunctionValue(_value); }
    virtual sel::Value execute(const std::vector<sel::Value> &params) const override {
        const detail::StateBlock *state = _value.GetStateBlock().get();
        _value.Push();
        for(auto const it:params)
            detail::_push(*state, it);
        lua_call(state->GetState(), (int)params.size(), 1);
        Value ret = detail::_pop(detail::_id<Value>{}, *state);
        lua_settop(state->GetState(), 0);
        return ret;
    }

    virtual void push_value(const detail::StateBlock &l) const override {
        if(_value.GetStateBlock().get() == &l)
            _value.Push();
        else
            detail::_push(l, nullptr);
    }
};


class UserDataValue : public BaseValue<Value::Type::UserData, std::vector<unsigned char>> {
public:
    explicit UserDataValue(const std::vector<unsigned char> &value) : BaseValue(value) {}
    virtual LuaValue* clone() const override { return new UserDataValue(_value); }
    virtual const void* userdata_value() const { return &_value.front(); }
    virtual void push_value(const detail::StateBlock &l) const override
    {
        void *data = lua_newuserdata(l.GetState(), _value.size());
        memcpy(data, &_value.front(), _value.size());
    }
};

class ThreadValue : public BaseValue<Value::Type::Thread, std::nullptr_t> {
public:
    explicit ThreadValue() : BaseValue(nullptr) {}
    virtual LuaValue* clone() const override { return new ThreadValue(); }
    virtual void push_value(const detail::StateBlock &l) const override
    {
        detail::_push(l, nullptr);
    }
};

template<class _Tp, class ..._Args>
std::unique_ptr<_Tp> sel_make_unique(_Args&& ...__args)
{
    return std::unique_ptr<_Tp>(new _Tp(__args...));
}

inline Value::Value(const Value &other) { _value.reset(other._value->clone()); }
inline Value::Value(Value &&other) { _value = std::move(other._value); }

inline Value::Value() : _value(sel_make_unique<NilValue>()) {}
inline Value::Value(bool value) : _value(sel_make_unique<BoolValue>(value)) {}
inline Value::Value(void *value) : _value(sel_make_unique<LightUserDataValue>(value)) {}
inline Value::Value(short value) : _value(sel_make_unique<NumberValue>(value)) {}
inline Value::Value(unsigned short value) : _value(sel_make_unique<NumberValue>(value)) {}
inline Value::Value(int value) : _value(sel_make_unique<NumberValue>(value)) {}
inline Value::Value(unsigned int value) : _value(sel_make_unique<NumberValue>(value)) {}
inline Value::Value(long value) : _value(sel_make_unique<NumberValue>(value)) {}
inline Value::Value(unsigned long value) : _value(sel_make_unique<NumberValue>(value)) {}
inline Value::Value(long long value) : _value(sel_make_unique<NumberValue>(value)) {}
inline Value::Value(unsigned long long value) : _value(sel_make_unique<NumberValue>(value)) {}
inline Value::Value(float value) : _value(sel_make_unique<NumberValue>(value)) {}
inline Value::Value(double value) : _value(sel_make_unique<NumberValue>(value)) {}
inline Value::Value(long double value) : _value(sel_make_unique<NumberValue>(value)) {}
inline Value::Value(const char* value) : _value(sel_make_unique<StringValue>(std::string(value))) {}
inline Value::Value(const std::string &value) : _value(sel_make_unique<StringValue>(value)) {}
inline Value::Value(std::string &&value) : _value(sel_make_unique<StringValue>(std::move(value))) {}
inline Value::Value(const std::map<Value, Value> &value) : _value(sel_make_unique<TableValue>(value)) {}
inline Value::Value(std::map<Value, Value> &&value) : _value(sel_make_unique<TableValue>(std::move(value))) {}
inline Value::Value(const std::vector<Value> &value) : _value(sel_make_unique<TableValue>(value)) {}
inline Value::Value(std::vector<Value> &&value) : _value(sel_make_unique<TableValue>(std::move(value))) {}
inline Value::Value(const LuaRef& ref) : _value(sel_make_unique<LuaFunctionValue>(ref)) {}
template <typename Ret, typename... Args>
inline Value::Value(const std::function<Ret(Args...)> &value) : _value(sel_make_unique<CFunctionValue<Ret, Args...>>(value)) {}
template <typename Ret, typename... Args>
inline Value::Value(Ret (*value)(Args...)) : _value(sel_make_unique<CFunctionValue<Ret, Args...>>(value)) {}
inline Value::Value(const std::vector<unsigned char> &value) : _value(sel_make_unique<UserDataValue>(value)) {}

inline Value& Value::operator=(const Value &other){ _value.reset(other._value->clone()); return *this; }
inline Value& Value::operator=(Value &&other){ _value = std::move(other._value); return *this; }

inline Value& Value::operator=(bool value) { return (*this = Value(value)); }
inline Value& Value::operator=(void* value) { return (*this = Value(value)); }
inline Value& Value::operator=(short value) { return (*this = Value(value)); }
inline Value& Value::operator=(unsigned short value) { return (*this = Value(value)); }
inline Value& Value::operator=(int value) { return (*this = Value(value)); }
inline Value& Value::operator=(unsigned int value) { return (*this = Value(value)); }
inline Value& Value::operator=(long value) { return (*this = Value(value)); }
inline Value& Value::operator=(unsigned long value) { return (*this = Value(value)); }
inline Value& Value::operator=(long long value) { return (*this = Value(value)); }
inline Value& Value::operator=(unsigned long long value) { return (*this = Value(value)); }
inline Value& Value::operator=(float value) { return (*this = Value(value)); }
inline Value& Value::operator=(double value) { return (*this = Value(value)); }
inline Value& Value::operator=(long double value) { return (*this = Value(value)); }
inline Value& Value::operator=(const char* value) { return (*this = Value(value)); }
inline Value& Value::operator=(const std::string &value) { return (*this = Value(value)); }
inline Value& Value::operator=(std::string &&value) { return (*this = Value(value)); }
inline Value& Value::operator=(const std::map<Value, Value> &value) { return (*this = Value(value)); }
inline Value& Value::operator=(std::map<Value, Value> &&value) { return (*this = Value(value)); }
inline Value& Value::operator=(const std::vector<Value> &value) { return (*this = Value(value)); }
inline Value& Value::operator=(std::vector<Value> &&value) { return (*this = Value(value)); }
template <typename Ret, typename... Args>
inline Value& Value::operator=(const std::function<Ret(Args...)> &value) { return (*this = Value(value)); }
template <typename Ret, typename... Args>
inline Value& Value::operator=(Ret (*value)(Args...)) { return (*this = Value(value)); }
inline Value& Value::operator=(const LuaRef& value) { return (*this = Value(value)); }
inline Value& Value::operator=(const std::vector<unsigned char> &value) { return (*this = Value(value)); }

inline bool Value::bool_value() const { return _value->bool_value(); }
inline double Value::number_value() const { return _value->number_value(); }
inline const std::string& Value::string_value() const { return _value->string_value(); }
inline const std::map<Value, Value>& Value::table_value() const { return _value->table_value(); }
template <typename Ret, typename... Args>
inline const std::function<Ret(Args...)> Value::function_value() const
{
    Value value = *this;
    return [value](Args... args) { return Ret(value._value->execute(std::vector<sel::Value>{sel::Value(args)...})); };
}

inline Value::operator bool() const { return _value->bool_value(); }
inline Value::operator short() const { return _value->number_value(); }
inline Value::operator unsigned short() const { return _value->number_value(); }
inline Value::operator int() const { return _value->number_value(); }
inline Value::operator unsigned int() const { return _value->number_value(); }
inline Value::operator long() const { return _value->number_value(); }
inline Value::operator unsigned long() const { return _value->number_value(); }
inline Value::operator long long() const { return _value->number_value(); }
inline Value::operator unsigned long long() const { return _value->number_value(); }
inline Value::operator float() const { return _value->number_value(); }
inline Value::operator double() const { return _value->number_value(); }
inline Value::operator long double() const { return _value->number_value(); }
inline Value::operator const std::string&() const { return _value->string_value(); }
inline Value::operator const std::map<Value, Value>&() const { return _value->table_value(); }

inline Value::Type Value::type() const { return _value->type(); }
template <typename T>
inline const T Value::cast() const
{
    return static_cast<T>(*this);
}

template <typename T>
inline const Value& Value::operator[] (const T &value) const
{
    static const Value fake;
    const std::map<Value, Value>& table = _value->table_value();
    auto it = table.find(Value(value));
    if(it!=table.end())
        return it->second;
    else
        return fake;
}

template <typename... Args>
inline Value Value::operator()(Args... args) const {
    return _value->execute(std::vector<sel::Value>{sel::Value(args)...});
}

inline bool Value::operator <(const Value &other) const {
    Type type = this->type();
    if(type == other.type()){
        switch (type) {
            case Type::Nil:
                return true;
            case Type::Boolean:
                return _value->bool_value() < other._value->bool_value();
            case Type::LightUserData:
                return _value->userdata_value() < other._value->userdata_value();
            case Type::Number:
                return _value->number_value() < other._value->number_value();
            case Type::String:
                return _value->string_value() < other._value->string_value();
            case Type::Table:
                return _value->table_value() < other._value->table_value();
            case Type::Function:
                return _value.get() < other._value.get();
            case Type::UserData:
                return _value.get() < other._value.get();
            case Type::Thread:
                return _value.get() < other._value.get();
            default:
                assert(false);
        }
    }
    return type < other.type();
}

inline void Value::push(const detail::StateBlock &l) const {
    _value->push_value(l);
}

}
