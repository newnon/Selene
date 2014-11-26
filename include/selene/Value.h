#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cassert>
#include "primitives.h"

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
    Value(bool value);
    Value(void* value);
    Value(int value);
    Value(long value);
    Value(long long value);
    Value(double value);
    Value(float value);
    Value(const char* value);
    Value(const std::string &value);
    Value(const std::map<Value, Value> &value);
    Value(const LuaRef& ref);
    Value(const Value& v);
    Value(const std::vector<unsigned char> &value);
    
    inline bool Is(Type type) const { return this->type() == type; }
    
    inline Type type() const;
    inline bool bool_value() const;
    inline int int_value() const;
    inline long long_value() const;
    inline float float_value() const;
    inline double double_value() const;
    inline const void* userdata_value() const;
    inline const std::string& string_value() const;
    inline const std::map<Value, Value>& table_value() const;
    
    const Value& operator[] (const Value &value) const;
    Value& operator[] (const Value &value);
    
    operator bool() const;
    operator char() const;
    operator int() const;
    operator long() const;
    operator double() const;
    operator std::string() const;
    operator std::map<Value, Value>() const;
    template <typename... Args>
    Value operator()(Args... args) const;
    bool operator <(const Value &other) const;
    void push(const std::shared_ptr<lua_State> &l) const;
private:
    std::shared_ptr<LuaValue> _value;
};
   
namespace detail {
    
inline Value _get(_id<Value>, const std::shared_ptr<lua_State> &l, const int index) {
    switch (lua_type(l.get(), index)) {
        case LUA_TNIL:
            return Value();
        case LUA_TBOOLEAN:
            return Value(_get(_id<bool>{}, l, index));
        case LUA_TLIGHTUSERDATA:
            return Value(lua_touserdata(l.get(), index));
        case LUA_TNUMBER:
            return Value(_get(_id<double>{}, l, index));
        case LUA_TSTRING:
            return Value(_get(_id<std::string>{}, l, index));
        case LUA_TTABLE: {
                std::map<Value, Value> t;
                luaL_checktype(l.get(), index, LUA_TTABLE);
                lua_pushnil(l.get());
                while (lua_next(l.get(), -2)) {
                    Value idx = _get(_id<Value>{}, l, -2);
                    t[idx] = _get(_id<Value>{}, l, -1);
                    lua_pop(l.get(), 1);
                }
                return Value(t);
            }
        case LUA_TFUNCTION:
            return Value(LuaRef{l, luaL_ref(l.get(), LUA_REGISTRYINDEX)});
        case LUA_TUSERDATA: {
            std::vector<unsigned char> copy;
            unsigned char *data = (unsigned char *)lua_touserdata(l.get(), index);
            size_t len = lua_rawlen (l.get(), index);
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
    
inline Value _check_get(_id<Value>, const std::shared_ptr<lua_State> &l, const int index) {
    return _get(_id<Value>{}, l, index);
}
    
inline void _push(const std::shared_ptr<lua_State> &l, Value value) {
    value.push(l);
}

inline void push(const std::shared_ptr<lua_State> &l, MetatableRegistry &, Value&& value) {
    _push(l, std::forward<Value>(value));
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
    virtual ~LuaValue() {}
    virtual Value::Type type() const = 0;
    virtual bool bool_value() const { return '\0'; }
    virtual int int_value() const { return 0; }
    virtual long long_value() const { return 0; }
    virtual double double_value() const { return 0; }
    virtual float float_value() const { return 0; }
    virtual const void* userdata_value() const { return nullptr; }
    virtual const std::string& string_value() const { return statics().empty_string; }
    virtual const std::map<Value, Value>& table_value() const { return statics().empty_table; }
    virtual const Value& operator[](const Value&) const { return statics().nil; }
    virtual void push_value(const std::shared_ptr<lua_State> &l) const = 0;
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
    virtual void push_value(const std::shared_ptr<lua_State> &l) const override {
        detail::_push(l, nullptr);
    }
};

class BoolValue : public BaseValue<Value::Type::Boolean, bool> {
public:
    explicit BoolValue(bool value) : BaseValue(value) {}
    virtual inline bool bool_value() const override { return static_cast<bool>(_value); }
    virtual void push_value(const std::shared_ptr<lua_State> &l) const override {
        detail::_push(l, _value);
    }
};

class LightUserDataValue : public BaseValue<Value::Type::LightUserData, void*> {
public:
    explicit LightUserDataValue(void* value) : BaseValue(value) {}
    virtual inline bool bool_value() const override { return static_cast<bool>(_value); }
    virtual void* userdata_value() const { return _value; }
    virtual void push_value(const std::shared_ptr<lua_State> &l) const override {
        detail::_push(l, _value);
    }
};

class NumberValue : public BaseValue<Value::Type::Number, double> {
public:
    explicit NumberValue(float value) : BaseValue(value) {}
    explicit NumberValue(double value) : BaseValue(value) {}
    explicit NumberValue(int value) : BaseValue(value) {}
    explicit NumberValue(long value) : BaseValue(value) {}
    explicit NumberValue(long long value) : BaseValue(value) {}
    virtual inline int int_value() const override { return static_cast<int>(_value); }
    virtual inline long long_value() const override { return static_cast<long>(_value); }
    virtual inline float float_value() const override { return static_cast<float>(_value); }
    virtual inline double double_value() const override { return static_cast<double>(_value); }
    virtual inline const std::string& string_value() const override { return _temp = std::to_string(_value); }
    virtual void push_value(const std::shared_ptr<lua_State> &l) const override {
        detail::_push(l, _value);
    }

private:
    mutable std::string _temp;
};

class StringValue : public BaseValue<Value::Type::String, std::string> {
public:
    explicit StringValue(const std::string &value) : BaseValue(value) {}
    explicit StringValue(std::string &&value) : BaseValue(std::move(value)) {}
    virtual inline int int_value() const override { return static_cast<int>(std::stod(_value)); }
    virtual inline long long_value() const override { return static_cast<long>(std::stod(_value)); }
    virtual inline double double_value() const override { return static_cast<double>(std::stod(_value)); }
    virtual inline float float_value() const override { return static_cast<float>(std::stod(_value)); }
    virtual inline const std::string& string_value() const override { return _value; }
    virtual void push_value(const std::shared_ptr<lua_State> &l) const override {
        detail::_push(l, _value);
    }
};

class TableValue : public BaseValue<Value::Type::Table, std::map<Value, Value>> {
public:
    explicit TableValue(const std::map<Value, Value> &value) : BaseValue(value) {}
    explicit TableValue(std::map<Value, Value> &&value) : BaseValue(std::move(value)) {}
    virtual inline const std::map<Value, Value>& table_value() const override { return _value; }
    virtual inline Value& operator[](const Value &value) { return this->_value[value]; }
    virtual void push_value(const std::shared_ptr<lua_State> &l) const override {
        lua_createtable(l.get(), 0, 0);
        for (auto item : _value) {
            if(!item.first.Is(Value::Type::Nil) && !item.second.Is(Value::Type::Nil))
            {
                detail::_push(l, item.first);
                detail::_push(l, item.second);
                lua_rawset(l.get(), -3);
            }
        }
    }
};

class FunctionValue : public BaseValue<Value::Type::Function, LuaRef> {
public:
    explicit FunctionValue(const LuaRef &value) : BaseValue(value) {}
    template <typename... Args>
    const Value operator()(Args... args) const{
        const std::shared_ptr<lua_State> &state = _value.GetSate();
        _value.Push();
        detail::_push_n(state, args...);
        constexpr int num_args = sizeof...(Args);
        lua_call(state.get(), num_args, 1);
        Value ret = detail::_pop(detail::_id<Value>{}, state);
        lua_settop(state.get(), 0);
        return ret;
    }
    virtual void push_value(const std::shared_ptr<lua_State> &l) const override {
        if(_value.GetSate() == l)
            _value.Push();
        else
            detail::_push(l, nullptr);
    }
};

class UserDataValue : public BaseValue<Value::Type::UserData, std::vector<unsigned char>> {
public:
    explicit UserDataValue(const std::vector<unsigned char> &value) : BaseValue(value) {}
    virtual const void* userdata_value() const { return &_value.front(); }
    virtual void push_value(const std::shared_ptr<lua_State> &l) const override
    {
        void *data = lua_newuserdata(l.get(), _value.size());
        memcpy(data, &_value.front(), _value.size());
    }
};

class ThreadValue : public BaseValue<Value::Type::Thread, std::nullptr_t> {
public:
    explicit ThreadValue() : BaseValue(nullptr) {}
    virtual void push_value(const std::shared_ptr<lua_State> &l) const override
    {
        detail::_push(l, nullptr);
    }
};

inline Value::Value() : _value(std::make_shared<NilValue>()) {}
inline Value::Value(bool value) : _value(std::make_shared<BoolValue>(value)) {}
inline Value::Value(void *value) : _value(std::make_shared<LightUserDataValue>(value)) {}
inline Value::Value(int value) : _value(std::make_shared<NumberValue>(value)) {}
inline Value::Value(long value) : _value(std::make_shared<NumberValue>(value)) {}
inline Value::Value(long long value) : _value(std::make_shared<NumberValue>(value)) {}
inline Value::Value(double value) : _value(std::make_shared<NumberValue>(value)) {}
inline Value::Value(const char* value) : _value(std::make_shared<StringValue>(std::string(value))) {}
inline Value::Value(const std::string &value) : _value(std::make_shared<StringValue>(value)) {}
inline Value::Value(const std::map<Value, Value> &value) : _value(std::make_shared<TableValue>(value)) {}
inline Value::Value(const LuaRef& ref) : _value(std::make_shared<FunctionValue>(ref)) {}
inline Value::Value(const std::vector<unsigned char> &value) : _value(std::make_shared<UserDataValue>(value)) {}
inline Value::Value(const Value &value) : _value(value._value) {}

inline Value::Type Value::type() const { return _value->type(); }
inline bool Value::bool_value() const { return _value->bool_value(); }
inline int Value::int_value() const { return _value->int_value(); }
inline long Value::long_value() const { return _value->long_value(); }
inline float Value::float_value() const { return _value->float_value(); }
inline double Value::double_value() const { return _value->double_value(); }
inline const std::string& Value::string_value() const { return _value->string_value(); }
inline const void* Value::userdata_value() const { return _value->userdata_value(); }
inline const std::map<Value, Value>& Value::table_value() const { return _value->table_value(); }
inline const Value& Value::operator[](const Value &value) const { return (*_value)[value]; }

inline Value::operator bool() const { return bool_value(); }
inline Value::operator int() const { return int_value(); }
inline Value::operator long() const { return long_value(); }
inline Value::operator double() const { return double_value(); }
inline Value::operator std::string() const { return string_value(); }
inline Value::operator std::map<Value, Value>() const { return table_value(); }

template <typename... Args>
inline Value Value::operator()(Args... args) const {
    FunctionValue* functionValue = dynamic_cast<FunctionValue*>(_value.get());
    if(functionValue){
        return (*functionValue)(args...);
    }
    else{
        return Value();
    }
}

inline bool Value::operator <(const Value &other) const {
    Type type = this->type();
    if(type == other.type()){
        switch (type) {
            case Type::Nil:
                return true;
            case Type::Boolean:
                return bool_value() < other.bool_value();
            case Type::LightUserData:
                return userdata_value() < other.userdata_value();
            case Type::Number:
                return double_value() < other.double_value();
            case Type::String:
                return string_value() < other.string_value();
            case Type::Table:
                return table_value() < other.table_value();
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

inline void Value::push(const std::shared_ptr<lua_State> &l) const {
    _value->push_value(l);
}

}