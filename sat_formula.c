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

#include "sat_formula.h"
#include "sat_formula_parser.h"
#include "sat_formula_lexer.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

struct sat_formula {
    enum sat_formula_tag_t tag;
    long int literal;
    struct sat_formula *left_operand;
    struct sat_formula *right_operand;
};

static struct sat_formula * sat_formula_clause_find_non_cnf (GList *formula_clause);
static GList * sat_formula_clause_to_sorted_clause (GList *formula_clause);
static void sat_formula_clause_process (struct sat_formula *non_cnf_formula, GList *formula_clause, GQueue *formula_clause_queue);
static void sat_formula_clause_transform_inversion (struct sat_formula *non_cnf_formula);

static gint sat_formula_literal_compare_func (gconstpointer a, gconstpointer b);
static gint sat_formula_clause_compare_func (gconstpointer a, gconstpointer b);

static bool sat_formula_cnf_sorted_clause_is_true (GList *sorted_clause);
static bool sat_formula_cnf_sorted_clause_is_subsummed (GList *sorted_clause_subsummed, GList *sorted_clause_subsumming);
static GList * sat_formula_cnf_insert_reduce (GList *sorted_clause_list, GList *sorted_clause);

struct sat_formula * sat_formula_new_literal (long int literal)
{
    struct sat_formula *result = (struct sat_formula *) malloc (sizeof (struct sat_formula));
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

struct sat_formula * sat_formula_new_operation (enum sat_formula_tag_t tag, struct sat_formula *left, struct sat_formula *right)
{
    struct sat_formula *result = (struct sat_formula *) malloc (sizeof (struct sat_formula));
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
    free (formula);
    *fpointer = NULL;
}

static void sat_formula_print_nonewline (struct sat_formula *formula)
{
    if (formula == NULL) {
        fprintf (stderr, "ERROR - null pointer!\n");
        return;
    }

    switch (formula->tag) {
        case SAT_FORMULA_TAG_LITERAL:
            printf ("%ld", formula->literal);
            break;
        case SAT_FORMULA_TAG_INVERSION:
            printf ("-");
            sat_formula_print_nonewline (formula->left_operand);
            break;
        case SAT_FORMULA_TAG_OP_OR:
            printf ("(");
            sat_formula_print_nonewline (formula->left_operand);
            printf (" or ");
            sat_formula_print_nonewline (formula->right_operand);
            printf (")");
            break;
        case SAT_FORMULA_TAG_OP_AND:
            printf ("(");
            sat_formula_print_nonewline (formula->left_operand);
            printf (" and ");
            sat_formula_print_nonewline (formula->right_operand);
            printf (")");
            break;
        case SAT_FORMULA_TAG_OP_XOR:
            printf ("(");
            sat_formula_print_nonewline (formula->left_operand);
            printf (" xor ");
            sat_formula_print_nonewline (formula->right_operand);
            printf (")");
            break;
        case SAT_FORMULA_TAG_OP_EQUAL:
            printf ("(");
            sat_formula_print_nonewline (formula->left_operand);
            printf (" <=> ");
            sat_formula_print_nonewline (formula->right_operand);
            printf (")");
            break;
        case SAT_FORMULA_TAG_OP_RIMPL:
            printf ("(");
            sat_formula_print_nonewline (formula->left_operand);
            printf (" => ");
            sat_formula_print_nonewline (formula->right_operand);
            printf (")");
            break;
        case SAT_FORMULA_TAG_OP_LIMPL:
            printf ("(");
            sat_formula_print_nonewline (formula->left_operand);
            printf (" <= ");
            sat_formula_print_nonewline (formula->right_operand);
            printf (")");
            break;
        default:
            fprintf (stderr, "ERROR - unknown tag!\n");
            break;
    }
}

void sat_formula_print (struct sat_formula *formula)
{
    sat_formula_print_nonewline (formula);
    printf ("\n");
}

struct sat_formula * sat_formula_duplicate (struct sat_formula *formula)
{
    if (formula == NULL) return NULL;

    struct sat_formula *result = (struct sat_formula *) malloc (sizeof (struct sat_formula));
    if (result == NULL) return NULL;

    result->tag           = formula->tag;
    result->literal       = formula->literal;
    result->left_operand  = sat_formula_duplicate (formula->left_operand);
    result->right_operand = sat_formula_duplicate (formula->right_operand);

    return result;
}

struct sat_formula *sat_formula_parse (const char *expr)
{
    struct sat_formula *formula = NULL;
    yyscan_t scanner;
    YY_BUFFER_STATE state;

    if (yylex_init (&scanner)) {
        // couldn't initialize
        return NULL;
    }

    state = yy_scan_string (expr, scanner);

    if (yyparse (&formula, scanner)) {
        // error parsing
        sat_formula_free (&formula);

        yy_delete_buffer (state, scanner);

        yylex_destroy (scanner);

        return NULL;
    }

    yy_delete_buffer (state, scanner);

    yylex_destroy (scanner);

    return formula;
}

GList * sat_formula_to_cnf (struct sat_formula *formula)
{
    if (formula == NULL) return NULL;
    formula = sat_formula_duplicate (formula);

    GQueue *formula_clause_queue = g_queue_new ();
    GList  *formula_clause       = NULL;
    GList  *result_list          = NULL;
    formula_clause               = g_list_prepend (formula_clause, formula);

    g_queue_push_head (formula_clause_queue, formula_clause);

    while (! g_queue_is_empty (formula_clause_queue)) {
        GList *formula_clause = g_queue_pop_head (formula_clause_queue);
        if (formula_clause == NULL) continue;

        struct sat_formula *non_cnf = sat_formula_clause_find_non_cnf (formula_clause);
        if (non_cnf == NULL) {
            GList *sorted_clause = sat_formula_clause_to_sorted_clause (formula_clause);

            for (GList *li = formula_clause; li != NULL; li = li->next) {
                sat_formula_free ((struct sat_formula **) &li->data);
            }
            g_list_free (formula_clause);

            result_list = sat_formula_cnf_insert_reduce (result_list, sorted_clause);
        } else {
            sat_formula_clause_process (non_cnf, formula_clause, formula_clause_queue);
        }
    }

    g_queue_free (formula_clause_queue);

    return result_list;
}

static struct sat_formula * sat_formula_clause_find_non_cnf (GList *formula_clause)
{
    if (formula_clause == NULL) return NULL;

    for (GList *li = formula_clause; li != NULL; li = li->next) {
        struct sat_formula *f = li->data;
        if (f->tag != SAT_FORMULA_TAG_LITERAL) return f;
    }

    return NULL;
}

static gint sat_formula_literal_compare_func (gconstpointer a, gconstpointer b)
{
    long int ia = GPOINTER_TO_INT (a);
    long int ib = GPOINTER_TO_INT (b);

    if (ia + ib == 0) return ib-ia;
    if (ia < 0) ia = -ia;
    if (ib < 0) ib = -ib;

    return ia - ib;
}

static gint sat_formula_clause_compare_func (gconstpointer a, gconstpointer b)
{
    GList *clause_a = (GList *) a;
    GList *clause_b = (GList *) b;

    guint len_a = g_list_length (clause_a);
    guint len_b = g_list_length (clause_b);

    return len_a - len_b;
}

static GList * sat_formula_clause_to_sorted_clause (GList *formula_clause)
{
    GList *result = NULL;

    for (GList *li = formula_clause; li != NULL; li = li->next) {
        struct sat_formula *f = li->data;
        result = g_list_prepend (result, GINT_TO_POINTER (f->literal));
    }
    result = g_list_sort (result, sat_formula_literal_compare_func);

    return result;
}

static void sat_formula_clause_process (struct sat_formula *non_cnf_formula, GList *formula_clause, GQueue *formula_clause_queue)
{
    if (non_cnf_formula == NULL) return;
    if (formula_clause == NULL) return;
    if (formula_clause_queue == NULL) return;

    if (non_cnf_formula->tag == SAT_FORMULA_TAG_LITERAL) return;

    if (non_cnf_formula->tag == SAT_FORMULA_TAG_INVERSION) {
        sat_formula_clause_transform_inversion (non_cnf_formula);
        g_queue_push_head (formula_clause_queue, formula_clause);
        return;
    }

    if (non_cnf_formula->tag == SAT_FORMULA_TAG_OP_RIMPL) {
        struct sat_formula *noninv     = non_cnf_formula->right_operand;
        non_cnf_formula->right_operand = NULL;
        non_cnf_formula->tag           = SAT_FORMULA_TAG_INVERSION;
        formula_clause = g_list_prepend (formula_clause, noninv);

        g_queue_push_head (formula_clause_queue, formula_clause);
        return;
    }

    if (non_cnf_formula->tag == SAT_FORMULA_TAG_OP_LIMPL) {
        struct sat_formula *noninv     = non_cnf_formula->left_operand;
        non_cnf_formula->left_operand  = non_cnf_formula->right_operand;
        non_cnf_formula->right_operand = NULL;
        non_cnf_formula->tag           = SAT_FORMULA_TAG_INVERSION;
        formula_clause = g_list_prepend (formula_clause, noninv);

        g_queue_push_head (formula_clause_queue, formula_clause);
        return;
    }

    if (non_cnf_formula->tag == SAT_FORMULA_TAG_OP_OR) {
        struct sat_formula *sub_a = non_cnf_formula->left_operand;

        formula_clause = g_list_prepend (formula_clause, non_cnf_formula->right_operand);

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

    if (non_cnf_formula->tag == SAT_FORMULA_TAG_OP_AND) {
        GList *formula_clause2 = NULL;

        for (GList *li = formula_clause; li != NULL; li = li->next) {
            struct sat_formula *f = li->data;
            if (f == non_cnf_formula) {
                li->data        = f->left_operand;
                formula_clause2 = g_list_prepend (formula_clause2, f->right_operand);
            } else {
                formula_clause2 = g_list_prepend (formula_clause2, sat_formula_duplicate (f));
            }
        }

        non_cnf_formula->left_operand  = NULL;
        non_cnf_formula->right_operand = NULL;
        sat_formula_free (&non_cnf_formula);

        g_queue_push_head (formula_clause_queue, formula_clause);
        g_queue_push_head (formula_clause_queue, formula_clause2);
        return;
    }

    if (non_cnf_formula->tag == SAT_FORMULA_TAG_OP_XOR) {
        GList *formula_clause2 = NULL;

        for (GList *li = formula_clause; li != NULL; li = li->next) {
            struct sat_formula *f = li->data;
            if (f == non_cnf_formula) {
                li->data        = f->left_operand;
                struct sat_formula * b1 = sat_formula_duplicate (f->left_operand);
                struct sat_formula * b2 = sat_formula_duplicate (f->right_operand);
                b1 = sat_formula_new_operation (SAT_FORMULA_TAG_INVERSION, b1, NULL);
                b2 = sat_formula_new_operation (SAT_FORMULA_TAG_INVERSION, b2, NULL);
                formula_clause2 = g_list_prepend (formula_clause2, b1);
                formula_clause2 = g_list_prepend (formula_clause2, b2);
            } else {
                formula_clause2 = g_list_prepend (formula_clause2, sat_formula_duplicate (f));
            }
        }
        formula_clause = g_list_prepend (formula_clause, non_cnf_formula->right_operand);

        non_cnf_formula->left_operand  = NULL;
        non_cnf_formula->right_operand = NULL;
        sat_formula_free (&non_cnf_formula);

        g_queue_push_head (formula_clause_queue, formula_clause);
        g_queue_push_head (formula_clause_queue, formula_clause2);
        return;
    }

    if (non_cnf_formula->tag == SAT_FORMULA_TAG_OP_EQUAL) {
        GList *formula_clause2 = NULL;

        for (GList *li = formula_clause; li != NULL; li = li->next) {
            struct sat_formula *f = li->data;
            if (f == non_cnf_formula) {
                li->data        = f->left_operand;
                struct sat_formula * b1 = sat_formula_duplicate (f->left_operand);
                struct sat_formula * b2 = sat_formula_duplicate (f->right_operand);
                b1 = sat_formula_new_operation (SAT_FORMULA_TAG_INVERSION, b1, NULL);
                formula_clause2 = g_list_prepend (formula_clause2, b1);
                formula_clause2 = g_list_prepend (formula_clause2, b2);
            } else {
                formula_clause2 = g_list_prepend (formula_clause2, sat_formula_duplicate (f));
            }
        }

        non_cnf_formula->left_operand  = non_cnf_formula->right_operand;
        non_cnf_formula->right_operand = NULL;
        non_cnf_formula->tag           = SAT_FORMULA_TAG_INVERSION;
        formula_clause = g_list_prepend (formula_clause, non_cnf_formula);

        g_queue_push_head (formula_clause_queue, formula_clause);
        g_queue_push_head (formula_clause_queue, formula_clause2);
        return;
    }
}

static void sat_formula_clause_transform_inversion (struct sat_formula *non_cnf_formula)
{
        struct sat_formula *sub_f = non_cnf_formula->left_operand;

        /* -(a) ... -a */
        if (sub_f->tag == SAT_FORMULA_TAG_LITERAL) {
            non_cnf_formula->literal = -sub_f->literal;
            non_cnf_formula->tag     = SAT_FORMULA_TAG_LITERAL;
            sat_formula_free (&non_cnf_formula->left_operand);
            return;
        }

        /* -(-(f)) ... f */
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

        /* -(f1 xor f2) ... f1 ==  f2 */
        /* -(f1 ==  f2) ... f1 xor f2 */
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

        /* -(f1 and f2) ... (-f1) or  (-f2) */
        /* -(f1 or  f2) ... (-f1) and (-f2) */
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

        /* -(f1 => f2) ... -((-f1) or   f2)  ...   f1  and (-f2) */
        /* -(f1 <= f2) ... -(  f1  or (-f2)) ... (-f1) and   f2  */
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

static GList * sat_formula_cnf_insert_reduce (GList *sorted_clause_list, GList *sorted_clause)
{
    if (sorted_clause == NULL) return sorted_clause_list;

    if (sat_formula_cnf_sorted_clause_is_true (sorted_clause)) {
        g_list_free (sorted_clause);
        return sorted_clause_list;
    }

    GList *li = sorted_clause_list;
    guint len_insert = g_list_length (sorted_clause);

    /* clause to insert subsummed? */
    while (li != NULL) {
        GList *current_clause = li->data;
        if (g_list_length (current_clause) > len_insert) break;

        if (sat_formula_cnf_sorted_clause_is_subsummed (sorted_clause, current_clause)) {
            g_list_free (sorted_clause);
            return sorted_clause_list;
        }

        li = li->next;
    }

    /* existing clause is subsummed? */
    while (li != NULL) {
        GList *current_clause = li->data;
        GList *li_next = li->next;

        if (sat_formula_cnf_sorted_clause_is_subsummed (current_clause, sorted_clause)) {
            g_list_free (current_clause);
            sorted_clause_list = g_list_delete_link (sorted_clause_list, li);
        }

        li = li_next;
    }

    return g_list_insert_sorted (sorted_clause_list, sorted_clause, sat_formula_clause_compare_func);
}

static bool sat_formula_cnf_sorted_clause_is_true (GList *sorted_clause)
{
    for (GList *li = sorted_clause; li != NULL; li = li->next) {
        if (li->next != NULL) {
            long int this_literal = GPOINTER_TO_INT (li->data);
            long int next_literal = GPOINTER_TO_INT (li->next->data);

            if (this_literal + next_literal == 0) return true;
        }
    }

    return false;
}

static bool sat_formula_cnf_sorted_clause_is_subsummed (GList *sorted_clause_subsummed, GList *sorted_clause_subsumming)
{
    if (sorted_clause_subsumming == NULL) return true;
    if (sorted_clause_subsummed == NULL) return false;

    GList *li_ssed = sorted_clause_subsummed;
    for (GList *li_ssing = sorted_clause_subsumming; li_ssing != NULL; li_ssing = li_ssing->next) {
        long int lit_ssing = GPOINTER_TO_INT (li_ssing->data);
        while (li_ssed != NULL) {
            long int lit_ssed = GPOINTER_TO_INT (li_ssed->data);
            if (lit_ssing == lit_ssed) break;

            li_ssed = li_ssed->next;
        }

        if (li_ssed == NULL) return false;
    }

    return true;
}
