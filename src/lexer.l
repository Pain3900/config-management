%option noyywrap nounput noinput nodefault 8bit

%{
#include "config.hpp"
#include "parser.hpp"

#define YY_DECL int yylex(Config& config)
#define YY_USER_ACTION locate(yytext, yyleng);

static int line = 1;
static int column = 1;

static void locate(const char* text, int length)
{
    yylloc.first_line = line;
    yylloc.first_column = column;
    for (int i = 0; i < length; ++i) {
        const auto byte = static_cast<unsigned char>(text[i]);
        if (byte == '\n') {
            ++line;
            column = 1;
        } else if ((byte & 0xC0) != 0x80) {
            ++column;
        }
    }
    yylloc.last_line = line;
    yylloc.last_column = column;
}

static Value* parse_number(Config& config, const char* text)
{
    return config.number(text, yylloc.first_line, yylloc.first_column);
}
%}

%%

[ \t\r\n]+        { }
":="              { return ASSIGN; }
";"               { return SEMICOLON; }
"."               { return DOT; }
"{"               { return OPEN_ARRAY; }
"}"               { return CLOSE_ARRAY; }
"!["              { return OPEN_EXPRESSION; }
"]"               { return CLOSE_EXPRESSION; }
"+"               { return PLUS; }
"-"               { return MINUS; }
"*"               { return TIMES; }
"sort()"          { return SORT; }
[0-9]*\.[0-9]+    { yylval.value = parse_number(config, yytext); return NUMBER; }
[0-9]+            { yylval.value = parse_number(config, yytext); return INTEGER; }
[A-Z]+            { yylval.value = config.keep(Value{yytext}); return NAME; }
'[^'\r\n]*'       {
                     yylval.value =
                         config.keep(Value{std::string(yytext + 1, yyleng - 2)});
                     return STRING;
                  }
"'"               {
                     throw ConfigError(line, yylloc.first_column,
                                       "незакрытая строка");
                  }
.                 {
                     throw ConfigError(line, yylloc.first_column,
                                       "недопустимый символ");
                  }
<<EOF>>           { locate("", 0); return 0; }

%%
