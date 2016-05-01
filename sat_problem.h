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

#ifndef __sat_problem_h__
#define __sat_problem_h__

#include <glib.h>
#include <stdbool.h>

typedef struct sat_problem *SatProblem;

SatProblem sat_problem_new ();

void sat_problem_free (SatProblem *sat);

void sat_problem_add_clause_array (SatProblem sat, const char *const *clause);
void sat_problem_add_clause_glist (SatProblem sat, GList *clause);

void sat_problem_add_1ofn_order_encoding (SatProblem sat, GList *lit_list);
void sat_problem_add_1ofn_direct_encoding (SatProblem sat, GList *lit_list);
void sat_problem_add_2ofn_direct_encoding (SatProblem sat, GList *lit_list);

bool sat_problem_add_formula_mapping (SatProblem sat, const char* formula, GList *lit_mapping);

const char * sat_problem_get_varname_from_number (SatProblem sat, long int number);
long int sat_problem_get_varnumber_from_name (SatProblem sat, const char *name);
GList *sat_problem_get_clauses_mapped (SatProblem sat);

bool sat_problem_solve (SatProblem sat, const char *tmp_file_name, const char *solver_bin, bool solution_on_stdout, bool cleanup, bool cnf_gz);

bool sat_problem_satisfiable (SatProblem sat, bool *error);
bool sat_problem_var_result (SatProblem sat, const char *var, bool *error);

#endif
