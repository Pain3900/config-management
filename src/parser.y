%{
#include "config.hpp"

#include <utility>

int yylex(Config& config);
void yyerror(Config& config, const char* message);

static Value::Array& items(Value* value)
{
    return std::get<Value::Array>(value->data);
}
%}

%locations
%error-verbose
%parse-param { Config& config }
%lex-param { Config& config }

%union {
    Value* value;
}

%token <value> NUMBER "дробное число" INTEGER "целое число"
%token <value> STRING "строка" NAME "имя"
%token ASSIGN ":=" SEMICOLON ";" DOT "."
%token OPEN_ARRAY "{" CLOSE_ARRAY "}"
%token OPEN_EXPRESSION "![" CLOSE_EXPRESSION "]"
%token PLUS "+" MINUS "-" TIMES "*" SORT "sort()"

%type <value> value array array_items expression postfix

%%

document:
      declarations
    | declarations value  { config.result = std::move(*$2); }
    ;

declarations:
      /* пусто */
    | declarations NAME ASSIGN value SEMICOLON
      { config.declare(*$2, std::move(*$4), @2.first_line, @2.first_column); }
    ;

value:
      NUMBER
    | STRING
    | array
    | expression
    ;

array:
    OPEN_ARRAY array_items CLOSE_ARRAY  { $$ = $2; }
    ;

array_items:
      /* пусто */             { $$ = config.keep(Value{Value::Array{}}); }
    | array_items value DOT  { items($1).push_back(std::move(*$2)); }
    ;

expression:
    OPEN_EXPRESSION postfix CLOSE_EXPRESSION
    {
        $$ = config.keep(finish_expression(
            items($2), @1.first_line, @1.first_column));
    }
    ;

postfix:
      /* пусто */        { $$ = config.keep(Value{Value::Array{}}); }
    | postfix value     { items($1).push_back(std::move(*$2)); }
    | postfix INTEGER   { items($1).push_back(std::move(*$2)); }
    | postfix NAME
      { items($1).push_back(config.lookup(*$2, @2.first_line, @2.first_column)); }
    | postfix PLUS
      { apply(items($1), Operator::add, @2.first_line, @2.first_column); }
    | postfix MINUS
      { apply(items($1), Operator::subtract, @2.first_line, @2.first_column); }
    | postfix TIMES
      { apply(items($1), Operator::multiply, @2.first_line, @2.first_column); }
    | postfix SORT
      { sort_top(items($1), @2.first_line, @2.first_column); }
    ;

%%

void yyerror(Config&, const char* message)
{
    throw ConfigError(yylloc.first_line, yylloc.first_column, message);
}
