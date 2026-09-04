#pragma once
#include "common.hpp"
#include <map>
#include <variant>

namespace kg::json {

class Value {
public:
    using Array = std::vector<Value>;
    using Object = std::map<std::string, Value, std::less<>>;
    using Storage = std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;

    Value() : data_(nullptr) {}
    Value(std::nullptr_t) : data_(nullptr) {}
    Value(bool v) : data_(v) {}
    Value(double v) : data_(v) {}
    Value(std::string v) : data_(std::move(v)) {}
    Value(const char* v) : data_(std::string(v)) {}
    Value(Array v) : data_(std::move(v)) {}
    Value(Object v) : data_(std::move(v)) {}

    [[nodiscard]] bool IsNull() const { return std::holds_alternative<std::nullptr_t>(data_); }
    [[nodiscard]] bool IsBool() const { return std::holds_alternative<bool>(data_); }
    [[nodiscard]] bool IsNumber() const { return std::holds_alternative<double>(data_); }
    [[nodiscard]] bool IsString() const { return std::holds_alternative<std::string>(data_); }
    [[nodiscard]] bool IsArray() const { return std::holds_alternative<Array>(data_); }
    [[nodiscard]] bool IsObject() const { return std::holds_alternative<Object>(data_); }

    [[nodiscard]] bool AsBool(bool fallback=false) const;
    [[nodiscard]] double AsNumber(double fallback=0.0) const;
    [[nodiscard]] const std::string& AsString() const;
    [[nodiscard]] const Array& AsArray() const;
    [[nodiscard]] Array& AsArray();
    [[nodiscard]] const Object& AsObject() const;
    [[nodiscard]] Object& AsObject();

    [[nodiscard]] const Value* Find(std::string_view key) const;
    [[nodiscard]] Value* Find(std::string_view key);
    Value& operator[](std::string key);

private:
    Storage data_;
};

Value Parse(std::string_view text);
std::string Dump(const Value& value, int indent = 2);
std::string Escape(std::string_view text);

} // namespace kg::json
