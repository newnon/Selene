#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cassert>
#include "primitives.h"

namespace sel {
   
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
    
inline void _push(const std::shared_ptr<lua_State> &l, const Value &value) {
    value.push(l);
}

inline void _push(const std::shared_ptr<lua_State> &l, MetatableRegistry &, const Value& value) {
    _push(l, value);
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
    virtual double number_value() const { return 0; }
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
    explicit NumberValue(double value) : BaseValue(value) {}
    virtual inline double number_value() const override { return static_cast<double>(_value); }
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
    virtual inline double number_value() const override { return static_cast<double>(std::stod(_value)); }
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
        const std::shared_ptr<lua_State> &state = _value.GetState();
        _value.Push();
        detail::_push_n(state, args...);
        constexpr int num_args = sizeof...(Args);
        lua_call(state.get(), num_args, 1);
        Value ret = detail::_pop(detail::_id<Value>{}, state);
        lua_settop(state.get(), 0);
        return ret;
    }
    virtual void push_value(const std::shared_ptr<lua_State> &l) const override {
        if(_value.GetState() == l)
            _value.Push();
        else
            detail::_push(l, nullptr);
    }
template <typename Ret, typename... Args, class = typename std::enable_if<!std::is_void<Ret>::value>::type >
    inline const std::function<Ret(Args...)> function_value() const
    {
        LuaRef ref = _value;
        return [ref] (Args... args)
        {
            const std::shared_ptr<lua_State> &state = ref.GetState();
            ref.Push();
            detail::_push_n(state, args...);
            constexpr int num_args = sizeof...(Args);
            lua_call(state.get(), num_args, 1);
            Ret ret = detail::_pop(detail::_id<Ret>{}, state);
            lua_settop(state.get(), 0);
            return ret;
        };
    }
    template <typename Ret, typename... Args, class = typename std::enable_if<std::is_void<Ret>::value>::type>
    inline const std::function<void(Args...)> function_value() const
    {
        LuaRef ref = _value;
        return [ref] (Args... args)
        {
            const std::shared_ptr<lua_State> &state = ref.GetState();
            ref.Push();
            detail::_push_n(state, args...);
            constexpr int num_args = sizeof...(Args);
            lua_call(state.get(), num_args, 1);
            lua_settop(state.get(), 0);
        };
    }
};

template <typename... Args>
class CFunctionValue : public BaseValue<Value::Type::Function, std::function<Value(Args...)>> {
public:
    explicit CFunctionValue(const std::function<Value(Args...)> &value) : BaseValue<Value::Type::Function, std::function<Value(Args...)>>(value) {}
    template <typename Ret>
    explicit CFunctionValue(Ret (*value)(Args...)) : BaseValue<Value::Type::Function, std::function<Value(Args...)>>(value) {}
    const Value operator()(Args... args) const{
        return this->_value(args...);
    }
    virtual void push_value(const std::shared_ptr<lua_State> &l) const override {
        Registry *registry = detail::get_registry(l.get());
        if(registry)
            registry->Register(this->_value);
    }
    template <typename Ret>
    inline const std::function<Ret(Args...)> function_value() const
    {
        auto ref = this->_value;
        return [ref] (Args... args)
        {
            return static_cast<Ret>(ref(args...));
        };
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
inline Value::Value(short value) : _value(std::make_shared<NumberValue>(value)) {}
inline Value::Value(unsigned short value) : _value(std::make_shared<NumberValue>(value)) {}
inline Value::Value(int value) : _value(std::make_shared<NumberValue>(value)) {}
inline Value::Value(unsigned int value) : _value(std::make_shared<NumberValue>(value)) {}
inline Value::Value(long value) : _value(std::make_shared<NumberValue>(value)) {}
inline Value::Value(unsigned long value) : _value(std::make_shared<NumberValue>(value)) {}
inline Value::Value(long long value) : _value(std::make_shared<NumberValue>(value)) {}
inline Value::Value(unsigned long long value) : _value(std::make_shared<NumberValue>(value)) {}
inline Value::Value(float value) : _value(std::make_shared<NumberValue>(value)) {}
inline Value::Value(double value) : _value(std::make_shared<NumberValue>(value)) {}
inline Value::Value(long double value) : _value(std::make_shared<NumberValue>(value)) {}
inline Value::Value(const char* value) : _value(std::make_shared<StringValue>(std::string(value))) {}
inline Value::Value(const std::string &value) : _value(std::make_shared<StringValue>(value)) {}
inline Value::Value(const std::map<Value, Value> &value) : _value(std::make_shared<TableValue>(value)) {}
inline Value::Value(const LuaRef& ref) : _value(std::make_shared<FunctionValue>(ref)) {}
template <typename Ret, typename... Args>
inline Value::Value(const std::function<Ret(Args...)> &value) : _value(std::make_shared<CFunctionValue>(value)) {}
template <typename Ret, typename... Args>
inline Value::Value(Ret (*value)(Args...)) : _value(std::make_shared<CFunctionValue<Args...>>(value)) {}
inline Value::Value(const std::vector<unsigned char> &value) : _value(std::make_shared<UserDataValue>(value)) {}
inline Value::Value(const Value &value) : _value(value._value) {}

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
inline Value& Value::operator=(const std::map<Value, Value> &value) { return (*this = Value(value)); }
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
    FunctionValue* functionValue = dynamic_cast<FunctionValue*>(_value.get());
    if(functionValue){
        return functionValue->function_value<Ret, Args...>();
    }
    else{
        CFunctionValue<Args...> *cfunctionValue = dynamic_cast<CFunctionValue<Args...>*>(_value.get());
        if(cfunctionValue)
            return cfunctionValue->template function_value<Ret>();
    }
    return std::function<Ret(Args...)>();
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
inline const T& Value::cast() const
{
    return *this;
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
    FunctionValue* functionValue = dynamic_cast<FunctionValue*>(_value.get());
    if(functionValue){
        return (*functionValue)(args...);
    }
    else{
        CFunctionValue<Args...> *cfunctionValue = dynamic_cast<CFunctionValue<Args...>*>(_value.get());
        if(cfunctionValue)
            return (*cfunctionValue)(args...);
        else
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

inline void Value::push(const std::shared_ptr<lua_State> &l) const {
    _value->push_value(l);
}

}
