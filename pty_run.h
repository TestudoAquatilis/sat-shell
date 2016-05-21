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

#ifndef __pty_run_h__
#define __pty_run_h__

#include <glib.h>

typedef struct pty_run_data *PTYRunData;

/* create new subprocess in pty with exec + arg list given in exec_arg_list
 * returns data needed for interaction or NULL on failure */
PTYRunData pty_run_new (GSList *exec_arg_list);

/* wait on child an free data */
void pty_run_finish (PTYRunData *data);

/* return line read from child process stdout/err without newline
 * or NULL if no new line can be read */
const char * pty_run_getline (PTYRunData data);

#endif
