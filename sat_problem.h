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

#ifndef __sat_problem_h__
#define __sat_problem_h__

#include <glib.h>
#include <stdbool.h>

typedef struct sat_problem *SatProblem;

/* allocate and return a new sat_problem */
SatProblem sat_problem_new ();

/* free existing sat_problem *sat */
void sat_problem_free (SatProblem *sat);

/* add a clause to sat represented as a NULL-terminated array of variables (const char *) */
void sat_problem_add_clause_array  (SatProblem sat, const char *const *clause);
/* add a clause to sat represented as a GSList of variables (const char *) */
void sat_problem_add_clause_gslist (SatProblem sat, GSList *clause);

/* apply 1 of n order encoding to literals (const char *) in lit_list. */
void sat_problem_add_1ofn_order_encoding (SatProblem sat, GSList *lit_list);
/* apply (at least/most) m of n direct encoding to literals (const char *) in lit_list. */
void sat_problem_add_mofn_direct_encoding (SatProblem sat, GSList *lit_list, unsigned int m, bool atleast, bool atmost);

/* add a formula as mapping and return true on success.
 * formula: the formula represented as string with variables 1 ... n.
 * lit_mapping: list of literals (const char *) to map to variables in given formula.
 * parsed formulas are cached for making multiple usage more efficient as only mapping
 * needs to be applied individually whereas parsing only needs to be done once. */
bool sat_problem_add_formula_mapping (SatProblem sat, const char* formula, GSList *lit_mapping);

/* lookup variable integer mapping and return mapped string */
const char * sat_problem_get_varname_from_number (SatProblem sat, long int number);
/* lookup variable string and return mapped integer */
long int sat_problem_get_varnumber_from_name (SatProblem sat, const char *name);
/* get the internal data structure of all currently mapped clauses
 * as a GQueue of 0-terminated literal (long int) arrays.
 * the result should not be modified. */
GQueue *sat_problem_get_clauses_mapped (SatProblem sat);

/* solve current sat problem and return false on errors, true otherwise.
 * tmp_file_name: prefix for temporary files for solver,
 * solver_bin: binary to execute,
 * solution_on_stdout: if true, assume that solver prints result on stdout,
 *   otherwise to filename given as second argument,
 * cleanup: if true, temporary files will be removed afterwards,
 * cnf_gz: CNF file will be in gzipped dimacs, otherwise in plain dimacs. */
bool sat_problem_solve (SatProblem sat, const char *tmp_file_name, const char *solver_bin, bool solution_on_stdout, bool cleanup, bool cnf_gz);
/* invalidate current solution to obtain a different one on next solve */
void sat_problem_cancel_solution (SatProblem sat);

/* return true, if problem is satisfiable. *error is set to true in
 * case of errors: e.g. solver is not yet run. */
bool sat_problem_satisfiable (SatProblem sat, bool *error);
/* obtain assigned boolean value to variable var.
 * In case of errors (e.g. unknown variable, not satisfiable, ...)
 * *error is set to true. */
bool sat_problem_var_result (SatProblem sat, const char *var, bool *error);
/* obtain a GList of all variables (const char *) assigned to var_assignment.
 * In case of errors (e.g. not satisfiable, ...)
 * *error is set to true. */
GSList *sat_problem_var_result_list (SatProblem sat, bool var_assignment, bool *error);

#endif
