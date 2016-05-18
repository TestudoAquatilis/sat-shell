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

#include "sat_problem.h"
#include "sat_formula.h"
#include "sat_base_cnf.h"

#include <stdio.h>
#include <string.h>

/* highlevel sat_problem data struct */
struct sat_problem {
    /* current formula mapped to enumerated variables represented as BaseCNF */
    BaseCNF cnf;
    /* greatest mapped variable number in use */
    long int last_var;

    /* strings used for variable mapping in other data structures */
    GStringChunk *str_literals;

    /* hash table mapping variable names (char *) to mapped integers (long int) */
    GHashTable   *tbl_lit_name_to_int;
    /* hash table for unmapping variables as integers (long int) to their names (char *) */
    GHashTable   *tbl_lit_int_to_name;

    /* true, if sat solver was executed */
    bool         solver_run;
    /* true, if problem is satisfiable */
    bool         satisfiable;
    /* hash table mapping variable names (char *) to their boolean assigned value (bool) */
    GHashTable   *tbl_var_result;

    /* strings used for caching of parsed formulas */
    GStringChunk *str_formulas;
    /* hash table for cached parsed formulas mapping string representation (char *) to
     * a GList of clauses where a clause is a GQueue of literals (long int) */
    GHashTable   *formula_to_cnf_cache;

    /* counter for temporary variables generated for 1ofn order encoding */
    int special_coding_iterator_1ofn;
};

/* user data for generating result list from GHashTable with
 * sat_problem_var_result_list_gen_func */
struct sat_problem_var_result_list_gen_user_data {
    bool    assignment;
    GSList *result;
};

/* remove solution from sat_problem e.g. when a new clause is added */
static void sat_problem_clear_solution (struct sat_problem *sat);
/* encode literal from string to integer representation and return the result */
static long int sat_problem_encode_literal (struct sat_problem *sat, const char *literal);
/* iterator function for generating result list for sat_problem_var_result_list 
 * from GHashTable */
static void sat_problem_var_result_list_gen_func (gpointer key, gpointer value, gpointer user_data);


/* allocate and return a new sat_problem */
struct sat_problem * sat_problem_new ()
{
    struct sat_problem *result;

    result = g_slice_new (struct sat_problem);
    if (result == NULL) return NULL;

    result->cnf         = base_cnf_new ();
    result->last_var    = 0;
    result->solver_run  = false;
    result->satisfiable = false;

    result->str_literals         = NULL;
    result->str_formulas         = NULL;

    result->tbl_lit_name_to_int  = NULL;
    result->tbl_lit_int_to_name  = NULL;
    result->tbl_var_result       = NULL;
    result->formula_to_cnf_cache = NULL;

    result->str_literals = g_string_chunk_new (4096);
    result->str_formulas = g_string_chunk_new (4096);
    if ((result->str_literals == NULL) ||
        (result->str_formulas == NULL)) {
        sat_problem_free (&result);
        return NULL;
    }

    result->tbl_lit_name_to_int  = g_hash_table_new (g_str_hash, g_str_equal);
    result->tbl_lit_int_to_name  = g_hash_table_new (g_direct_hash, g_direct_equal);
    result->tbl_var_result       = g_hash_table_new (g_str_hash, g_str_equal);
    result->formula_to_cnf_cache = g_hash_table_new (g_str_hash, g_str_equal);

    if ((result->tbl_lit_name_to_int == NULL) || 
        (result->tbl_lit_int_to_name == NULL) || 
        (result->tbl_var_result == NULL) ||
        (result->formula_to_cnf_cache == NULL)) {

        sat_problem_free (&result);
        return NULL;
    }

    result->special_coding_iterator_1ofn = 0;

    return result;
}

/* free existing sat_problem *sat */
void sat_problem_free (struct sat_problem **sat)
{
    if (sat == NULL) return;

    struct sat_problem *sp = *sat;
    if (sp == NULL) return;

    base_cnf_free (&(sp->cnf));

    if (sp->tbl_lit_name_to_int != NULL)  g_hash_table_destroy (sp->tbl_lit_name_to_int);
    if (sp->tbl_lit_int_to_name != NULL)  g_hash_table_destroy (sp->tbl_lit_int_to_name);
    if (sp->tbl_var_result != NULL)       g_hash_table_destroy (sp->tbl_var_result);
    if (sp->formula_to_cnf_cache != NULL) {
        GHashTableIter iter;
        gpointer key;
        gpointer value;
        g_hash_table_iter_init (&iter, sp->formula_to_cnf_cache);

        while (g_hash_table_iter_next (&iter, &key, &value)) {
            GList *clause_list = (GList *) value;
            g_list_free_full (clause_list, (GDestroyNotify) g_queue_free);
        }

        g_hash_table_destroy (sp->formula_to_cnf_cache);
    }

    if (sp->str_literals != NULL) g_string_chunk_free (sp->str_literals);
    if (sp->str_formulas != NULL) g_string_chunk_free (sp->str_formulas);

    g_slice_free (struct sat_problem, sp);
    *sat = NULL;
}

/* remove solution from sat_problem e.g. when a new clause is added */
static void sat_problem_clear_solution (struct sat_problem *sat)
{
    if (sat == NULL) return;
    if (sat->solver_run) {
        sat->solver_run  = false;
        sat->satisfiable = false;
        g_hash_table_remove_all (sat->tbl_var_result);
    }
}

/* encode literal from string to integer representation and return the result */
static long int sat_problem_encode_literal (struct sat_problem *sat, const char *literal)
{
    if (sat == NULL) return 0;
    if (literal == NULL) return 0;
    if (strlen (literal) == 0) return 0;

    /* remember and remove inversions */
    bool invert = false;
    while (literal[0] == '-') {
        invert = !invert;
        literal++;
        if (strlen (literal) == 0) return 0;
    }

    /* if already encoded: use existing encoding.
     * otherwise: create new mapping to next unused variable */
    gpointer lookup_val = NULL;
    if (g_hash_table_contains (sat->tbl_lit_name_to_int, literal)) {
        lookup_val = g_hash_table_lookup (sat->tbl_lit_name_to_int, literal);
    } else {
        sat->last_var++;
        lookup_val = GSIZE_TO_POINTER (sat->last_var);
        char *ins_literal = g_string_chunk_insert_const (sat->str_literals, literal);

        g_hash_table_insert (sat->tbl_lit_name_to_int, ins_literal, lookup_val);
        g_hash_table_insert (sat->tbl_lit_int_to_name, lookup_val, ins_literal);
    }
    if (lookup_val == NULL) {
        return 0;
    }

    long int result = GPOINTER_TO_SIZE (lookup_val);

    if (invert) result = -result;

    return result;
}

/* add a clause to sat represented as a GSList of variables (const char *) */
void sat_problem_add_clause_gslist (struct sat_problem *sat, GSList *clause)
{
    if (sat == NULL) return;
    if (clause == NULL) return;

    /* check for empty str */
    for (GSList *li = clause; li != NULL; li = li->next) {
        const char *lit_str = li->data;
        if (strlen (lit_str) == 0) return;
        if (lit_str[0] == '-') {
            if (strlen (lit_str) == 1) return;
        }
    }

    GSList *lit_list = NULL;

    for (GSList *li = clause; li != NULL; li = li->next) {
        long int literal = sat_problem_encode_literal (sat, li->data);
        lit_list = g_slist_prepend (lit_list, GSIZE_TO_POINTER (literal));
    }

    lit_list = g_slist_reverse (lit_list);

    base_cnf_add_clause_gslist (sat->cnf, lit_list);

    g_slist_free (lit_list);

    sat_problem_clear_solution (sat);
}

/* add a clause to sat represented as a NULL-terminated array of variables (const char *) */
void sat_problem_add_clause_array (struct sat_problem *sat, const char *const *clause)
{
    if (sat == NULL) return;
    if (clause == NULL) return;

    /* check for empty str */
    for (int i = 0; clause[i] != NULL; i++) {
        const char *lit_str = clause[i];
        if (strlen (lit_str) == 0) return;
        if (lit_str[0] == '-') {
            if (strlen (lit_str) == 1) return;
        }
    }

    GSList *lit_list = NULL;

    for (int i = 0; clause[i] != NULL; i++) {
        long int literal = sat_problem_encode_literal (sat, clause[i]);
        lit_list = g_slist_prepend (lit_list, GSIZE_TO_POINTER (literal));
    }

    lit_list = g_slist_reverse (lit_list);

    base_cnf_add_clause_gslist (sat->cnf, lit_list);

    g_slist_free (lit_list);

    sat_problem_clear_solution (sat);
}

/* apply 1 of n order encoding to literals (const char *) in lit_list. */
void sat_problem_add_1ofn_order_encoding (struct sat_problem *sat, GSList *lit_list)
{
    /* checks */
    if (sat == NULL) return;

    size_t n_lit = g_slist_length (lit_list);

    if (n_lit == 0) return;
    if (n_lit == 1) {
        sat_problem_add_clause_gslist (sat, lit_list);
        return;
    }

    /* helper variables and main variables */
    const char *h_var_template     = "_int_1ofn_%d_%d_";

    long int *main_array = g_slice_alloc (sizeof (long int) * 2*n_lit);
    long int *help_array = &(main_array[n_lit]);

    int i = 0;
    for (GSList *li = lit_list; li != NULL; li = li->next) {
        long int literal = sat_problem_encode_literal (sat, li->data);
        main_array[i] = literal;
        i++;
    }

    GString *temp_str = g_string_new (NULL);

    for (i = 0; i < n_lit - 1; i++) {
        g_string_printf (temp_str, h_var_template, sat->special_coding_iterator_1ofn, i);
        long int literal = sat_problem_encode_literal (sat, temp_str->str);
        help_array[i] = literal;
    }

    /* clauses */
    long int temp_clause[4];

    temp_clause[2] = 0;
    for (i = 0; i < n_lit - 2; i++) {
        temp_clause[0] = help_array[i];
        temp_clause[1] = -help_array[i+1];
        base_cnf_add_clause_array (sat->cnf, temp_clause);
    }

    i = 0;
    temp_clause[0] = main_array[i];
    temp_clause[1] = help_array[i];
    temp_clause[2] = 0;
    base_cnf_add_clause_array (sat->cnf, temp_clause);
    temp_clause[0] = -main_array[i];
    temp_clause[1] = -help_array[i];
    base_cnf_add_clause_array (sat->cnf, temp_clause);

    temp_clause[3] = 0;
    for (i = 1; i < n_lit-1; i++) {
        temp_clause[0] = -main_array[i];
        temp_clause[1] = help_array[i-1];
        temp_clause[2] = 0;
        base_cnf_add_clause_array (sat->cnf, temp_clause);
        temp_clause[0] = -main_array[i];
        temp_clause[1] = -help_array[i];
        base_cnf_add_clause_array (sat->cnf, temp_clause);
        temp_clause[0] = main_array[i];
        temp_clause[1] = -help_array[i-1];
        temp_clause[2] = help_array[i];
        base_cnf_add_clause_array (sat->cnf, temp_clause);
    }

    i = n_lit - 1;
    temp_clause[0] = -main_array[i];
    temp_clause[1] = help_array[i-1];
    temp_clause[2] = 0;
    base_cnf_add_clause_array (sat->cnf, temp_clause);
    temp_clause[0] = main_array[i];
    temp_clause[1] = -help_array[i-1];
    base_cnf_add_clause_array (sat->cnf, temp_clause);

    /* finalization */
    sat->special_coding_iterator_1ofn++;
    g_string_free (temp_str, true);
    g_slice_free1 ((sizeof (long int) * 2*n_lit), main_array);
    sat_problem_clear_solution (sat);
}

/* apply m of n direct encoding to literals (const char *) in lit_list. */
void sat_problem_add_mofn_direct_encoding (struct sat_problem *sat, GSList *lit_list, unsigned int m)
{
    /* checks */
    if (sat == NULL) return;

    size_t n_lit = g_slist_length (lit_list);

    if (m > n_lit) return;

    if (n_lit <= 1) return;
    if (n_lit == m) {
        GSList *temp_clause = g_slist_prepend (NULL, NULL);

        for (GSList *li = lit_list; li != NULL; li = li->next) {
            temp_clause->data = li->data;
            sat_problem_add_clause_gslist (sat, temp_clause);
        }

        g_slist_free (temp_clause);

        return;
    }

    /* main variables */
    long int *main_array   = g_slice_alloc (sizeof (long int) * (n_lit + 1));
    long int *temp_clause  = g_slice_alloc (sizeof (long int) * (n_lit + 1));
    long int *index_clause = g_slice_alloc (sizeof (long int) * (n_lit + 1));

    int i = 0;
    for (GSList *li = lit_list; li != NULL; li = li->next) {
        long int literal = sat_problem_encode_literal (sat, li->data);
        main_array[i]   = literal;
        index_clause[i] = i;
        temp_clause[i]  = literal;
        i++;
    }
    main_array[n_lit] = 0;

    /* clauses */
    /* min m */
    i = n_lit - m;
    temp_clause[n_lit-m+1] = 0;

    while (true) {
        if (i == n_lit - m) {
            /* last element */
            if (index_clause[i] < n_lit) {
                /* valid clause */
                base_cnf_add_clause_array (sat->cnf, temp_clause);
            } else {
                /* invalid clause */
                i--;
            }
        } else {
            /* intermediate element */
            if (index_clause[i] < i + m) {
                /* valid value */
                index_clause[i+1] = index_clause[i];
                i++;
            } else {
                /* invalid value */
                i--;
            }
        }

        if (i < 0) break;

        /* next index */
        long int index = index_clause[i];
        index++;
        index_clause[i] = index;
        temp_clause[i] = main_array[index];
    }


    /* exclusion */
    temp_clause[m+1] = 0;

    i = 0;
    index_clause[0] = 0;
    temp_clause[0]  = -main_array[0];

    while (true) {
        if (i == m) {
            /* last element */
            if (index_clause[i] < n_lit) {
                /* valid clause */
                base_cnf_add_clause_array (sat->cnf, temp_clause);
            } else {
                /* invalid clause */
                i--;
            }
        } else {
            /* intermediate element */
            if (index_clause[i] < i + n_lit - m) {
                /* valid value */
                index_clause[i+1] = index_clause[i];
                i++;
            } else {
                /* invalid value */
                i--;
            }
        }

        if (i < 0) break;

        /* next index */
        long int index = index_clause[i];
        index++;
        index_clause[i] = index;
        temp_clause[i] = -main_array[index];
    }

    g_slice_free1 ((sizeof (long int) * (n_lit + 1)), main_array);
    g_slice_free1 ((sizeof (long int) * (n_lit + 1)), temp_clause);
    g_slice_free1 ((sizeof (long int) * (n_lit + 1)), index_clause);
    sat_problem_clear_solution (sat);
}

/* add a formula as mapping and return true on success. 
 * formula: the formula represented as string with variables 1 ... n.
 * lit_mapping: list of literals (const char *) to map to variables in given formula.
 * parsed formulas are cached for making multiple usage more efficient as only mapping 
 * needs to be applied individually whereas parsing only needs to be done once. */
bool sat_problem_add_formula_mapping (struct sat_problem *sat, const char* formula, GSList *lit_mapping)
{
    if (sat == NULL) return false;
    if (formula == NULL) return false;
    if (lit_mapping == NULL) return false;

    GList *clause_list = NULL;

    /* lookup / generate + cache */
    if (!g_hash_table_lookup_extended (sat->formula_to_cnf_cache, formula, NULL, (void **) &clause_list)) {
        SatFormula parsed_formula = NULL;

        parsed_formula = sat_formula_parse (formula);
        if (parsed_formula == NULL) {
            printf ("ERROR: could not parse formula \"%s\"\n", formula);
            return false;
        }

        clause_list = sat_formula_to_cnf (parsed_formula);
        sat_formula_free (&parsed_formula);

        if (clause_list == NULL) {
            printf ("ERROR: invalid formula \"%s\"\n", formula);
        }

        /* cache */
        char *key = g_string_chunk_insert_const (sat->str_formulas, formula);
        g_hash_table_insert (sat->formula_to_cnf_cache, key, clause_list);
    }

    /* debug print */
    /*
    for (GList *li = clause_list; li != NULL; li = li->next) {
        printf ("DEBUG: [");
        GQueue *clause = li->data;
        for (GList *lj = clause->head; lj != NULL; lj = lj->next) {
            long int lit = GPOINTER_TO_SIZE (lj->data);
            if (lj->next == NULL) {
                printf ("%ld", lit);
            } else {
                printf ("%ld, ", lit);
            }
        }
        printf ("]\n");
    }
    */

    /* map */
    bool result = true;
    size_t n_lits = g_slist_length (lit_mapping);
    GArray *encoded_mapping = g_array_sized_new (true, true, sizeof (long int), n_lits + 1);
    long int zero_mapped = 0;
    g_array_append_val (encoded_mapping, zero_mapped);

    for (GSList *li = lit_mapping; li != NULL; li = li->next) {
        const char *lit_str = li->data;
        long int lit_enc = sat_problem_encode_literal (sat, lit_str);
        g_array_append_val (encoded_mapping, lit_enc);
        /* printf ("DEBUG: new encoded lit: %s -> %ld\n", lit_str, lit_enc); */
    }

    GSList *clause_list_mapped = NULL;
    for (GList *li = clause_list; li != NULL; li = li->next) {
        GQueue *clause_raw    = li->data;
        GSList *clause_mapped = NULL;

        for (GList *lj = clause_raw->head; lj != NULL; lj = lj->next) {
            long int lit_raw = GPOINTER_TO_SIZE (lj->data);
            bool lit_invert = false;
            if (lit_raw < 0) {
                lit_raw    = -lit_raw;
                lit_invert = true;
            }

            if (lit_raw > n_lits) {
                printf ("ERROR: no mapping specified for raw variable %ld\n", lit_raw);
                g_slist_free (clause_mapped);
                result = false;
                goto sat_problem_add_formula_mapping_finalize;
            }

            long int lit_mapped = g_array_index (encoded_mapping, long int, lit_raw);
            if (lit_invert) lit_mapped = -lit_mapped;

            clause_mapped = g_slist_prepend (clause_mapped, GSIZE_TO_POINTER (lit_mapped));
        }

        clause_mapped = g_slist_reverse (clause_mapped);
        clause_list_mapped = g_slist_prepend (clause_list_mapped, clause_mapped);
    }

    clause_list_mapped = g_slist_reverse (clause_list_mapped);

    /* debug print */
    /*
    for (GSList *li = clause_list_mapped; li != NULL; li = li->next) {
        printf ("DEBUG: [");
        GSList *clause = li->data;
        for (GSList *lj = clause; lj != NULL; lj = lj->next) {
            long int lit = GPOINTER_TO_SIZE (lj->data);
            if (lj->next == NULL) {
                printf ("%ld", lit);
            } else {
                printf ("%ld, ", lit);
            }
        }
        printf ("]\n");
    }
    */

    /* insert */
    for (GSList *li = clause_list_mapped; li != NULL; li = li->next) {
        GSList *clause = li->data;
        base_cnf_add_clause_gslist (sat->cnf, clause);
    }

    /* cleanup */
sat_problem_add_formula_mapping_finalize:
    g_slist_free_full (clause_list_mapped, (GDestroyNotify) g_slist_free);

    g_array_free (encoded_mapping, true);
    return result;
}

/* lookup variable integer mapping and return mapped string */
const char * sat_problem_get_varname_from_number (struct sat_problem *sat, long int number)
{
    if (sat == NULL) return NULL;
    if (number <= 0) return NULL;

    gpointer lookup_val = g_hash_table_lookup (sat->tbl_lit_int_to_name, GSIZE_TO_POINTER (number));

    return (const char *) lookup_val;
}

/* lookup variable string and return mapped integer */
long int sat_problem_get_varnumber_from_name (struct sat_problem *sat, const char *name)
{
    if (sat == NULL) return 0;
    if (name == NULL) return 0;

    bool invert = false;
    while (name[0] == '-') {
        invert = !invert;
        name++;
        if (strlen (name) == 0) return 0;
    }

    gpointer lookup_val = g_hash_table_lookup (sat->tbl_lit_name_to_int, name);
    if (lookup_val == NULL) return 0;
    long int result = GPOINTER_TO_SIZE (lookup_val);
    if (invert) return -result;
    return result;
}

/* get the internal data structure of all currently mapped clauses
 * as a GQueue of 0-terminated literal (long int) arrays.
 * the result should not be modified. */
GQueue *sat_problem_get_clauses_mapped (struct sat_problem *sat)
{
    if (sat == NULL) return NULL;
    return base_cnf_clauses (sat->cnf);
}

/* solve current sat problem and return false on errors, true otherwise.
 * tmp_file_name: prefix for temporary files for solver,
 * solver_bin: binary to execute,
 * solution_on_stdout: if true, assume that solver prints result on stdout,
 *   otherwise to filename given as second argument,
 * cleanup: if true, temporary files will be removed afterwards,
 * cnf_gz: CNF file will be in gzipped dimacs, otherwise in plain dimacs. */
bool sat_problem_solve (struct sat_problem *sat, const char *tmp_file_name, const char *solver_bin, bool solution_on_stdout, bool cleanup, bool cnf_gz)
{
    if (sat == NULL) return false;
    if (tmp_file_name == NULL) return false;

    bool run_success = base_cnf_solve (sat->cnf, tmp_file_name, solver_bin, solution_on_stdout, cleanup, cnf_gz);

    if (run_success == false) return false;

    sat->solver_run = true;

    GSList *solution = base_cnf_solution (sat->cnf);

    if (solution == NULL) {
        sat->satisfiable = false;
        return true;
    }
    sat->satisfiable = true;

    for (GSList *li = solution; li != NULL; li = li->next) {
        long int i_result = GPOINTER_TO_SIZE (li->data);

        bool var_result = (i_result > 0 ? true     : false);
        long int var    = (i_result > 0 ? i_result : -i_result);
        char *var_name  = g_hash_table_lookup (sat->tbl_lit_int_to_name, GSIZE_TO_POINTER (var));

        if (var_name == NULL) continue;

        g_hash_table_insert (sat->tbl_var_result, var_name, GSIZE_TO_POINTER (var_result));
    }

    return true;
}

/* invalidate current solution to obtain a different one on next solve */
void sat_problem_cancel_solution (struct sat_problem *sat)
{
    base_cnf_cancel_solution (sat->cnf);
    sat_problem_clear_solution (sat);
}

/* return true, if problem is satisfiable. *error is set to true in
 * case of errors: e.g. solver is not yet run. */
bool sat_problem_satisfiable (struct sat_problem *sat, bool *error)
{
    if (sat == NULL) {
        if (error != NULL) *error = true;
        return false;
    }
    if (!sat->solver_run) {
        if (error != NULL) *error = true;
        printf ("ERROR: problem not yet solved\n");
        return false;
    }

    if (error != NULL) *error = false;
    return sat->satisfiable;
}

/* obtain assigned boolean value to variable var.
 * In case of errors (e.g. unknown variable, not satisfiable, ...)
 * *error is set to true. */
bool sat_problem_var_result (struct sat_problem *sat, const char *var, bool *error)
{
    if (sat == NULL) {
        if (error != NULL) *error = true;
        return false;
    }
    if (!sat->solver_run) {
        if (error != NULL) *error = true;
        printf ("ERROR: problem not yet solved\n");
        return false;
    }
    if (!sat->satisfiable) {
        if (error != NULL) *error = true;
        printf ("ERROR: problem not satisfiable\n");
        return false;
    }

    if (!g_hash_table_contains (sat->tbl_var_result, var)) {
        if (error != NULL) *error = true;
        printf ("ERROR: variable not found: %s\n", var);
        return false;
    }

    if (error != NULL) *error = false;
    gpointer result = g_hash_table_lookup (sat->tbl_var_result, var);
    return (bool) GPOINTER_TO_SIZE (result);
}

/* iterator function for generating result list for sat_problem_var_result_list 
 * from GHashTable */
static void sat_problem_var_result_list_gen_func (gpointer key, gpointer value, gpointer user_data)
{
    char *var_name      = key;
    bool var_assignment = GPOINTER_TO_SIZE (value);

    struct sat_problem_var_result_list_gen_user_data *iter_data = user_data;

    if (var_assignment != iter_data->assignment) return;

    iter_data->result = g_slist_prepend (iter_data->result, var_name);
}

/* obtain a GList of all variables (const char *) assigned to var_assignment.
 * In case of errors (e.g. not satisfiable, ...)
 * *error is set to true. */
GSList *sat_problem_var_result_list (struct sat_problem *sat, bool var_assignment, bool *error)
{
    if (sat == NULL) {
        if (error != NULL) *error = true;
        return NULL;
    }
    if (!sat->solver_run) {
        if (error != NULL) *error = true;
        printf ("ERROR: problem not yet solved\n");
        return NULL;
    }
    if (!sat->satisfiable) {
        if (error != NULL) *error = true;
        printf ("ERROR: problem not satisfiable\n");
        return NULL;
    }

    struct sat_problem_var_result_list_gen_user_data iter_data = {var_assignment, NULL};

    g_hash_table_foreach (sat->tbl_var_result, sat_problem_var_result_list_gen_func, &iter_data);

    return iter_data.result;
}
