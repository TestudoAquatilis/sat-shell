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

#include "sat_shell.h"

#include <stdio.h>
#include <stdbool.h>
#include <glib.h>

/* struct for storing command line options */
struct sat_shell_options {
    char *script_name;
    bool version;
};

/* command line options */
static struct sat_shell_options options = {
    .script_name = NULL,
    .version     = false
};

/* command line options definition */
static GOptionEntry option_entries[] = {
    {"script",  's', 0, G_OPTION_ARG_FILENAME, &options.script_name, "Execute tcl script from FILE instead of running in shell mode", "FILE"},
    {"version", 'v', 0, G_OPTION_ARG_NONE,     &options.version,     "Print version info",                                            NULL},
    {NULL}
};

/* command line options parsing */
static bool sat_shell_parse_options (int *argcp, char **argvp[])
{
    GError         *option_error   = NULL;
    GOptionContext *option_context = g_option_context_new (NULL);

    bool result = true;

    g_option_context_set_summary (option_context, "sat solver interactive tcl shell");
    g_option_context_add_main_entries (option_context, option_entries, NULL);

    if (!g_option_context_parse (option_context, argcp, argvp, &option_error)) {
        fprintf (stderr, "Error parsing command line options: %s\n", option_error->message);
        g_error_free (option_error);
        result = false;
    }

    g_option_context_free (option_context);

    return result;
}

int main (int argc, char *argv[])
{
    /* arguments */
    if (!sat_shell_parse_options (&argc, &argv)) {
        return 1;
    }

    if (options.version) {
        printf ("sat-shell version: %s\n", VERSION_STRING);
        return 0;
    }

    /* new sat shell */
    SatShell shell = sat_shell_new ();

    /* run */
    if (shell != NULL) {
        if (options.script_name != NULL) {
            sat_shell_run_script (shell, options.script_name);
            g_free (options.script_name);
        } else {
            sat_shell_run_shell (shell);
        }
    }

    /* free */
    sat_shell_free (&shell);

    return 0;
}
