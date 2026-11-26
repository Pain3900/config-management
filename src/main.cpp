#include "config.hpp"

#include <cstdio>
#include <iostream>
#include <memory>
#include <string_view>

extern FILE* yyin;
int yyparse(Config& config);
int yylex_destroy();

namespace {

constexpr auto usage = "Использование: ./run.sh --input <файл>\n";

struct ScannerCleanup {
    ~ScannerCleanup() { yylex_destroy(); }
};

void translate(const char* path)
{
    std::unique_ptr<FILE, decltype(&std::fclose)> input(
        std::fopen(path, "rb"), &std::fclose);
    if (!input) {
        throw std::runtime_error("не удалось открыть файл " + std::string(path));
    }
    yyin = input.get();
    const ScannerCleanup cleanup;
    Config config;
    if (yyparse(config) != 0) {
        throw std::runtime_error("не удалось разобрать файл");
    }
    std::cout << config.json() << '\n';
}

}  // namespace

int main(int argc, char* argv[])
{
    constexpr auto help_argc = 2;
    constexpr auto input_argc = 3;
    const std::string_view flag = argc > 1 ? argv[1] : "";
    if (argc == help_argc && (flag == "-h" || flag == "--help")) {
        std::cout << usage;
        return 0;
    }
    if (argc != input_argc || (flag != "-i" && flag != "--input")) {
        std::cerr << usage;
        return 2;
    }
    try {
        translate(argv[2]);
    } catch (const std::exception& error) {
        std::cerr << "Ошибка: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
