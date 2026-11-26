#include "config.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <string_view>
#include <utility>

namespace {

const std::string& as_string(const Value& value)
{
    return std::get<std::string>(value.data);
}

double as_number(const Value& value, int line, int column)
{
    if (const auto* number = std::get_if<double>(&value.data)) {
        return *number;
    }
    throw ConfigError(line, column, "ожидалось число");
}

Value pop(Value::Array& stack)
{
    Value value = std::move(stack.back());
    stack.pop_back();
    return value;
}

double calculate(double left, double right, Operator op)
{
    switch (op) {
        case Operator::add: return left + right;
        case Operator::subtract: return left - right;
        case Operator::multiply: return left * right;
    }
    throw std::logic_error("неизвестная операция");
}

std::string quote_json(std::string_view input)
{
    constexpr char hex[] = "0123456789abcdef";
    std::string output = "\"";
    for (unsigned char c : input) {
        if (c == '"' || c == '\\') {
            output += '\\';
            output += static_cast<char>(c);
        } else if (c < 0x20) {
            output += "\\u00";
            output += hex[c >> 4];
            output += hex[c & 0x0F];
        } else {
            output += static_cast<char>(c);
        }
    }
    return output + '"';
}

std::string number_json(double number)
{
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), number);
    if (result.ec != std::errc{}) {
        throw std::runtime_error("не удалось записать число в JSON");
    }
    return std::string(buffer, result.ptr);
}

std::string value_json(const Value& value)
{
    if (const auto* number = std::get_if<double>(&value.data)) {
        return number_json(*number);
    }
    if (const auto* string = std::get_if<std::string>(&value.data)) {
        return quote_json(*string);
    }

    std::string output = "[";
    for (const auto& item : std::get<Value::Array>(value.data)) {
        if (output.size() > 1) {
            output += ", ";
        }
        output += value_json(item);
    }
    return output + ']';
}

}  // namespace

ConfigError::ConfigError(int line, int column, const std::string& message)
    : std::runtime_error(std::to_string(line) + ":" + std::to_string(column)
                         + ": " + message)
{
}

Value* Config::keep(Value value)
{
    temporaries.push_back(std::move(value));
    return &temporaries.back();
}

Value* Config::number(const char* text, int line, int column)
{
    const std::string_view digits(text);
    double value = 0;
    const auto end = digits.data() + digits.size();
    const auto result = std::from_chars(digits.data(), end, value);
    if (result.ec != std::errc{} || result.ptr != end) {
        throw ConfigError(line, column, "число вне допустимого диапазона");
    }
    return keep(Value{value});
}

const Value& Config::lookup(const Value& name, int line, int column) const
{
    const auto& key = as_string(name);
    const auto it = constants.find(key);
    if (it == constants.end()) {
        throw ConfigError(line, column, "неизвестная константа " + key);
    }
    return it->second;
}

void Config::declare(const Value& name, Value value, int line, int column)
{
    const auto& key = as_string(name);
    if (!constants.emplace(key, std::move(value)).second) {
        throw ConfigError(line, column, "повторное объявление " + key);
    }
}

std::string Config::json() const
{
    if (result) {
        return value_json(*result);
    }

    std::string output = "{";
    for (const auto& [name, value] : constants) {
        if (output.size() > 1) {
            output += ", ";
        }
        output += quote_json(name) + ": " + value_json(value);
    }
    return output + '}';
}

void apply(Value::Array& stack, Operator op, int line, int column)
{
    constexpr auto operand_count = 2;
    if (stack.size() < operand_count) {
        throw ConfigError(line, column, "операции нужны два операнда");
    }
    const double right = as_number(pop(stack), line, column);
    const double left = as_number(pop(stack), line, column);
    const double result = calculate(left, right, op);
    if (!std::isfinite(result)) {
        throw ConfigError(line, column, "результат вне допустимого диапазона");
    }
    stack.push_back(Value{result});
}

void sort_top(Value::Array& stack, int line, int column)
{
    if (stack.empty()) {
        throw ConfigError(line, column, "sort() нужен операнд");
    }
    auto* items = std::get_if<Value::Array>(&stack.back().data);
    if (items == nullptr) {
        throw ConfigError(line, column, "sort() ожидает массив");
    }
    if (items->empty()) {
        return;
    }
    const auto& first = items->front().data;
    const auto comparable = [&first](const Value& item) {
        return item.data.index() == first.index()
            && !std::holds_alternative<Value::Array>(item.data);
    };
    if (!std::all_of(items->begin(), items->end(), comparable)) {
        throw ConfigError(line, column, "sort() ожидает числа или строки");
    }
    std::sort(items->begin(), items->end(),
              [](const Value& a, const Value& b) {
                  if (const auto* number = std::get_if<double>(&a.data)) {
                      return *number < std::get<double>(b.data);
                  }
                  return as_string(a) < as_string(b);
              });
}

Value finish_expression(Value::Array& stack, int line, int column)
{
    constexpr auto result_count = 1;
    if (stack.size() != result_count) {
        throw ConfigError(line, column, "выражение должно дать одно значение");
    }
    return pop(stack);
}
