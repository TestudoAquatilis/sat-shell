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

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <glib.h>

char inbuf [1024];

int main (int argc, char *argv[])
{
    SatFormula formula = NULL;

    GString *expr = g_string_new (NULL);
    bool iterate  = true;

    if (argc > 1) {
        g_string_assign (expr, argv[1]);
        iterate = false;
    }

    while (true) {
        if (iterate) {
            g_string_assign (expr, "");
            while (true) {
                if (fgets (inbuf, 1024, stdin) != NULL) {
                    g_string_append (expr, inbuf);
                    if (expr->str [expr->len - 1] == '\n') break;
                } else {
                    break;
                }
            }
        }

        if (strcmp (expr->str, "exit\n") == 0) break;

        formula = sat_formula_parse (expr->str);

        sat_formula_print (formula);

        GList *clause_list = sat_formula_to_cnf (formula);

        for (GList *li1 = clause_list; li1 != NULL; li1 = li1->next) {
            GQueue *clause = li1->data;
            printf ("[");
            for (GList *li2 = clause->head; li2 != NULL; li2 = li2->next) {
                long int lit = GPOINTER_TO_SIZE (li2->data);
                if (li2->next != NULL) {
                    printf ("%ld, ", lit);
                } else {
                    printf ("%ld", lit);
                }
            }
            printf ("]\n");
            g_queue_free (clause);
        }

        g_list_free (clause_list);

        sat_formula_free (&formula);

        if (!iterate) break;
    }

    if (argc <= 1) {
        g_string_free (expr, true);
    }

    return 0;
}
