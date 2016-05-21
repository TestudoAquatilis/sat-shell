/*
 *  sat-shell is an interactive tcl-shell for solving satisfiability problems.
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
    /* Add error handling routine as needed */
    return 0;
}

%}

%code requires {

#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void* yyscan_t;
#endif

}

%output  "parser/sat_formula_parser.c"
%defines "parser/sat_formula_parser.h"
%name-prefix "sat_formula_yy"


%define api.pure
%lex-param   { yyscan_t scanner }
%parse-param { SatFormula *formula }
%parse-param { yyscan_t scanner }

%union {
    long int variable;
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

%token <variable> TOKEN_VARIABLE
%token TOKEN_ERROR

%type <formula> formula_equal formula_impl formula_xor formula_or formula_and formula_basic
%destructor { sat_formula_free (&$$); } <formula>

%%

input
    : formula_equal[F] { *formula = $F; }
    ;

formula_equal
    : formula_equal[L] TOKEN_EQUAL formula_impl[R] { $$ = sat_formula_new_operation ( SAT_FORMULA_TAG_OP_EQUAL, $L, $R ); }
    | formula_impl[F] { $$ = $F; }
    ;

formula_impl
    : formula_impl[L] TOKEN_LIMPL formula_xor[R] { $$ = sat_formula_new_operation ( SAT_FORMULA_TAG_OP_LIMPL, $L, $R ); }
    | formula_impl[L] TOKEN_RIMPL formula_xor[R] { $$ = sat_formula_new_operation ( SAT_FORMULA_TAG_OP_RIMPL, $L, $R ); }
    | formula_xor[F] { $$ = $F; }
    ;

formula_xor
    : formula_xor[L] TOKEN_XOR formula_or[R] { $$ = sat_formula_new_operation ( SAT_FORMULA_TAG_OP_XOR, $L, $R ); }
    | formula_or[F] { $$ = $F; }
    ;

formula_or
    : formula_or[L] TOKEN_OR formula_and[R] { $$ = sat_formula_new_operation ( SAT_FORMULA_TAG_OP_OR, $L, $R ); }
    | formula_and[F] { $$ = $F; }
    ;

formula_and
    : formula_and[L] TOKEN_AND formula_basic[R] { $$ = sat_formula_new_operation ( SAT_FORMULA_TAG_OP_AND, $L, $R ); }
    | formula_and[L] formula_basic[R] { $$ = sat_formula_new_operation ( SAT_FORMULA_TAG_OP_AND, $L, $R ); }
    | formula_basic[F] { $$ = $F; }
    ;

formula_basic
    : TOKEN_LPAREN formula_equal[F] TOKEN_RPAREN { $$ = $F; }
    | TOKEN_LBRACK formula_equal[F] TOKEN_RBRACK { $$ = $F; }
    | TOKEN_LBRACE formula_equal[F] TOKEN_RBRACE { $$ = $F; }
    | TOKEN_LTAG formula_equal[F] TOKEN_RTAG { $$ = $F; }
    | TOKEN_INVERT formula_basic[F] { $$ = sat_formula_new_operation ( SAT_FORMULA_TAG_INVERSION, $F, NULL ); }
    | TOKEN_VARIABLE { $$ = sat_formula_new_literal ($1); }
    ;

%%
