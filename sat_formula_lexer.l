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

#include <stdio.h>

/* skip unused functions */
#define YY_NO_UNPUT
#define YY_NO_INPUT
 
%}

%option outfile="sat_formula_lexer.c" header-file="sat_formula_lexer.h"
%option warn nodefault
 
%option reentrant noyywrap never-interactive nounistd
%option bison-bridge
 
LPAREN      "("
RPAREN      ")"
LBRACK      "["
RBRACK      "]"
LBRACE      "{"
RBRACE      "}"
LTAG        "<"
RTAG        ">"

INVERT      "!"|"-"|"~"|"not"|"NOT"
AND         "*"|"&&"|"&"|"and"|"AND"
OR          "+"|"||"|"|"|"or"|"OR"
XOR         "^"|"!="|"xor"|"XOR"
EQUAL       "<->"|"<=>"|"="|"=="|"xnor"|"XNOR"
RIMPL       "->"|"=>"
LIMPL       "<-"|"<="
 
NUMBER      [0-9]+
WS          [ \r\n\t]*
 
%%
 
{WS}            { /* Skip blanks. */ }
{NUMBER}        { sscanf(yytext, "%uld", &yylval->literal); return TOKEN_LITERAL; }
 
{LPAREN} { return TOKEN_LPAREN; }
{RPAREN} { return TOKEN_RPAREN; }
{LBRACK} { return TOKEN_LBRACK; }
{RBRACK} { return TOKEN_RBRACK; }
{LBRACE} { return TOKEN_LBRACE; }
{RBRACE} { return TOKEN_RBRACE; }
{LTAG}   { return TOKEN_LTAG;   }
{RTAG}   { return TOKEN_RTAG;   }

{INVERT} { return TOKEN_INVERT; }
{AND}    { return TOKEN_AND;    }
{OR}     { return TOKEN_OR;     }
{XOR}    { return TOKEN_XOR;    }
{EQUAL}  { return TOKEN_EQUAL;  }
{RIMPL}  { return TOKEN_RIMPL;  }
{LIMPL}  { return TOKEN_LIMPL;  }
.        { return TOKEN_ERROR;  }
 
%%
/* 
static int yyerror(const char *msg) {
    fprintf(stderr, "Error:%s\n", msg); return 0;
}
*/
