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

#ifndef __sat_formula_h__
#define __sat_formula_h__

#include <glib.h>

/* tag for sat formula syntax tree elements */
enum sat_formula_tag_t {
    /* literal: literal contains value */
    SAT_FORMULA_TAG_LITERAL,
    /* inversion: left operand is inverted, right operand is NULL */
    SAT_FORMULA_TAG_INVERSION,
    /* two-operand operations */
    SAT_FORMULA_TAG_OP_OR,
    SAT_FORMULA_TAG_OP_AND,
    SAT_FORMULA_TAG_OP_XOR,
    SAT_FORMULA_TAG_OP_EQUAL,
    SAT_FORMULA_TAG_OP_RIMPL,
    SAT_FORMULA_TAG_OP_LIMPL
};

typedef enum sat_formula_tag_t SatFormulaTag;
typedef struct sat_formula *SatFormula;

/* allocate and return a new sat_formula element representing the given literal */
struct sat_formula * sat_formula_new_literal (long int literal);
/* allocate and return a new sat_formula element representing the inversion of given argument */
struct sat_formula * sat_formula_new_inversion (SatFormula arg);
/* allocate and return a new sat_formula element representing the given operation with given operands */
struct sat_formula * sat_formula_new_operation (SatFormulaTag tag, SatFormula left, SatFormula right);
/* recursively free the given sat_formula tree */
void sat_formula_free (SatFormula *fpointer);

/* recursively print the given sat_formula */
void sat_formula_print (SatFormula formula);
/* parse and return the given formula into an allocated sat_formula tree */
SatFormula sat_formula_parse (const char *expr);

/* recursively duplicate the given sat_formula tree */
SatFormula sat_formula_duplicate (SatFormula formula);
/* transforms and returns a given sat_formula tree into a CNF formula represented
 * as a GList of clauses, where each clause is a GQueue of literals as long int */
GList * sat_formula_to_cnf (SatFormula formula);

#endif
