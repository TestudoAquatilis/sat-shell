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

#include "pty_run.h"

#include <stdio.h>
#include <stdbool.h>

int main (int argc, char *argv[])
{
    GSList *execlist = NULL;

    if (argc <= 1) return -1;

    for (int i = 1; i < argc; i++) {
        execlist = g_slist_prepend (execlist, argv[i]);
    }
    execlist = g_slist_reverse (execlist);

    PTYRunData run_data = pty_run_new (execlist);
    g_slist_free (execlist);

    while (true) {
        const char *line = pty_run_getline (run_data);

        if (line == NULL) break;

        fprintf (stderr, "PTY: %s\n", line);
    }

    pty_run_finish (&run_data);

    return 0;
}
