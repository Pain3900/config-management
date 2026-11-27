#pragma once

#include <deque>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

struct Value {
    using Array = std::vector<Value>;
    std::variant<double, std::string, Array> data;
    std::string numeric_literal{};
    bool quote_numeric_literal = false;
};

enum class Operator { add, subtract, multiply };

class ConfigError : public std::runtime_error {
public:
    ConfigError(int line, int column, const std::string& message);
};

struct Config {
    std::deque<Value> temporaries;
    std::map<std::string, Value> constants;
    std::optional<Value> result;

    Value* keep(Value value);
    Value* number(const char* text, int line, int column);
    Value lookup(const Value& name, int line, int column) const;
    void declare(const Value& name, Value value, int line, int column);
    std::string json() const;
};

void apply(Value::Array& stack, Operator op, int line, int column);
void sort_top(Value::Array& stack, int line, int column);
Value finish_expression(Value::Array& stack, int line, int column);
