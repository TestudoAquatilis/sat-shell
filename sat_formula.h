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

#ifndef __sat_formula_h__
#define __sat_formula_h__

#include <glib.h>

enum sat_formula_tag_t {
    SAT_FORMULA_TAG_LITERAL,
    SAT_FORMULA_TAG_INVERSION,
    SAT_FORMULA_TAG_OP_OR,
    SAT_FORMULA_TAG_OP_AND,
    SAT_FORMULA_TAG_OP_XOR,
    SAT_FORMULA_TAG_OP_EQUAL,
    SAT_FORMULA_TAG_OP_RIMPL,
    SAT_FORMULA_TAG_OP_LIMPL
};

typedef enum sat_formula_tag_t SatFormulaTag;
typedef struct sat_formula *SatFormula;

struct sat_formula * sat_formula_new_literal (long int literal);
struct sat_formula * sat_formula_new_operation (SatFormulaTag tag, SatFormula left, SatFormula right);
void sat_formula_free (SatFormula *fpointer);

void sat_formula_print (SatFormula formula);
SatFormula sat_formula_parse (const char *expr);

SatFormula sat_formula_duplicate (SatFormula formula);
GList * sat_formula_to_cnf (SatFormula formula);

#endif
