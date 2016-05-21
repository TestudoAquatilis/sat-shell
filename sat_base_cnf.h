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

#ifndef __sat_base_cnf_h__
#define __sat_base_cnf_h__

#include <stdbool.h>
#include <glib.h>

typedef struct base_cnf *BaseCNF;

/* returns a newly allocated BaseCNF */
BaseCNF base_cnf_new ();

/* frees data of BaseCNF and sets the referenced pointer to NULL */
void base_cnf_free (BaseCNF *cnf);

/* adds a clause to cnf given as 0-terminated array of literals as (long int) */
void base_cnf_add_clause_array  (BaseCNF cnf, const long int *clause);
/* adds a clause to cnf given as GSList of literals as (long int) */
void base_cnf_add_clause_gslist  (BaseCNF cnf, GSList *clause);
/* adds a clause to cnf given as GQueue of literals as (long int) */
void base_cnf_add_clause_gqueue (BaseCNF cnf, GQueue *clause);

/* solves cnf, returns true on successful run, false if an error occurred.
 * temporary files ar prefixed with tmp_file_name, solver binary solver_bin is used,
 * solution_on_stdout: if true it is assumed that solver prints solution on stdout,
 * cleanup: if true remove temporary files when finished,
 * cnf_gz: if true use gzipped dimacs for cnf file */
bool base_cnf_solve (BaseCNF cnf, const char *tmp_file_name, const char *solver_bin, bool solution_on_stdout, bool cleanup, bool cnf_gz);
/* make current solution of cnf invalid to optain another one on next solving */
void base_cnf_cancel_solution (BaseCNF cnf);

/* return internal clauses of cnf as GQueue of 0-terminated arrays of literals as (long int).
 * returned GQueue should not be modified */
GQueue * base_cnf_clauses (BaseCNF cnf);
/* return solution (if satisfiable) or NULL if not or not yet solved as GSList of literals as (long int).
 * returned GSList should not be modified. */
GSList * base_cnf_solution (BaseCNF cnf);

#endif
