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

#include "sat_base_cnf.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <glib.h>
#include <zlib.h>

struct base_cnf {
    unsigned long int max_literal;
    unsigned long int num_clauses;

    GList *clauses;
    GList *solution;
};

struct base_cnf * base_cnf_new ()
{
    struct base_cnf *result = (struct base_cnf *) malloc (sizeof (struct base_cnf));

    result->max_literal = 0;
    result->num_clauses = 0;
    result->clauses  = NULL;
    result->solution = NULL;

    return result;
}

void base_cnf_free (struct base_cnf **cnf)
{
    if (cnf == NULL) return;

    struct base_cnf *rcnf = *cnf;
    if (rcnf == NULL) return;

    if (rcnf->solution != NULL) {
        g_list_free (rcnf->solution);
    }

    if (rcnf->clauses != NULL) {
        for (GList *cl = rcnf->clauses; cl != NULL; cl = cl->next) {
            long int *clause = (long int *) cl->data;
            if (clause != NULL) free (clause);
            cl->data = NULL;
        }
        g_list_free (rcnf->clauses);
    }

    free (rcnf);

    *cnf = NULL;
}

static void base_cnf_clear_solution (struct base_cnf *cnf)
{
    if (cnf->solution != NULL) {
        g_list_free (cnf->solution);
    }
    cnf->solution = NULL;
}

void base_cnf_add_clause_array (struct base_cnf *cnf, const long int *clause)
{
    if (cnf == NULL) return;
    if (clause == NULL) return;

    unsigned int len = 0;
    while (clause[len] != 0) len++;
    if (len == 0) return;

    long int *new_clause = malloc (sizeof (long int) * (len + 1));

    for (int i = 0; i < len; i++) {
        long int i_lit = clause[i];
        new_clause[i] = i_lit;

        if (i_lit < 0) i_lit = -i_lit;
        
        if (i_lit > cnf->max_literal) cnf->max_literal = i_lit;
    }
    new_clause[len] = 0;

    cnf->clauses = g_list_prepend (cnf->clauses, (gpointer) new_clause);
    cnf->num_clauses++;

    base_cnf_clear_solution (cnf);
}

void base_cnf_add_clause_glist (BaseCNF cnf, GList *clause)
{
    if (cnf == NULL) return;
    if (clause == NULL) return;

    unsigned int len = g_list_length (clause);
    if (len == 0) return;

    long int *new_clause = malloc (sizeof (long int) * (len + 1));

    int i = 0;
    for (GList *li = clause; li != NULL; li = li->next, i++) {
        long int i_lit = GPOINTER_TO_INT (li->data);
        new_clause[i] = i_lit;

        if (i_lit < 0) i_lit = -i_lit;
        
        if (i_lit > cnf->max_literal) cnf->max_literal = i_lit;
    }
    new_clause[len] = 0;

    cnf->clauses = g_list_prepend (cnf->clauses, (gpointer) new_clause);
    cnf->num_clauses++;

    base_cnf_clear_solution (cnf);
}

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

static void base_cnf_print_dimacs_header (FILE *file, unsigned long int max_literal, unsigned long int num_clauses)
{
    if (file == NULL) return;

    fprintf (file, "p cnf %ld %ld\n", max_literal, num_clauses);
}

static void base_cnf_print_dimacs_header_gz (gzFile file, unsigned long int max_literal, unsigned long int num_clauses)
{
    if (file == NULL) return;

    gzprintf (file, "p cnf %ld %ld\n", max_literal, num_clauses);
}

static void base_cnf_print_dimacs (struct base_cnf *cnf, FILE *file)
{
    if (file == NULL) return;
    if (cnf == NULL) return;

    base_cnf_print_dimacs_header (file, cnf->max_literal, cnf->num_clauses);

    for (GList *cl = cnf->clauses; cl != NULL; cl = cl->next) {
        const long int *clause = (long int *) cl->data;
        base_cnf_print_dimacs_clause (file, clause);
    }
}

static void base_cnf_print_dimacs_gz (struct base_cnf *cnf, gzFile file)
{
    if (file == NULL) return;
    if (cnf == NULL) return;

    base_cnf_print_dimacs_header_gz (file, cnf->max_literal, cnf->num_clauses);

    for (GList *cl = cnf->clauses; cl != NULL; cl = cl->next) {
        const long int *clause = (long int *) cl->data;
        base_cnf_print_dimacs_clause_gz (file, clause);
    }
}

static bool base_cnf_run_solver (const char *solver_binary, const char *filename_cnf, const char *filename_sol, bool solution_on_stdout)
{
    if (solver_binary == NULL) return false;
    if (filename_cnf == NULL) return false;
    if (filename_sol == NULL) return false;

    GString *cmd_str = g_string_new (NULL);
    bool result = true;

    FILE *sol_file = NULL;

    if (solution_on_stdout) {
        sol_file = fopen (filename_sol, "w");
        if (sol_file == NULL) {
            return false;
        }
        g_string_printf (cmd_str, "%s %s", solver_binary, filename_cnf);
    } else {
        g_string_printf (cmd_str, "%s %s %s", solver_binary, filename_cnf, filename_sol);
    }

    FILE *sat_pipe;

    sat_pipe = popen (cmd_str->str, "r");
    if (sat_pipe == NULL) {
        printf ("ERROR: could not execute %s\n", cmd_str->str);
        g_string_free (cmd_str, true);
        if (sol_file != NULL) fclose (sol_file);
        return false;
    }

    bool read_on    = true;
    bool new_line   = true;
    bool write_line = false;
    bool print_line = false;

    while (read_on) {
        const size_t buff_len = 1024;
        char in_buff[buff_len];
        if (fgets (in_buff, buff_len, sat_pipe) != 0) {
            if (solution_on_stdout) {
                int len = strlen (in_buff);
                if (new_line) {
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
                }

                if (print_line) {
                    printf ("SOLVER: %s", in_buff);
                }
                if (write_line) {
                    if (new_line) {
                        fprintf (sol_file, "%s", &in_buff[2]);
                    } else {
                        fprintf (sol_file, "%s", &in_buff[0]);
                    }
                }

                if (in_buff[len-1] == '\n') {
                    new_line = true;
                } else {
                    new_line = false;
                }
            } else {
                printf ("SOLVER: %s", in_buff);
            }
        } else {
            read_on = false;
        }
    }

    pclose (sat_pipe);

    if (sol_file != NULL) fclose (sol_file);
    g_string_free (cmd_str, true);
    return result;
}

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
    GList *lit_list = NULL;

    while (true) {
        long int literal;
        int match = fscanf (file, "%ld", &literal);
        if (match != 1) break;
        if (literal == 0) break;

        lit_list = g_list_prepend (lit_list, GINT_TO_POINTER (literal));
    }

    lit_list = g_list_reverse (lit_list);
    cnf->solution = lit_list;
}

bool base_cnf_solve (struct base_cnf *cnf, const char *tmp_file_name, const char *solver_bin, bool solution_on_stdout, bool cleanup, bool cnf_gz)
{
    if (cnf == NULL) return false;
    if (tmp_file_name == NULL) return false;

    size_t file_name_len = strlen (tmp_file_name) + 8;

    /* writing cnf */
    char *cnf_file_name = malloc (sizeof (char) * file_name_len);

    FILE  *cnf_file    = NULL;
    gzFile cnf_file_gz = NULL;

    if (!cnf_gz) {
        snprintf (cnf_file_name, file_name_len, "%s.cnf", tmp_file_name);

        cnf_file = fopen (cnf_file_name, "w");
        if (cnf_file == NULL) {
            printf ("ERROR: could not open file %s\n", cnf_file_name);
            free (cnf_file_name);
            return false;
        }
    } else {
        snprintf (cnf_file_name, file_name_len, "%s.cnf.gz", tmp_file_name);

        cnf_file_gz = gzopen (cnf_file_name, "w");
        if (cnf_file_gz == NULL) {
            printf ("ERROR: could not open file %s\n", cnf_file_name);
            free (cnf_file_name);
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
    char *sol_file_name = malloc (sizeof (char) * file_name_len);

    snprintf (sol_file_name, file_name_len, "%s.sol", tmp_file_name);

    if (solver_bin == NULL) {
        solver_bin         = "minisat";
        solution_on_stdout = false;
    }
    printf ("INFO: running solver (%s)...\n", solver_bin);
    bool success = base_cnf_run_solver (solver_bin, cnf_file_name, sol_file_name, solution_on_stdout);

    if (!success) {
        if (cleanup) remove (cnf_file_name);
        free (cnf_file_name);
        free (sol_file_name);
        return false;
    }

    /* reading solution */
    FILE *sol_file = fopen (sol_file_name, "r");
    if (sol_file == NULL) {
        printf ("ERROR: could not open file %s\n", sol_file_name);
        if (cleanup) remove (cnf_file_name);
        free (sol_file_name);
        free (cnf_file_name);
        return false;
    }

    printf ("INFO: reading solution...\n");
    base_cnf_read_sol (cnf, sol_file);

    fclose (sol_file);

    /* freeing stuff */
    if (cleanup) remove (cnf_file_name);
    if (cleanup) remove (sol_file_name);
    free (cnf_file_name);
    free (sol_file_name);

    return true;
}

GList * base_cnf_clauses (struct base_cnf *cnf)
{
    if (cnf == NULL) return NULL;
    GList *result = g_list_copy (cnf->clauses);
    result = g_list_reverse (result);
    return result;
}

GList * base_cnf_solution (struct base_cnf *cnf)
{
    if (cnf == NULL) return NULL;
    return cnf->solution;
}
