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

#include "sat_formula.h"
#include "sat_formula_parser.h"
#include "sat_formula_lexer.h"

#include <stdio.h>
#include <stdbool.h>

/* sat formula syntax tree element */
struct sat_formula {
    /* tag defining operation/data */
    enum sat_formula_tag_t tag;
    /* for literal elements: literal as long int */
    long int literal;
    /* operand for inversion, left operand for two-operand operations */
    struct sat_formula *left_operand;
    /* right operand for two-operand operations */
    struct sat_formula *right_operand;
};

/* find and return the first sat_formula element in a clause represented
 * as a GSlist of sat_formula elements that is not a literal */
static struct sat_formula * sat_formula_clause_find_non_cnf (GSList *formula_clause);

/* transform a clause represented by a GSList of sat_formula tree elements
 * that contains only literals into a sorted clause represented by a GQueue
 * of literals represented by long int */
static GQueue* sat_formula_clause_to_sorted_clause (GSList *formula_clause);

/* process a sat_formula tree element in order to transform it into CNF.
 * non_cnf_formula is the tree element that is not a literal and is contained in
 * formula clause (GSList of formula tree elements).
 * formula clause is the clause to transform. newly generated clauses in transformation
 * will be added to formula_clause_queue. */
static void sat_formula_clause_process (struct sat_formula *non_cnf_formula, GSList *formula_clause, GQueue *formula_clause_queue);

/* reduce a formula tree beginning with an inversion */
static void sat_formula_clause_transform_inversion (struct sat_formula *non_cnf_formula);

/* compare two literals (long int casted to gpointer):
 * first by absolute value, secondly by sign */
static gint sat_formula_literal_compare_func (gconstpointer a, gconstpointer b, gpointer userdata);
/* compare two clauses as GQueue by number of elements */
static gint sat_formula_clause_compare_func (gconstpointer a, gconstpointer b);

/* check if a clause represented as sorted GQueue of literals is always true */
static bool sat_formula_cnf_sorted_clause_is_true (GQueue *sorted_clause);

/* check if a clause sorted_clause_subsumming subsums a clause sorted_clause_subsummed.
 * both clauses are represented as sorted GQueues of literals (long int). */
static bool sat_formula_cnf_sorted_clause_is_subsummed (GQueue *sorted_clause_subsummed, GQueue *sorted_clause_subsumming);

/* insert a sorted clause sorted_clause represented as a sorted GQueue
 * of literals (long int)
 * into a sorted list of clauses sorted_clause_list represented by a sorted GList of
 * sorted GQueues, remove subsummed clauses, ignore tautologies.
 * return the updated clause list. */
static GList * sat_formula_cnf_insert_reduce (GList *sorted_clause_list, GQueue *sorted_clause);

/* allocate and return a new sat_formula element representing the given literal */
struct sat_formula * sat_formula_new_literal (long int literal)
{
    struct sat_formula *result = g_slice_new (struct sat_formula);
    if (result == NULL) return NULL;

    /* DEBUG
    printf ("new literal formula: %ld\n", literal);
    */

    result->tag     = SAT_FORMULA_TAG_LITERAL;
    result->literal = literal;
    result->left_operand  = NULL;
    result->right_operand = NULL;

    return result;
}

/* allocate and return a new sat_formula element representing the inversion of given argument */
struct sat_formula * sat_formula_new_inversion (struct sat_formula *arg)
{
    struct sat_formula *result = g_slice_new (struct sat_formula);
    if (result == NULL) return NULL;

    result->tag     = SAT_FORMULA_TAG_INVERSION;
    result->literal = 0;
    result->left_operand  = arg;
    result->right_operand = NULL;

    /* DEBUG
    printf ("new formula: ");
    sat_formula_print (result);
    */

    return result;
}

/* allocate and return a new sat_formula element representing the given operation with given operands */
struct sat_formula * sat_formula_new_operation (enum sat_formula_tag_t tag, struct sat_formula *left, struct sat_formula *right)
{
    struct sat_formula *result = g_slice_new (struct sat_formula);
    if (result == NULL) return NULL;

    result->tag     = tag;
    result->literal = 0;
    result->left_operand  = left;
    result->right_operand = right;

    /* DEBUG
    printf ("new formula: ");
    sat_formula_print (result);
    */

    return result;
}

/* recursively free the given sat_formula tree */
void sat_formula_free (struct sat_formula **fpointer)
{
    if (fpointer == NULL) return;
    struct sat_formula *formula = *fpointer;

    /* DEBUG
    printf ("freeing formula: ");
    sat_formula_print (formula);
    */

    if (formula == NULL) return;

    sat_formula_free (&formula->left_operand);
    sat_formula_free (&formula->right_operand);
    g_slice_free (struct sat_formula, formula);
    *fpointer = NULL;
}

/* recursively print the given sat_formula tree without final newline */
static void sat_formula_print_nonewline (struct sat_formula *formula, int paran_level)
{
    if (formula == NULL) {
        fprintf (stderr, "ERROR - null pointer!\n");
        return;
    }

    const char *paran_open  = "([{<";
    const char *paran_close = ")]}>";

    int paran_level_sub = (paran_level + 1) % 4;

    switch (formula->tag) {
        case SAT_FORMULA_TAG_LITERAL:
            printf ("%ld", formula->literal);
            break;
        case SAT_FORMULA_TAG_INVERSION:
            printf ("-");
            sat_formula_print_nonewline (formula->left_operand, paran_level);
            break;
        case SAT_FORMULA_TAG_OP_OR:
            printf ("%c", paran_open[paran_level]);
            sat_formula_print_nonewline (formula->left_operand, paran_level_sub);
            printf (" or ");
            sat_formula_print_nonewline (formula->right_operand, paran_level_sub);
            printf ("%c", paran_close[paran_level]);
            break;
        case SAT_FORMULA_TAG_OP_AND:
            printf ("%c", paran_open[paran_level]);
            sat_formula_print_nonewline (formula->left_operand, paran_level_sub);
            printf (" and ");
            sat_formula_print_nonewline (formula->right_operand, paran_level_sub);
            printf ("%c", paran_close[paran_level]);
            break;
        case SAT_FORMULA_TAG_OP_XOR:
            printf ("%c", paran_open[paran_level]);
            sat_formula_print_nonewline (formula->left_operand, paran_level_sub);
            printf (" xor ");
            sat_formula_print_nonewline (formula->right_operand, paran_level_sub);
            printf ("%c", paran_close[paran_level]);
            break;
        case SAT_FORMULA_TAG_OP_EQUAL:
            printf ("%c", paran_open[paran_level]);
            sat_formula_print_nonewline (formula->left_operand, paran_level_sub);
            printf (" <=> ");
            sat_formula_print_nonewline (formula->right_operand, paran_level_sub);
            printf ("%c", paran_close[paran_level]);
            break;
        case SAT_FORMULA_TAG_OP_RIMPL:
            printf ("%c", paran_open[paran_level]);
            sat_formula_print_nonewline (formula->left_operand, paran_level_sub);
            printf (" => ");
            sat_formula_print_nonewline (formula->right_operand, paran_level_sub);
            printf ("%c", paran_close[paran_level]);
            break;
        case SAT_FORMULA_TAG_OP_LIMPL:
            printf ("%c", paran_open[paran_level]);
            sat_formula_print_nonewline (formula->left_operand, paran_level_sub);
            printf (" <= ");
            sat_formula_print_nonewline (formula->right_operand, paran_level_sub);
            printf ("%c", paran_close[paran_level]);
            break;
        default:
            fprintf (stderr, "ERROR - unknown tag!\n");
            break;
    }
}

/* recursively print the given sat_formula */
void sat_formula_print (struct sat_formula *formula)
{
    sat_formula_print_nonewline (formula, 0);
    printf ("\n");
}

/* recursively duplicate the given sat_formula tree */
struct sat_formula * sat_formula_duplicate (struct sat_formula *formula)
{
    if (formula == NULL) return NULL;

    struct sat_formula *result = g_slice_new (struct sat_formula);
    if (result == NULL) return NULL;

    result->tag           = formula->tag;
    result->literal       = formula->literal;
    result->left_operand  = sat_formula_duplicate (formula->left_operand);
    result->right_operand = sat_formula_duplicate (formula->right_operand);

    return result;
}

/* parse and return the given formula into an allocated sat_formula tree */
struct sat_formula *sat_formula_parse (const char *expr)
{
    struct sat_formula *formula = NULL;
    yyscan_t scanner;
    YY_BUFFER_STATE state;

    if (sat_formula_yylex_init (&scanner)) {
        /* couldn't initialize */
        return NULL;
    }

    state = sat_formula_yy_scan_string (expr, scanner);

    if (sat_formula_yyparse (&formula, scanner)) {
        /* error parsing */
        sat_formula_free (&formula);

        sat_formula_yy_delete_buffer (state, scanner);

        sat_formula_yylex_destroy (scanner);

        return NULL;
    }

    sat_formula_yy_delete_buffer (state, scanner);

    sat_formula_yylex_destroy (scanner);

    return formula;
}

/* transforms and returns a given sat_formula tree into a CNF formula represented
 * as a GList of clauses, where each clause is a GQueue of literals as long int */
GList * sat_formula_to_cnf (struct sat_formula *formula)
{
    if (formula == NULL) return NULL;
    formula = sat_formula_duplicate (formula);

    GQueue *formula_clause_queue = g_queue_new ();
    GSList *formula_clause       = NULL;
    GList  *result_list          = NULL;
    formula_clause = g_slist_prepend (formula_clause, formula);

    g_queue_push_head (formula_clause_queue, formula_clause);

    while (! g_queue_is_empty (formula_clause_queue)) {
        GSList *formula_clause = g_queue_pop_head (formula_clause_queue);
        if (formula_clause == NULL) continue;

        struct sat_formula *non_cnf = sat_formula_clause_find_non_cnf (formula_clause);
        if (non_cnf == NULL) {
            GQueue *sorted_clause = sat_formula_clause_to_sorted_clause (formula_clause);

            for (GSList *li = formula_clause; li != NULL; li = li->next) {
                sat_formula_free ((struct sat_formula **) &li->data);
            }
            g_slist_free (formula_clause);

            result_list = sat_formula_cnf_insert_reduce (result_list, sorted_clause);
        } else {
            sat_formula_clause_process (non_cnf, formula_clause, formula_clause_queue);
        }
    }

    g_queue_free (formula_clause_queue);

    return result_list;
}

/* find and return the first sat_formula element in a clause represented
 * as a GSlist of sat_formula elements that is not a literal */
static struct sat_formula * sat_formula_clause_find_non_cnf (GSList *formula_clause)
{
    if (formula_clause == NULL) return NULL;

    for (GSList *li = formula_clause; li != NULL; li = li->next) {
        struct sat_formula *f = li->data;
        if (f->tag != SAT_FORMULA_TAG_LITERAL) return f;
    }

    return NULL;
}

/* compare two literals (long int casted to gpointer):
 * first by absolute value, secondly by sign */
static gint sat_formula_literal_compare_func (gconstpointer a, gconstpointer b, gpointer userdata)
{
    long int ia = GPOINTER_TO_SIZE (a);
    long int ib = GPOINTER_TO_SIZE (b);

    if (ia + ib == 0) return ib-ia;
    if (ia < 0) ia = -ia;
    if (ib < 0) ib = -ib;

    return ia - ib;
}

/* compare two clauses as GQueue by number of elements */
static gint sat_formula_clause_compare_func (gconstpointer a, gconstpointer b)
{
    GQueue *clause_a = (GQueue *) a;
    GQueue *clause_b = (GQueue *) b;

    guint len_a = g_queue_get_length (clause_a);
    guint len_b = g_queue_get_length (clause_b);

    return len_a - len_b;
}

/* transform a clause represented by a GSList of sat_formula tree elements
 * that contains only literals into a sorted clause represented by a GQueue
 * of literals represented by long int */
static GQueue * sat_formula_clause_to_sorted_clause (GSList *formula_clause)
{
    GQueue *result = g_queue_new ();

    for (GSList *li = formula_clause; li != NULL; li = li->next) {
        struct sat_formula *f = li->data;
        g_queue_push_tail (result, GSIZE_TO_POINTER (f->literal));
    }
    g_queue_sort (result, sat_formula_literal_compare_func, NULL);

    /* remove duplicates */
    long int last_lit = 0;
    GList *li = result->head;
    while (li != NULL) {
        long int lit = GPOINTER_TO_SIZE (li->data);

        GList *li_next = li->next;
        if (lit == last_lit) {
            g_queue_delete_link (result, li);
        } else {
            last_lit = lit;
        }
        li = li_next;
    }

    return result;
}

/* process a sat_formula tree element in order to transform it into CNF.
 * non_cnf_formula is the tree element that is not a literal and is contained in
 * formula clause (GSList of formula tree elements).
 * formula clause is the clause to transform. newly generated clauses in transformation
 * will be added to formula_clause_queue. */
static void sat_formula_clause_process (struct sat_formula *non_cnf_formula, GSList *formula_clause, GQueue *formula_clause_queue)
{
    if (non_cnf_formula == NULL) return;
    if (formula_clause == NULL) return;
    if (formula_clause_queue == NULL) return;

    /* literal: nothing to do */
    if (non_cnf_formula->tag == SAT_FORMULA_TAG_LITERAL) return;

    /* inversion: invert formula-tree element and add clause to queue */
    if (non_cnf_formula->tag == SAT_FORMULA_TAG_INVERSION) {
        sat_formula_clause_transform_inversion (non_cnf_formula);
        g_queue_push_head (formula_clause_queue, formula_clause);
        return;
    }

    /* implication right: a -> b becomes -a, b
     * tree element becomes inversion of left operand,
     * right operand is added to clause as new element,
     * clause is resubmitted to queue */
    if (non_cnf_formula->tag == SAT_FORMULA_TAG_OP_RIMPL) {
        struct sat_formula *noninv     = non_cnf_formula->right_operand;
        non_cnf_formula->right_operand = NULL;
        non_cnf_formula->tag           = SAT_FORMULA_TAG_INVERSION;
        formula_clause = g_slist_prepend (formula_clause, noninv);

        g_queue_push_head (formula_clause_queue, formula_clause);
        return;
    }

    /* implication left: a <- b becomes a, -b
     * tree element becomes inversion of right operand,
     * left operand is added to clause as new element,
     * clause is resubmitted to queue */
    if (non_cnf_formula->tag == SAT_FORMULA_TAG_OP_LIMPL) {
        struct sat_formula *noninv     = non_cnf_formula->left_operand;
        non_cnf_formula->left_operand  = non_cnf_formula->right_operand;
        non_cnf_formula->right_operand = NULL;
        non_cnf_formula->tag           = SAT_FORMULA_TAG_INVERSION;
        formula_clause = g_slist_prepend (formula_clause, noninv);

        g_queue_push_head (formula_clause_queue, formula_clause);
        return;
    }

    /* or: a or b becomes a, b
     * right operand is added as new element to clause, tree element
     * becomes representation of its left operand, left operand is freed */
    if (non_cnf_formula->tag == SAT_FORMULA_TAG_OP_OR) {
        struct sat_formula *sub_a = non_cnf_formula->left_operand;

        formula_clause = g_slist_prepend (formula_clause, non_cnf_formula->right_operand);

        non_cnf_formula->left_operand  = sub_a->left_operand;
        non_cnf_formula->right_operand = sub_a->right_operand;
        non_cnf_formula->tag           = sub_a->tag;
        non_cnf_formula->literal       = sub_a->literal;

        sub_a->left_operand  = NULL;
        sub_a->right_operand = NULL;
        sat_formula_free (&sub_a);

        g_queue_push_head (formula_clause_queue, formula_clause);
        return;
    }

    /* and: a and b becomes one clause containing a, one containing b
     * clause is copied: formula tree element is changed to its left operand in
     * original clause while its right operand is added to newly created clause.
     * Both clauses are added for further processing */
    if (non_cnf_formula->tag == SAT_FORMULA_TAG_OP_AND) {
        GSList *formula_clause2 = NULL;

        for (GSList *li = formula_clause; li != NULL; li = li->next) {
            struct sat_formula *f = li->data;
            if (f == non_cnf_formula) {
                li->data        = f->left_operand;
                formula_clause2 = g_slist_prepend (formula_clause2, f->right_operand);
            } else {
                formula_clause2 = g_slist_prepend (formula_clause2, sat_formula_duplicate (f));
            }
        }

        non_cnf_formula->left_operand  = NULL;
        non_cnf_formula->right_operand = NULL;
        sat_formula_free (&non_cnf_formula);

        g_queue_push_head (formula_clause_queue, formula_clause);
        g_queue_push_head (formula_clause_queue, formula_clause2);
        return;
    }

    /* xor: a xor b becomes one clause containing a, b and one containing -a, -b */
    if (non_cnf_formula->tag == SAT_FORMULA_TAG_OP_XOR) {
        GSList *formula_clause2 = NULL;

        for (GSList *li = formula_clause; li != NULL; li = li->next) {
            struct sat_formula *f = li->data;
            if (f == non_cnf_formula) {
                li->data        = f->left_operand;
                struct sat_formula * b1 = sat_formula_duplicate (f->left_operand);
                struct sat_formula * b2 = sat_formula_duplicate (f->right_operand);
                b1 = sat_formula_new_operation (SAT_FORMULA_TAG_INVERSION, b1, NULL);
                b2 = sat_formula_new_operation (SAT_FORMULA_TAG_INVERSION, b2, NULL);
                formula_clause2 = g_slist_prepend (formula_clause2, b1);
                formula_clause2 = g_slist_prepend (formula_clause2, b2);
            } else {
                formula_clause2 = g_slist_prepend (formula_clause2, sat_formula_duplicate (f));
            }
        }
        formula_clause = g_slist_prepend (formula_clause, non_cnf_formula->right_operand);

        non_cnf_formula->left_operand  = NULL;
        non_cnf_formula->right_operand = NULL;
        sat_formula_free (&non_cnf_formula);

        g_queue_push_head (formula_clause_queue, formula_clause);
        g_queue_push_head (formula_clause_queue, formula_clause2);
        return;
    }

    /* equality: a == b becomes two clauses, one containing -a, b and one containing a, -b */
    if (non_cnf_formula->tag == SAT_FORMULA_TAG_OP_EQUAL) {
        GSList *formula_clause2 = NULL;

        for (GSList *li = formula_clause; li != NULL; li = li->next) {
            struct sat_formula *f = li->data;
            if (f == non_cnf_formula) {
                li->data        = f->left_operand;
                struct sat_formula * b1 = sat_formula_duplicate (f->left_operand);
                struct sat_formula * b2 = sat_formula_duplicate (f->right_operand);
                b1 = sat_formula_new_operation (SAT_FORMULA_TAG_INVERSION, b1, NULL);
                formula_clause2 = g_slist_prepend (formula_clause2, b1);
                formula_clause2 = g_slist_prepend (formula_clause2, b2);
            } else {
                formula_clause2 = g_slist_prepend (formula_clause2, sat_formula_duplicate (f));
            }
        }

        non_cnf_formula->left_operand  = non_cnf_formula->right_operand;
        non_cnf_formula->right_operand = NULL;
        non_cnf_formula->tag           = SAT_FORMULA_TAG_INVERSION;
        formula_clause = g_slist_prepend (formula_clause, non_cnf_formula);

        g_queue_push_head (formula_clause_queue, formula_clause);
        g_queue_push_head (formula_clause_queue, formula_clause2);
        return;
    }
}

/* reduce a formula tree beginning with an inversion */
static void sat_formula_clause_transform_inversion (struct sat_formula *non_cnf_formula)
{
        struct sat_formula *sub_f = non_cnf_formula->left_operand;

        /* literal: -(a) becomes -a */
        if (sub_f->tag == SAT_FORMULA_TAG_LITERAL) {
            non_cnf_formula->literal = -sub_f->literal;
            non_cnf_formula->tag     = SAT_FORMULA_TAG_LITERAL;
            sat_formula_free (&non_cnf_formula->left_operand);
            return;
        }

        /* inversion: -(-(f)) becomes f */
        if (sub_f->tag == SAT_FORMULA_TAG_INVERSION) {
            struct sat_formula *ssub_f     = sub_f->left_operand;
            non_cnf_formula->literal       = ssub_f->literal;
            non_cnf_formula->tag           = ssub_f->tag;
            non_cnf_formula->left_operand  = ssub_f->left_operand;
            non_cnf_formula->right_operand = ssub_f->right_operand;
            ssub_f->left_operand  = NULL;
            ssub_f->right_operand = NULL;
            sat_formula_free (&sub_f);
            return;
        }

        /* xor, equal:
         * -(f1 xor f2) becomes f1 ==  f2
         * -(f1 ==  f2) becomes f1 xor f2 */
        if ((sub_f->tag == SAT_FORMULA_TAG_OP_XOR) || (sub_f->tag == SAT_FORMULA_TAG_OP_EQUAL)) {
            non_cnf_formula->literal       = sub_f->literal;
            if (sub_f->tag == SAT_FORMULA_TAG_OP_XOR) {
                non_cnf_formula->tag       = SAT_FORMULA_TAG_OP_EQUAL;
            } else {
                non_cnf_formula->tag       = SAT_FORMULA_TAG_OP_XOR;
            }
            non_cnf_formula->left_operand  = sub_f->left_operand;
            non_cnf_formula->right_operand = sub_f->right_operand;
            sub_f->left_operand  = NULL;
            sub_f->right_operand = NULL;
            sat_formula_free (&sub_f);
            return;
        }

        /* and, or:
         * -(f1 and f2) becomes (-f1) or  (-f2)
         * -(f1 or  f2) becomes (-f1) and (-f2) */
        if ((sub_f->tag == SAT_FORMULA_TAG_OP_AND) || (sub_f->tag == SAT_FORMULA_TAG_OP_OR)) {
            if (sub_f->tag == SAT_FORMULA_TAG_OP_AND) {
                non_cnf_formula->tag       = SAT_FORMULA_TAG_OP_OR;
            } else {
                non_cnf_formula->tag       = SAT_FORMULA_TAG_OP_AND;
            }
            non_cnf_formula->right_operand = sat_formula_new_operation (SAT_FORMULA_TAG_INVERSION, sub_f->right_operand, NULL);

            sub_f->tag           = SAT_FORMULA_TAG_INVERSION;
            sub_f->right_operand = NULL;
            return;
        }

        /* implications:
         * -(f1 => f2) becomes -((-f1) or   f2)  becomes   f1  and (-f2)
         * -(f1 <= f2) becomes -(  f1  or (-f2)) becomes (-f1) and   f2  */
        if ((sub_f->tag == SAT_FORMULA_TAG_OP_RIMPL) || (sub_f->tag == SAT_FORMULA_TAG_OP_LIMPL)) {
            non_cnf_formula->tag       = SAT_FORMULA_TAG_OP_AND;
            if (sub_f->tag == SAT_FORMULA_TAG_OP_RIMPL) {
                non_cnf_formula->right_operand = sub_f->left_operand;
                sub_f->left_operand  = sub_f->right_operand;
                sub_f->right_operand = NULL;
            } else {
                non_cnf_formula->right_operand = sub_f->right_operand;
                sub_f->right_operand = NULL;
            }
            sub_f->tag = SAT_FORMULA_TAG_INVERSION;
            return;
        }
}

/* insert a sorted clause sorted_clause represented as a sorted GQueue
 * of literals (long int)
 * into a sorted list of clauses sorted_clause_list represented by a sorted GList of
 * sorted GQueues, remove subsummed clauses, ignore tautologies.
 * return the updated clause list. */
static GList * sat_formula_cnf_insert_reduce (GList *sorted_clause_list, GQueue *sorted_clause)
{
    if (sorted_clause == NULL) return sorted_clause_list;

    if (sat_formula_cnf_sorted_clause_is_true (sorted_clause)) {
        g_queue_free (sorted_clause);
        return sorted_clause_list;
    }

    GList *li = sorted_clause_list;
    guint len_insert = g_queue_get_length (sorted_clause);

    /* clause to insert subsummed? */
    while (li != NULL) {
        GQueue *current_clause = li->data;
        if (g_queue_get_length (current_clause) > len_insert) break;

        if (sat_formula_cnf_sorted_clause_is_subsummed (sorted_clause, current_clause)) {
            g_queue_free (sorted_clause);
            return sorted_clause_list;
        }

        li = li->next;
    }

    /* existing clause is subsummed? */
    while (li != NULL) {
        GQueue *current_clause = li->data;
        GList *li_next = li->next;

        if (sat_formula_cnf_sorted_clause_is_subsummed (current_clause, sorted_clause)) {
            g_queue_free (current_clause);
            sorted_clause_list = g_list_delete_link (sorted_clause_list, li);
        }

        li = li_next;
    }

    return g_list_insert_sorted (sorted_clause_list, sorted_clause, sat_formula_clause_compare_func);
}

/* check if a clause represented as sorted GQueue of literals is always true */
static bool sat_formula_cnf_sorted_clause_is_true (GQueue *sorted_clause)
{
    for (GList *li = sorted_clause->head; li != NULL; li = li->next) {
        if (li->next != NULL) {
            long int this_literal = GPOINTER_TO_SIZE (li->data);
            long int next_literal = GPOINTER_TO_SIZE (li->next->data);

            if (this_literal + next_literal == 0) return true;
        }
    }

    return false;
}

/* check if a clause sorted_clause_subsumming subsums a clause sorted_clause_subsummed.
 * both clauses are represented as sorted GQueues of literals (long int). */
static bool sat_formula_cnf_sorted_clause_is_subsummed (GQueue *sorted_clause_subsummed, GQueue *sorted_clause_subsumming)
{
    if (sorted_clause_subsumming == NULL) return true;
    if (sorted_clause_subsummed == NULL) return false;

    GList *li_ssed = sorted_clause_subsummed->head;
    for (GList *li_ssing = sorted_clause_subsumming->head; li_ssing != NULL; li_ssing = li_ssing->next) {
        long int lit_ssing = GPOINTER_TO_SIZE (li_ssing->data);
        while (li_ssed != NULL) {
            long int lit_ssed = GPOINTER_TO_SIZE (li_ssed->data);
            if (lit_ssing == lit_ssed) break;

            li_ssed = li_ssed->next;
        }

        if (li_ssed == NULL) return false;
    }

    return true;
}
