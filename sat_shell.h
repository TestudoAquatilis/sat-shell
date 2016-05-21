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

#ifndef __sat_shell_h__
#define __sat_shell_h__

#include <stdbool.h>

typedef struct sat_shell *SatShell;

/* allocate and return new sat_shell */
SatShell sat_shell_new ();
/* free sat */
void sat_shell_free (SatShell *sat);

/* run sat shell in shell mode */
void sat_shell_run_shell (SatShell sat);
/* run sat shell in script mode: execute given script */
void sat_shell_run_script (SatShell sat, const char *script);

/* print license info: short/long version */
void sat_shell_license_info (bool small);

#endif
