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

#ifndef __sat_base_cnf_h__
#define __sat_base_cnf_h__

#include <stdbool.h>
#include <glib.h>

typedef struct base_cnf *BaseCNF;

BaseCNF base_cnf_new ();
void base_cnf_free (BaseCNF *cnf);

void base_cnf_add_clause_array (BaseCNF cnf, const long int *clause);
void base_cnf_add_clause_glist (BaseCNF cnf, GList *clause);

bool base_cnf_solve (BaseCNF cnf, const char *tmp_file_name, const char *solver_bin, bool solution_on_stdout, bool cleanup, bool cnf_gz);

GList * base_cnf_clauses (BaseCNF cnf);
GList * base_cnf_solution (BaseCNF cnf);

#endif
