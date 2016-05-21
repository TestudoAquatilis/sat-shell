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

#include "sat_base_cnf.h"
#include "pty_run.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <glib.h>
#include <zlib.h>

/* base cnf data struct */
struct base_cnf {
    /* maximum variable */
    unsigned long int max_var;

    /* GQueue containing clauses as 0-terminated arrays of literals as (long int) */
    GQueue *clauses;
    /* GSList containing solution literal values as (long int) - NULL if not solved */
    GSList *solution;
};

/* returns a newly allocated BaseCNF */
struct base_cnf * base_cnf_new ()
{
    struct base_cnf *result = g_slice_new (struct base_cnf);
    if (result == NULL) return NULL;

    result->max_var = 0;
    result->clauses  = g_queue_new ();
    if (result->clauses == NULL) return NULL;
    result->solution = NULL;

    return result;
}

/* frees data of BaseCNF and sets the referenced pointer to NULL */
void base_cnf_free (struct base_cnf **cnf)
{
    if (cnf == NULL) return;

    struct base_cnf *rcnf = *cnf;
    if (rcnf == NULL) return;

    if (rcnf->solution != NULL) {
        g_slist_free (rcnf->solution);
    }

    if (rcnf->clauses != NULL) {
        for (GList *cl = rcnf->clauses->head; cl != NULL; cl = cl->next) {
            long int *clause = (long int *) cl->data;
            if (clause != NULL) {
                int len = 0;
                while (clause[len] != 0) len++;
                g_slice_free1 (sizeof (long int) * (len + 1), clause);
            }
            cl->data = NULL;
        }
        g_queue_free (rcnf->clauses);
    }

    g_slice_free (struct base_cnf, rcnf);

    *cnf = NULL;
}

/* frees existing solution */
static void base_cnf_clear_solution (struct base_cnf *cnf)
{
    if (cnf->solution != NULL) {
        g_slist_free (cnf->solution);
    }
    cnf->solution = NULL;
}

/* adds a clause to *cnf given as 0-terminated array of literals as (long int) */
void base_cnf_add_clause_array (struct base_cnf *cnf, const long int *clause)
{
    if (cnf == NULL) return;
    if (clause == NULL) return;

    unsigned int len = 0;
    while (clause[len] != 0) len++;
    if (len == 0) return;

    long int *new_clause = g_slice_alloc (sizeof (long int) * (len + 1));

    for (int i = 0; i < len; i++) {
        long int i_lit = clause[i];
        new_clause[i] = i_lit;

        if (i_lit < 0) i_lit = -i_lit;

        if (i_lit > cnf->max_var) cnf->max_var = i_lit;
    }
    new_clause[len] = 0;

    g_queue_push_tail (cnf->clauses, (gpointer) new_clause);

    base_cnf_clear_solution (cnf);
}

/* adds a clause to *cnf given as GSList of literals as (long int) */
void base_cnf_add_clause_gslist (BaseCNF cnf, GSList *clause)
{
    if (cnf == NULL) return;
    if (clause == NULL) return;

    unsigned int len = g_slist_length (clause);
    if (len == 0) return;

    long int *new_clause = g_slice_alloc (sizeof (long int) * (len + 1));

    int i = 0;
    for (GSList *li = clause; li != NULL; li = li->next, i++) {
        long int i_lit = GPOINTER_TO_SIZE (li->data);
        new_clause[i] = i_lit;

        if (i_lit < 0) i_lit = -i_lit;

        if (i_lit > cnf->max_var) cnf->max_var = i_lit;
    }
    new_clause[len] = 0;

    g_queue_push_tail (cnf->clauses, (gpointer) new_clause);

    base_cnf_clear_solution (cnf);
}

/* adds a clause to *cnf given as GQueue of literals as (long int) */
void base_cnf_add_clause_gqueue (BaseCNF cnf, GQueue *clause)
{
    if (cnf == NULL) return;
    if (clause == NULL) return;

    unsigned int len = g_queue_get_length (clause);
    if (len == 0) return;

    long int *new_clause = g_slice_alloc (sizeof (long int) * (len + 1));

    int i = 0;
    for (GList *li = clause->head; li != NULL; li = li->next, i++) {
        long int i_lit = GPOINTER_TO_SIZE (li->data);
        new_clause[i] = i_lit;

        if (i_lit < 0) i_lit = -i_lit;

        if (i_lit > cnf->max_var) cnf->max_var = i_lit;
    }
    new_clause[len] = 0;

    g_queue_push_tail (cnf->clauses, (gpointer) new_clause);

    base_cnf_clear_solution (cnf);
}

/* prints a clause given as 0-terminated array of literals into file in DIMACS format */
static void base_cnf_print_dimacs_clause (FILE *file, const long int *clause)
{
    if (file == NULL) return;
    if (clause == NULL) return;

    int i = 0;

    while (clause[i] != 0) {
        fprintf (file, "%ld ", clause[i]);
        i++;
    }

    fprintf (file, "0\n");
}

/* prints a clause given as 0-terminated array of literals into gzip file in DIMACS format */
static void base_cnf_print_dimacs_clause_gz (gzFile file, const long int *clause)
{
    if (file == NULL) return;
    if (clause == NULL) return;

    int i = 0;

    while (clause[i] != 0) {
        gzprintf (file, "%ld ", clause[i]);
        i++;
    }

    gzprintf (file, "0\n");
}

/* prints a DIMACS header into file for a formula with max_var variables and num_clauses clauses */
static void base_cnf_print_dimacs_header (FILE *file, unsigned long int max_var, unsigned long int num_clauses)
{
    if (file == NULL) return;

    fprintf (file, "p cnf %ld %ld\n", max_var, num_clauses);
}

/* prints a DIMACS header into gzip file for a formula with max_var variables and num_clauses clauses */
static void base_cnf_print_dimacs_header_gz (gzFile file, unsigned long int max_var, unsigned long int num_clauses)
{
    if (file == NULL) return;

    gzprintf (file, "p cnf %ld %ld\n", max_var, num_clauses);
}

/* prints a DIMACS file for the formula represented by *cnf */
static void base_cnf_print_dimacs (struct base_cnf *cnf, FILE *file)
{
    if (file == NULL) return;
    if (cnf == NULL) return;

    base_cnf_print_dimacs_header (file, cnf->max_var, g_queue_get_length (cnf->clauses));

    for (GList *cl = cnf->clauses->head; cl != NULL; cl = cl->next) {
        const long int *clause = (long int *) cl->data;
        base_cnf_print_dimacs_clause (file, clause);
    }
}

/* prints a gzip DIMACS file for the formula represented by *cnf */
static void base_cnf_print_dimacs_gz (struct base_cnf *cnf, gzFile file)
{
    if (file == NULL) return;
    if (cnf == NULL) return;

    base_cnf_print_dimacs_header_gz (file, cnf->max_var, g_queue_get_length (cnf->clauses));

    for (GList *cl = cnf->clauses->head; cl != NULL; cl = cl->next) {
        const long int *clause = (long int *) cl->data;
        base_cnf_print_dimacs_clause_gz (file, clause);
    }
}

/* runs sat solver with binary path given by solver_binary, and cnf file given by filename_cnf
 * solution is written to file given by filename_sol; if solution_on_stdout is true, solver is assumed
 * to print solution onto stdout, otherwise into the file given as second argument.
 * returns true on success, false otherwise */
static bool base_cnf_run_solver (const char *solver_binary, const char *filename_cnf, const char *filename_sol, bool solution_on_stdout)
{
    if (solver_binary == NULL) return false;
    if (filename_cnf == NULL) return false;
    if (filename_sol == NULL) return false;

    bool result = true;

    FILE *sol_file = NULL;

    GSList *solver_execlist = NULL;
    solver_execlist = g_slist_prepend (solver_execlist, (char *) solver_binary);
    solver_execlist = g_slist_prepend (solver_execlist, (char *) filename_cnf);

    if (solution_on_stdout) {
        sol_file = fopen (filename_sol, "w");
        if (sol_file == NULL) {
            g_slist_free (solver_execlist);
            return false;
        }
    } else {
        solver_execlist = g_slist_prepend (solver_execlist, (char *) filename_sol);
    }
    solver_execlist = g_slist_reverse (solver_execlist);

    PTYRunData solver_run_data = pty_run_new (solver_execlist);
    g_slist_free (solver_execlist);

    if (solver_run_data == NULL) {
        printf ("ERROR: could not execute %s\n", solver_binary);
        if (sol_file != NULL) fclose (sol_file);
        return false;
    }

    bool read_on    = true;
    bool write_line = false;
    bool print_line = false;

    while (read_on) {
        const char *in_buff = pty_run_getline (solver_run_data);
        if (in_buff != NULL) {
            if (solution_on_stdout) {
                int len = strlen (in_buff);

                print_line = true;
                write_line = false;

                if (len > 1) {
                    if (in_buff[0] == 's') {
                        write_line = true;
                    }
                    if (in_buff[0] == 'v') {
                        write_line = true;
                        print_line = false;
                    }
                }

                if (print_line) {
                    printf ("SOLVER: %s\n", in_buff);
                }
                if (write_line) {
                    fprintf (sol_file, "%s\n", &in_buff[2]);
                }

            } else {
                printf ("SOLVER: %s\n", in_buff);
            }
        } else {
            read_on = false;
        }
    }

    pty_run_finish (&solver_run_data);

    if (sol_file != NULL) fclose (sol_file);
    return result;
}

/* read a solution from file into *cnf */
static void base_cnf_read_sol (struct base_cnf *cnf, FILE *file)
{
    if (cnf == NULL) return;
    if (file == NULL) return;

    base_cnf_clear_solution (cnf);

    /* satisfiable? */
    char *sat_line;
    int match = fscanf (file, "%ms", &sat_line);
    bool satisfiable = true;

    if (match == 1) {
        if (strcmp ("SAT", sat_line) != 0) {
            if (strcmp ("SATISFIABLE", sat_line) != 0) {
                satisfiable = false;
            }
        }
        free (sat_line);
    } else {
        satisfiable = false;
    }

    if (!satisfiable) {
        printf ("INFO: not satisfiable\n");
        return;
    }

    /* solution */
    GSList *lit_list = NULL;

    while (true) {
        long int literal;
        int match = fscanf (file, "%ld", &literal);
        if (match != 1) break;
        if (literal == 0) break;

        lit_list = g_slist_prepend (lit_list, GSIZE_TO_POINTER (literal));
    }

    lit_list = g_slist_reverse (lit_list);
    cnf->solution = lit_list;
}

/* solves cnf, returns true on successful run, false if an error occurred.
 * temporary files ar prefixed with tmp_file_name, solver binary solver_bin is used,
 * solution_on_stdout: if true it is assumed that solver prints solution on stdout,
 * cleanup: if true remove temporary files when finished,
 * cnf_gz: if true use gzipped dimacs for cnf file */
bool base_cnf_solve (struct base_cnf *cnf, const char *tmp_file_name, const char *solver_bin, bool solution_on_stdout, bool cleanup, bool cnf_gz)
{
    if (cnf == NULL) return false;
    if (tmp_file_name == NULL) return false;

    size_t file_name_len = strlen (tmp_file_name) + 8;

    /* writing cnf */
    char *cnf_file_name = g_slice_alloc (sizeof (char) * file_name_len);

    FILE  *cnf_file    = NULL;
    gzFile cnf_file_gz = NULL;

    if (!cnf_gz) {
        snprintf (cnf_file_name, file_name_len, "%s.cnf", tmp_file_name);

        cnf_file = fopen (cnf_file_name, "w");
        if (cnf_file == NULL) {
            printf ("ERROR: could not open file %s\n", cnf_file_name);
            g_slice_free1 (sizeof (char) * file_name_len, cnf_file_name);
            return false;
        }
    } else {
        snprintf (cnf_file_name, file_name_len, "%s.cnf.gz", tmp_file_name);

        cnf_file_gz = gzopen (cnf_file_name, "w");
        if (cnf_file_gz == NULL) {
            printf ("ERROR: could not open file %s\n", cnf_file_name);
            g_slice_free1 (sizeof (char) * file_name_len, cnf_file_name);
            return false;
        }
    }

    printf ("INFO: writing cnf file...\n");

    if (!cnf_gz) {
        base_cnf_print_dimacs (cnf, cnf_file);

        fclose (cnf_file);
    } else {
        base_cnf_print_dimacs_gz (cnf, cnf_file_gz);

        gzclose (cnf_file_gz);
    }

    /* solve */
    char *sol_file_name = g_slice_alloc (sizeof (char) * file_name_len);

    snprintf (sol_file_name, file_name_len, "%s.sol", tmp_file_name);

    if (solver_bin == NULL) {
        solver_bin         = "minisat";
        solution_on_stdout = false;
    }
    printf ("INFO: running solver (%s)...\n", solver_bin);
    bool success = base_cnf_run_solver (solver_bin, cnf_file_name, sol_file_name, solution_on_stdout);

    if (!success) {
        if (cleanup) remove (cnf_file_name);
        g_slice_free1 (sizeof (char) * file_name_len, cnf_file_name);
        g_slice_free1 (sizeof (char) * file_name_len, sol_file_name);
        return false;
    }

    /* reading solution */
    FILE *sol_file = fopen (sol_file_name, "r");
    if (sol_file == NULL) {
        printf ("ERROR: could not open file %s\n", sol_file_name);
        if (cleanup) remove (cnf_file_name);
        g_slice_free1 (sizeof (char) * file_name_len, sol_file_name);
        g_slice_free1 (sizeof (char) * file_name_len, cnf_file_name);
        return false;
    }

    printf ("INFO: reading solution...\n");
    base_cnf_read_sol (cnf, sol_file);

    fclose (sol_file);

    /* freeing stuff */
    if (cleanup) remove (cnf_file_name);
    if (cleanup) remove (sol_file_name);
    g_slice_free1 (sizeof (char) * file_name_len, cnf_file_name);
    g_slice_free1 (sizeof (char) * file_name_len, sol_file_name);

    return true;
}

/* make current solution of cnf invalid to optain another one on next solving */
void base_cnf_cancel_solution (struct base_cnf *cnf)
{
    if (cnf == NULL) return;

    GSList *cancel_clause = NULL;

    for (GSList *li = cnf->solution; li != NULL; li = li->next) {
        long int lit = -GPOINTER_TO_SIZE (li->data);

        cancel_clause = g_slist_prepend (cancel_clause, GSIZE_TO_POINTER (lit));
    }

    base_cnf_add_clause_gslist (cnf, cancel_clause);

    g_slist_free (cancel_clause);
}

/* return internal clauses of cnf as GQueue of 0-terminated arrays of literals as (long int).
 * returned GQueue should not be modified. */
GQueue* base_cnf_clauses (struct base_cnf *cnf)
{
    if (cnf == NULL) return NULL;
    return cnf->clauses;
}

/* return solution (if satisfiable) or NULL if not or not yet solved as GSList of literals as (long int).
 * returned GSList should not be modified. */
GSList * base_cnf_solution (struct base_cnf *cnf)
{
    if (cnf == NULL) return NULL;
    return cnf->solution;
}
