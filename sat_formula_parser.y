/*
 *  sat-shell is an interactive tcl-shell for sat-solver interaction
 *  Copyright (C) 2016  Andreas Dixius
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

%{
 
#include "sat_formula.h"
#include "sat_formula_parser.h"
#include "sat_formula_lexer.h"

static int yyerror(SatFormula *formula, yyscan_t scanner, const char *msg) {
    // Add error handling routine as needed
    return 0;
}
 
%}

%code requires {

#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void* yyscan_t;
#endif

}

%output  "sat_formula_parser.c"
%defines "sat_formula_parser.h"
 
%define api.pure
%lex-param   { yyscan_t scanner }
%parse-param { SatFormula *formula }
%parse-param { yyscan_t scanner }

%union {
    int literal;
    SatFormula formula;
}
 
%left TOKEN_AND
%left TOKEN_OR
%left TOKEN_XOR
%left TOKEN_EQUAL
%left TOKEN_RIMPL
%left TOKEN_LIMPL
 
%token TOKEN_LPAREN
%token TOKEN_RPAREN
%token TOKEN_LBRACK
%token TOKEN_RBRACK
%token TOKEN_LBRACE
%token TOKEN_RBRACE
%token TOKEN_LTAG
%token TOKEN_RTAG

%precedence TOKEN_INVERT
%token TOKEN_AND
%token TOKEN_OR
%token TOKEN_XOR
%token TOKEN_EQUAL
%token TOKEN_RIMPL
%token TOKEN_LIMPL

%token <literal> TOKEN_LITERAL
%token TOKEN_ERROR

%type <formula> formula
%destructor { sat_formula_free (&$$); } <formula>
 
%%
 
input
    : formula { *formula = $1; }
    ;
 
formula
    : formula[L] TOKEN_EQUAL formula[R] { $$ = sat_formula_new_operation ( SAT_FORMULA_TAG_OP_EQUAL, $L, $R ); }
    | formula[L] TOKEN_LIMPL formula[R] { $$ = sat_formula_new_operation ( SAT_FORMULA_TAG_OP_LIMPL, $L, $R ); }
    | formula[L] TOKEN_RIMPL formula[R] { $$ = sat_formula_new_operation ( SAT_FORMULA_TAG_OP_RIMPL, $L, $R ); }
    | formula[L] TOKEN_XOR formula[R] { $$ = sat_formula_new_operation ( SAT_FORMULA_TAG_OP_XOR, $L, $R ); }
    | formula[L] TOKEN_OR formula[R] { $$ = sat_formula_new_operation ( SAT_FORMULA_TAG_OP_OR, $L, $R ); }
    | formula[L] TOKEN_AND formula[R] { $$ = sat_formula_new_operation ( SAT_FORMULA_TAG_OP_AND, $L, $R ); }
    | TOKEN_INVERT formula[L] { $$ = sat_formula_new_operation ( SAT_FORMULA_TAG_INVERSION, $L, NULL ); }
    | TOKEN_LPAREN formula[E] TOKEN_RPAREN { $$ = $E; }
    | TOKEN_LBRACK formula[E] TOKEN_RBRACK { $$ = $E; }
    | TOKEN_LBRACE formula[E] TOKEN_RBRACE { $$ = $E; }
    | TOKEN_LTAG formula[E] TOKEN_RTAG { $$ = $E; }
    | TOKEN_LITERAL { $$ = sat_formula_new_literal ($1); }
    ;
 
%%
