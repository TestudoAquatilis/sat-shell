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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <pty.h>
#include <unistd.h>
#include <sys/wait.h>

/* data of subprocess in pty */
struct pty_run_data {
    /* child process id */
    pid_t   child_pid;
    /* filedescriptor id for stdin/out/err */
    int     pty_fd;
    /* argument list for childprocess as array */
    GArray  *arg_list;
    /* line data for reading from stdout/err  */
    GString *line_data;
    /* set to true if last character read */
    bool    done;
};

/* function run by child after fork, argv = argument list (argv[0] = executable) */
static void pty_run_child (char *argv[]);

/* create new subprocess in pty with exec + arg list given in exec_arg_list
 * returns data needed for interaction or NULL on failure */
struct pty_run_data * pty_run_new (GSList *exec_arg_list)
{
    struct pty_run_data *result = g_slice_new (struct pty_run_data);
    result->line_data = NULL;
    result->done = false;
    
    /* argument list */
    result->arg_list = g_array_new (true, false, sizeof (char *));
    for (GSList *li = exec_arg_list; li != NULL; li = li->next) {
        g_array_append_val (result->arg_list, li->data);
    }

    pid_t pid = forkpty (&(result->pty_fd), NULL, NULL, NULL);

    if (pid == 0) {
        /* child */
        pty_run_child ((char **) result->arg_list->data);
        /* not reached */
    } else if (pid == -1) {
        /* error */
        g_array_free (result->arg_list, true);
        g_slice_free (struct pty_run_data, result);
        fprintf (stderr, "Error: failed to start pty for subprocess\n");
        return NULL;
    }

    result->child_pid = pid;

    return result;
}

/* function run by child after fork, argv = argument list (argv[0] = executable) */
static void pty_run_child (char *argv[])
{
    if (execvp (argv[0], argv) < 0) {
        fprintf (stderr, "Error: failed to execute %s\n", argv[0]);
        exit (-1);
    }
    /* not reached */
}

/* wait on child an free data */
void pty_run_finish (struct pty_run_data **data)
{
    if (data == NULL) return;

    struct pty_run_data *rdata = *data;
    if (rdata == NULL) return;
    
    waitpid (rdata->child_pid, NULL, 0);
    close (rdata->pty_fd);

    g_array_free (rdata->arg_list, true);

    if (rdata->line_data != NULL) {
        g_string_free (rdata->line_data, true);
    }

    g_slice_free (struct pty_run_data, rdata);

    *data = NULL;
}

/* return line read from child process stdout/err without newline
 * or NULL if no new line can be read */
const char * pty_run_getline (struct pty_run_data *data)
{
    if (data == NULL) return NULL;

    if (data->done) return NULL;

    if (data->line_data == NULL) {
        data->line_data = g_string_new (NULL);
    }

    GString *line = data->line_data;

    g_string_assign (line, "");

    char read_char;
    int  pty_fd = data->pty_fd;

    while (read (pty_fd, &read_char, 1) > 0) {
        if (read_char == '\n') return line->str;
        g_string_append_c (line, read_char);
    }

    data->done = true;

    if (line->len > 0) return line->str;
    return NULL;
}
