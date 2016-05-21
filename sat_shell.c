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
#include "sat_problem.h"

#include <tclln.h>
#include <tcl.h>
#include <glib.h>
#include <string.h>

/* sat_shell data */
struct sat_shell {
    /* tclln data */
    TclLN tclln;
    /* sat_problem data */
    SatProblem sat;
};

/* Tcl helper function for parsing boolean arguments */
static int sat_shell_tcl_bool_parse (ClientData client_data, Tcl_Obj *obj, void *dest_ptr);
/* Tcl helper function for parsing lists in GSLists */
static int sat_shell_tcl_string_list_parse (ClientData client_data, Tcl_Obj *obj, void *dest_ptr);
/* Tcl helper function for parsing lists of lists in GSLists of GSLists */
static int sat_shell_tcl_string_list_list_parse (ClientData client_data, Tcl_Obj *obj, void *dest_ptr);

/* tcl commands */
static int sat_shell_command_add_clause      (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static int sat_shell_command_add_encoding    (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static int sat_shell_command_add_formula     (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static int sat_shell_command_solve           (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static int sat_shell_command_reset           (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static int sat_shell_command_cancel_solution (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static int sat_shell_command_get_var_result  (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static int sat_shell_command_get_var_mapping (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static int sat_shell_command_get_clauses     (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static int sat_shell_command_help            (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static int sat_shell_command_license         (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);

/* command info */
struct sat_shell_command_data {
    const char        *command;
    const char *const *completion_list;
    Tcl_ObjCmdProc    *proc;
    const char        *help;
};

/* all commands and their data */
static struct sat_shell_command_data sat_shell_command_data_list [] = {
    {"add_clause",
        (const char * const []) {"-clause", "-list", "-help", NULL},
        sat_shell_command_add_clause,
        "Add a clause or list of clauses to current sat problem."
    },
    {"add_encoding",
        (const char * const []) {"-literals", "-encoding", "-parameter", NULL},
        sat_shell_command_add_encoding,
        "Add special encoding (e.g. 1 of n order encoding) for a list of literals."
    },
    {"add_formula",
        (const char * const []) {"-formula", "-mapping", NULL},
        sat_shell_command_add_formula,
        "Add a formula with 1 ... n and map them to a list of literals in the current problem.\n"
        "Parsed formula strings are cached to make multiple usage of same formula string more efficient."
    },
    {"solve",
        (const char * const []) {"-tempfile_keep", "-tempfile_clean", "-tempfile_base", "-compress_cnf", "-plain_cnf", "-solver_binary", "-solution_on_stdout", "-help", NULL},
        sat_shell_command_solve,
        "Solve current sat problem - return true if satisfiable."
    },
    {"reset",
        (const char * const []) {"-help", NULL},
        sat_shell_command_reset,
        "Reset sat problem - deletes all currently added clauses and variables."
    },
    {"cancel_solution",
        (const char * const []) {"-help", NULL},
        sat_shell_command_cancel_solution,
        "Invalidate current solution - on next \"solve\" another solution must be generated if still satisfiable."
    },
    {"get_var_result",
        (const char * const []) {"-var", "-assignment", "-help", NULL},
        sat_shell_command_get_var_result,
        "Get assignment for variables after problem has been solved."
    },
    {"get_var_mapping",
        (const char * const []) {"-name", "-number", "-help", NULL},
        sat_shell_command_get_var_mapping,
        "Get mapping of named literals in sat problem to enumerated literals for solver."
    },
    {"get_clauses",
        (const char * const []) {"-help", NULL},
        sat_shell_command_get_clauses,
        "Get all clauses of current sat problem."
    },
    {"help",
        (const char * const []) {"-help", NULL},
        sat_shell_command_help,
        "Print this help text."
    },
    {"license",
        (const char * const []) {"-help", NULL},
        sat_shell_command_license,
        "Print License information."
    },
    {NULL, NULL, NULL, NULL}
};

/* allocate and return new sat_shell */
struct sat_shell * sat_shell_new ()
{
    struct sat_shell *result = g_slice_new (struct sat_shell);

    if (result == NULL) return NULL;

    result->tclln = tclln_new ("sat-shell");
    if (result->tclln == NULL) {
        sat_shell_free (&result);
        return NULL;
    }

    tclln_provide_completion_command (result->tclln, NULL);

    for (struct sat_shell_command_data *di = &sat_shell_command_data_list[0]; di->command != NULL; di++) {
        tclln_add_command (result->tclln, di->command, di->completion_list, di->proc, (ClientData) result, NULL);
    }

    tclln_set_prompt  (result->tclln, "sat-shell> ",
                                      "         : ");

    result->sat = sat_problem_new ();
    if (result->sat == NULL) {
        sat_shell_free (&result);
        return NULL;
    }

    return result;
}

/* free sat */
void sat_shell_free (struct sat_shell **sat)
{
    if (sat == NULL) return;

    struct sat_shell *s = *sat;
    if (s == NULL) return;

    if (s->tclln != NULL) {
        tclln_free (s->tclln);
    }

    if (s->sat != NULL) {
        sat_problem_free (&(s->sat));
    }

    g_slice_free (struct sat_shell, s);
    *sat = NULL;
}

/* run sat shell in shell mode */
void sat_shell_run_shell (struct sat_shell *sat)
{
    if (sat == NULL) return;
    if (sat->tclln == NULL) return;

    tclln_run (sat->tclln);
}

/* run sat shell in script mode: execute given script */
void sat_shell_run_script (struct sat_shell *sat, const char *script)
{
    if (sat == NULL) return;
    if (sat->tclln == NULL) return;

    tclln_run_file (sat->tclln, script, false);
}

/* Tcl helper function for parsing boolean arguments */
static int sat_shell_tcl_bool_parse (ClientData client_data, Tcl_Obj *obj, void *dest_ptr)
{
    bool *bool_dest = (bool *) dest_ptr;
    if (dest_ptr == NULL) {
        return 1;
    }

    if (obj == NULL) {
        return -1;
    }

    int bool_result;
    if (Tcl_GetBooleanFromObj (NULL, obj, &bool_result) != TCL_OK) {
        return -1;
    }

    *bool_dest = (bool_result ? true : false);

    return 1;
}

/* Tcl helper function for parsing lists in GSLists */
static int sat_shell_tcl_string_list_parse (ClientData client_data, Tcl_Obj *obj, void *dest_ptr)
{
    GSList **list_dest = (GSList **) dest_ptr;
    if (dest_ptr == NULL) {
        return 1;
    }

    GSList *result = NULL;

    if (obj == NULL) {
        return 1;
    }

    Tcl_Obj *in_list = obj;

    int len = 0;
    int temp_result = Tcl_ListObjLength (NULL, in_list, &len);
    if (temp_result != TCL_OK) return -1;

    for (int i = 0; i < len; i++) {
        Tcl_Obj *lit;
        temp_result = Tcl_ListObjIndex (NULL, in_list, i, &lit);
        if (temp_result != TCL_OK) {
            g_slist_free (result);
            return -1;
        }
        if (lit == NULL) {
            g_slist_free (result);
            return -1;
        }
        char *lit_str = Tcl_GetString (lit);

        result = g_slist_prepend (result, lit_str);
    }

    *list_dest = g_slist_reverse(result);
    return 1;
}

/* Tcl helper function for parsing lists of lists in GSLists of GSLists */
static int sat_shell_tcl_string_list_list_parse (ClientData client_data, Tcl_Obj *obj, void *dest_ptr)
{
    GSList **list_dest = (GSList **) dest_ptr;
    if (dest_ptr == NULL) {
        return 1;
    }

    GSList *result = NULL;

    if (obj == NULL) {
        return 1;
    }

    Tcl_Obj *in_list = obj;

    int len = 0;
    int temp_result = Tcl_ListObjLength (NULL, in_list, &len);
    if (temp_result != TCL_OK) return -1;

    for (int i = 0; i < len; i++) {
        GSList *result_sub = NULL;
        Tcl_Obj *list_sub;
        temp_result = Tcl_ListObjIndex (NULL, in_list, i, &list_sub);
        if (temp_result != TCL_OK) {
            goto string_list_list_parse_error_exit;
            return -1;
        }

        int len_sub = 0;
        int temp_result = Tcl_ListObjLength (NULL, list_sub, &len_sub);
        if (temp_result != TCL_OK) {
            goto string_list_list_parse_error_exit;
            return -1;
        }

        for (int i_sub = 0; i_sub < len_sub; i_sub++) {
            Tcl_Obj *lit;
            temp_result = Tcl_ListObjIndex (NULL, list_sub, i_sub, &lit);
            if (temp_result != TCL_OK) {
                g_slist_free (result_sub);
                goto string_list_list_parse_error_exit;
            }
            if (lit == NULL) {
                g_slist_free (result_sub);
                goto string_list_list_parse_error_exit;
            }
            char *lit_str = Tcl_GetString (lit);

            result_sub = g_slist_prepend (result_sub, lit_str);
        }
        result_sub = g_slist_reverse (result_sub);
        result = g_slist_prepend (result, result_sub);
    }

    *list_dest = g_slist_reverse (result);
    return 1;

string_list_list_parse_error_exit:
    g_slist_free_full (result, (GDestroyNotify) g_slist_free);
    return -1;
}

/* Tcl command for adding clauses: add_clause -clause <clause as list> | -list <list of clauses as lists> */
static int sat_shell_command_add_clause (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    GSList *clause = NULL;
    GSList *clist  = NULL;
    SatProblem sat = ((struct sat_shell *) client_data)->sat;

    Tcl_ArgvInfo arg_table [] = {
        {TCL_ARGV_FUNC,     "-clause", (void*) (Tcl_ArgvFuncProc*) sat_shell_tcl_string_list_parse,      (void*) &clause, "the clause as list of literals", NULL},
        {TCL_ARGV_FUNC,     "-list",   (void*) (Tcl_ArgvFuncProc*) sat_shell_tcl_string_list_list_parse, (void*) &clist,  "list of clauses as list of literals", NULL},
        TCL_ARGV_AUTO_HELP,
        TCL_ARGV_TABLE_END
    };

    int result = Tcl_ParseArgsObjv (interp, arg_table, &objc, objv, NULL);
    if (result != TCL_OK) return result;

    if ((clause == NULL) && (clist == NULL)) {
        Tcl_SetObjResult (interp, Tcl_NewStringObj ("error: expected a clause or a list of clauses", -1));
        return TCL_ERROR;
    }

    if (clause != NULL) {
        sat_problem_add_clause_gslist (sat, clause);

        g_slist_free (clause);
    }

    if (clist != NULL) {
        for (GSList *li = clist; li != NULL; li = li->next) {
            GSList *i_clause = (GSList *) li->data;
            sat_problem_add_clause_gslist (sat, i_clause);
            g_slist_free (i_clause);
        }
        g_slist_free (clist);
    }

    return TCL_OK;
}

/* Tcl command for adding encoding: add_encoding -literals <literals as list> -encoding (1ofn|mofn) [-parameter <parameter>] */
static int sat_shell_command_add_encoding (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    GSList *lit_list = NULL;
    SatProblem sat = ((struct sat_shell *) client_data)->sat;
    const char *encoding = NULL;
    int parameter = -1;

    Tcl_ArgvInfo arg_table [] = {
        {TCL_ARGV_FUNC,     "-literals",  (void*) (Tcl_ArgvFuncProc*) sat_shell_tcl_string_list_parse, (void *) &lit_list,  "the list of literals to apply encoding to", NULL},
        {TCL_ARGV_STRING,   "-encoding",  NULL,                                                        (void *) &encoding,  "the encoding to apply: one of \"1ofn\", \"2ofn\", \"mofn\" + parameter = m, \"1ofn_order\"", NULL},
        {TCL_ARGV_INT,      "-parameter", NULL,                                                        (void *) &parameter, "integer parameter for some encodings", NULL},
        TCL_ARGV_AUTO_HELP,
        TCL_ARGV_TABLE_END
    };

    int result = Tcl_ParseArgsObjv (interp, arg_table, &objc, objv, NULL);
    if (result != TCL_OK) return result;

    if (lit_list == NULL) {
        Tcl_SetObjResult (interp, Tcl_NewStringObj ("error: expected a list of literals", -1));
        return TCL_ERROR;
    }
    if (encoding == NULL) {
        Tcl_SetObjResult (interp, Tcl_NewStringObj ("error: expected an encoding", -1));
        g_slist_free (lit_list);
        return TCL_ERROR;
    }

    if (strcmp (encoding, "1ofn") == 0) {
        sat_problem_add_mofn_direct_encoding (sat, lit_list, 1);
    } else if (strcmp (encoding, "1ofn_order") == 0) {
        sat_problem_add_1ofn_order_encoding (sat, lit_list);
    } else if (strcmp (encoding, "2ofn") == 0) {
        sat_problem_add_mofn_direct_encoding (sat, lit_list, 2);
    } else if (strcmp (encoding, "mofn") == 0) {
        if (parameter > 0) {
            sat_problem_add_mofn_direct_encoding (sat, lit_list, parameter);
        } else {
            Tcl_SetObjResult (interp, Tcl_NewStringObj ("error: encoding \"mofn\" expects m as parameter in range 1 ... n", -1));
            g_slist_free (lit_list);
            return TCL_ERROR;
        }
    } else {
        Tcl_SetObjResult (interp, Tcl_NewStringObj ("error: encoding has to be one of \"1ofn\", \"2ofn\", \"mofn\", \"1ofn_order\"", -1));
        g_slist_free (lit_list);
        return TCL_ERROR;
    }

    g_slist_free (lit_list);

    return TCL_OK;
}

/* Tcl command for adding formulas: add_formula -formula <formula string> -mapping <literal mapping as list> */
static int sat_shell_command_add_formula (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    SatProblem sat = ((struct sat_shell *) client_data)->sat;

    GSList *mapping_list = NULL;
    const char *formula = NULL;

    Tcl_ArgvInfo arg_table [] = {
        {TCL_ARGV_STRING,   "-formula", NULL,                                                        (void *) &formula,      "raw formula with variables from 1 to n which are mapped to the literals of the mapping list", NULL},
        {TCL_ARGV_FUNC,     "-mapping", (void*) (Tcl_ArgvFuncProc*) sat_shell_tcl_string_list_parse, (void *) &mapping_list, "the list of literals to map on the encoded formula", NULL},
        TCL_ARGV_AUTO_HELP,
        TCL_ARGV_TABLE_END
    };

    int result = Tcl_ParseArgsObjv (interp, arg_table, &objc, objv, NULL);
    if (result != TCL_OK) return result;

    if (formula == NULL) {
        Tcl_SetObjResult (interp, Tcl_NewStringObj ("error: expected a formula to encode", -1));
        g_slist_free (mapping_list);
        return TCL_ERROR;
    }
    if (mapping_list == NULL) {
        Tcl_SetObjResult (interp, Tcl_NewStringObj ("error: expected a list of literals for mapping", -1));
        return TCL_ERROR;
    }

    if (!sat_problem_add_formula_mapping (sat, formula, mapping_list)) {
        g_slist_free (mapping_list);
        Tcl_SetObjResult (interp, Tcl_NewStringObj ("error: encoding + mapping failed", -1));
        return TCL_ERROR;
    }

    g_slist_free (mapping_list);

    return TCL_OK;
}

/* Tcl command for solving problem: solve [-tempfile_base <prefix>] [-solver_binary <binary>] [-solution_on_stdout] [-tempfile_clean|-tempfile_keep]
 *                                        [-compress_cnf|-plain_cnf] */
static int sat_shell_command_solve (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    SatProblem sat = ((struct sat_shell *) client_data)->sat;
    const char *tmp_file_basename = "tmp_cnf";
    const char *solver_bin        = "minisat";
    int solution_on_stdout        = false;
    int cleanup                   = true;
    int cnf_gz                    = true;

    int int_true  = true;
    int int_false = false;

    Tcl_ArgvInfo arg_table [] = {
        {TCL_ARGV_STRING,   "-tempfile_base",      NULL,                        (void *) &tmp_file_basename,  "filenames for cnf and solution are based on this name +suffixes", NULL},
        {TCL_ARGV_STRING,   "-solver_binary",      NULL,                        (void *) &solver_bin,         "executable of sat solver", NULL},
        {TCL_ARGV_CONSTANT, "-solution_on_stdout", (void *) &int_true,          (void *) &solution_on_stdout, "solver prints solution to stdout instead of a file", NULL},
        {TCL_ARGV_CONSTANT, "-tempfile_clean",     GINT_TO_POINTER (int_true),  (void *) &cleanup,            "remove temporary files after solving", NULL},
        {TCL_ARGV_CONSTANT, "-tempfile_keep",      GINT_TO_POINTER (int_false), (void *) &cleanup,            "keep temporary files after solving", NULL},
        {TCL_ARGV_CONSTANT, "-compress_cnf",       GINT_TO_POINTER (int_true),  (void *) &cnf_gz,             "compress cnf file", NULL},
        {TCL_ARGV_CONSTANT, "-plain_cnf",          GINT_TO_POINTER (int_false), (void *) &cnf_gz,             "do not compress cnf file", NULL},
        TCL_ARGV_AUTO_HELP,
        TCL_ARGV_TABLE_END
    };

    int result = Tcl_ParseArgsObjv (interp, arg_table, &objc, objv, NULL);
    if (result != TCL_OK) return result;

    sat_problem_solve (sat, tmp_file_basename, solver_bin, solution_on_stdout, cleanup, cnf_gz);

    bool error = false;
    bool satisfiable = sat_problem_satisfiable (sat, &error);

    if (error) {
        Tcl_SetObjResult (interp, Tcl_NewStringObj ("error while solving sat-problem", -1));
        return TCL_ERROR;
    }

    Tcl_SetObjResult (interp, Tcl_NewBooleanObj (satisfiable));

    return TCL_OK;
}

/* Tcl command for resetting problem: reset */
static int sat_shell_command_reset (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    struct sat_shell *shell = (struct sat_shell *) client_data;

    Tcl_ArgvInfo arg_table [] = {
        TCL_ARGV_AUTO_HELP,
        TCL_ARGV_TABLE_END
    };

    int result = Tcl_ParseArgsObjv (interp, arg_table, &objc, objv, NULL);
    if (result != TCL_OK) return result;

    if (shell->sat != NULL) {
        sat_problem_free (&(shell->sat));
    }
    shell->sat = sat_problem_new ();

    if (shell->sat == NULL) {
        Tcl_SetObjResult (interp, Tcl_NewStringObj ("internal allocation error", -1));
        return TCL_ERROR;
    }

    return TCL_OK;
}

/* Tcl command for cancelling current solution: cancel_solution */
static int sat_shell_command_cancel_solution (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    SatProblem sat = ((struct sat_shell *) client_data)->sat;

    Tcl_ArgvInfo arg_table [] = {
        TCL_ARGV_AUTO_HELP,
        TCL_ARGV_TABLE_END
    };

    int result = Tcl_ParseArgsObjv (interp, arg_table, &objc, objv, NULL);
    if (result != TCL_OK) return result;

    sat_problem_cancel_solution (sat);

    return TCL_OK;
}

/* Tcl command for getting results of variables: get_var_result [-var <var name>] [-assignment <assignment>] */
static int sat_shell_command_get_var_result (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    SatProblem sat       = ((struct sat_shell *) client_data)->sat;
    const char *var_name = NULL;
    bool assignment      = true;

    Tcl_ArgvInfo arg_table [] = {
        {TCL_ARGV_STRING,   "-var",        NULL,                                                 (void *) &var_name,   "var name to look up", NULL},
        {TCL_ARGV_FUNC,     "-assignment", (void*) (Tcl_ArgvFuncProc*) sat_shell_tcl_bool_parse, (void *) &assignment, "return list of variables assigned to given value", NULL},
        TCL_ARGV_AUTO_HELP,
        TCL_ARGV_TABLE_END
    };

    int result = Tcl_ParseArgsObjv (interp, arg_table, &objc, objv, NULL);
    if (result != TCL_OK) return result;

    if (var_name != NULL) {
        bool error = false;
        bool retval = sat_problem_var_result (sat, var_name, &error);

        if (error) {
            Tcl_SetObjResult (interp, Tcl_NewStringObj ("error while looking up variable", -1));
            return TCL_ERROR;
        }

        Tcl_SetObjResult (interp, Tcl_NewBooleanObj (retval));
    } else {
        bool error = false;
        GSList *result_list = sat_problem_var_result_list (sat, assignment, &error);

        if (error) {
            Tcl_SetObjResult (interp, Tcl_NewStringObj ("error while looking up results", -1));
            return TCL_ERROR;
        }

        Tcl_Obj *retval = Tcl_NewListObj (0, NULL);

        for (GSList *li = result_list; li != NULL; li = li->next) {
            const char *var_name = li->data;
            Tcl_Obj *var_obj = Tcl_NewStringObj (var_name, -1);
            Tcl_ListObjAppendElement (interp, retval, var_obj);
        }

        Tcl_SetObjResult (interp, retval);

        g_slist_free (result_list);
    }

    return TCL_OK;
}

/* Tcl command for obtaining mapping of variables: get_var_mapping [-name <var name>] [-number <var number>] */
static int sat_shell_command_get_var_mapping (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    SatProblem sat = ((struct sat_shell *) client_data)->sat;

    const char *var_name   = NULL;
    long int   var_number  = 0;

    Tcl_ArgvInfo arg_table [] = {
        {TCL_ARGV_STRING,   "-name",   NULL, (void *) &var_name,   "var name to look up", NULL},
        {TCL_ARGV_INT,      "-number", NULL, (void *) &var_number, "var number to look up", NULL},
        TCL_ARGV_AUTO_HELP,
        TCL_ARGV_TABLE_END
    };

    int result = Tcl_ParseArgsObjv (interp, arg_table, &objc, objv, NULL);
    if (result != TCL_OK) return result;

    if (var_name != NULL) {
        long int result = sat_problem_get_varnumber_from_name (sat, var_name);
        if (result == 0) {
            Tcl_SetObjResult (interp, Tcl_NewStringObj ("unknown variable", -1));
            return TCL_ERROR;
        }

        Tcl_SetObjResult (interp, Tcl_NewLongObj (result));
    } else if (var_number != 0) {
        const char *result = sat_problem_get_varname_from_number (sat, var_number);
        if (result == NULL) {
            Tcl_SetObjResult (interp, Tcl_NewStringObj ("unknown variable", -1));
            return TCL_ERROR;
        }

        Tcl_SetObjResult (interp, Tcl_NewStringObj (result, -1));
    } else {
        Tcl_SetObjResult (interp, Tcl_NewStringObj ("error while lookup up variable", -1));
        return TCL_ERROR;
    }

    return TCL_OK;
}

/* Tcl command for obtaining all clauses: get_clauses */
static int sat_shell_command_get_clauses (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    SatProblem sat = ((struct sat_shell *) client_data)->sat;

    Tcl_ArgvInfo arg_table [] = {
        TCL_ARGV_AUTO_HELP,
        TCL_ARGV_TABLE_END
    };

    int result = Tcl_ParseArgsObjv (interp, arg_table, &objc, objv, NULL);
    if (result != TCL_OK) return result;

    /* start creating result */
    Tcl_Obj *retval           = Tcl_NewListObj (0, NULL);
    GString *temp_str         = g_string_new (NULL);
    GHashTable *lit_to_tclobj = g_hash_table_new (g_direct_hash, g_direct_equal);
    GQueue *clause_list        = sat_problem_get_clauses_mapped (sat);

    for (GList *li = clause_list->head; li != NULL; li = li->next) {
        long int *clause = (long int *) li->data;
        if (clause == NULL) continue;

        Tcl_Obj *tcl_clause = Tcl_NewListObj (0, NULL);
        for (int i = 0; clause[i] != 0; i++) {
            long int lit = clause[i];

            Tcl_Obj *lit_obj = NULL;
            bool exists = g_hash_table_lookup_extended (lit_to_tclobj, GSIZE_TO_POINTER (lit), NULL, (void *) &lit_obj);
            if (!exists) {
                if (lit < 0) {
                    const char *var_name = sat_problem_get_varname_from_number (sat, -lit);
                    g_string_printf (temp_str, "-%s", var_name);
                } else {
                    const char *var_name = sat_problem_get_varname_from_number (sat, lit);
                    g_string_printf (temp_str, "%s", var_name);
                }
                lit_obj = Tcl_NewStringObj (temp_str->str, -1);
                g_hash_table_insert (lit_to_tclobj, GSIZE_TO_POINTER (lit), lit_obj);
            }

            Tcl_ListObjAppendElement (interp, tcl_clause, lit_obj);
        }
        Tcl_ListObjAppendElement (interp, retval, tcl_clause);
    }

    /* cleanup and return */
    g_string_free (temp_str, true);
    g_hash_table_destroy (lit_to_tclobj);

    Tcl_SetObjResult (interp, retval);
    return TCL_OK;
}

/* Tcl command for printing help */
static int sat_shell_command_help (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    Tcl_ArgvInfo arg_table [] = {
        TCL_ARGV_AUTO_HELP,
        TCL_ARGV_TABLE_END
    };

    int result = Tcl_ParseArgsObjv (interp, arg_table, &objc, objv, NULL);
    if (result != TCL_OK) return result;

    printf ("%s\n", "List of available special commands - to get more details for a specific command type <command> -help.\n");

    /* command length */
    int cmd_len = 0;

    for (struct sat_shell_command_data *di = &sat_shell_command_data_list[0]; di->command != NULL; di++) {
        int i_len = strlen (di->command);
        if (i_len > cmd_len) cmd_len = i_len;
    }
    cmd_len += 3;

    GString *line = g_string_new (NULL);
    GString *fill = g_string_new (NULL);

    g_string_assign (line, "<command>");
    g_string_assign (fill, "");
    while (line->len < cmd_len) {g_string_append_c (line, ' ');}
    while (fill->len < cmd_len) {g_string_append_c (fill, ' ');}

    g_string_append (line, "<info>");
    printf ("%s\n", line->str);

    for (struct sat_shell_command_data *di = &sat_shell_command_data_list[0]; di->command != NULL; di++) {
        g_string_assign (line, di->command);
        while (line->len < cmd_len) {g_string_append_c (line, ' ');}

        gchar **help_lines = g_strsplit (di->help, "\n", -1);

        for (int i = 0; help_lines[i] != NULL; i++) {
            g_string_append (line, help_lines[i]);
            printf ("%s\n", line->str);

            g_string_assign (line, fill->str);
        }

        g_strfreev (help_lines);
    }

    g_string_free (line, true);
    g_string_free (fill, true);

    printf ("\n");

    return TCL_OK;
}

/* Tcl command for printing license information */
static int sat_shell_command_license (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    Tcl_ArgvInfo arg_table [] = {
        TCL_ARGV_AUTO_HELP,
        TCL_ARGV_TABLE_END
    };

    int result = Tcl_ParseArgsObjv (interp, arg_table, &objc, objv, NULL);
    if (result != TCL_OK) return result;

    sat_shell_license_info (false);

    return TCL_OK;
}

/* print license info: short/long version */
void sat_shell_license_info (bool small)
{
    static const char *text_short =
        "sat-shell " VERSION_STRING "  Copyright (C) 2016  Andreas Dixius\n"
        "This program comes with ABSOLUTELY NO WARRANTY.\n"
        "This is free software, and you are welcome to redistribute it under certain conditions.\n"
        "Type \"license\" for more details, type \"help\" for help.\n"
        "\n";

    static const char *text_long =
        "sat-shell is an interactive tcl-shell for solving satisfiability problems.\n"
        "Copyright (C) 2016  Andreas Dixius\n"
        "\n"
        "This program is free software: you can redistribute it and/or modify\n"
        "it under the terms of the GNU General Public License as published by\n"
        "the Free Software Foundation, either version 3 of the License, or\n"
        "(at your option) any later version.\n"
        "\n"
        "This program is distributed in the hope that it will be useful,\n"
        "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
        "GNU General Public License for more details.\n"
        "\n"
        "You should have received a copy of the GNU General Public License\n"
        "along with this program.  If not, see <http://www.gnu.org/licenses/>.\n"
        "\n";

    if (small) {
        printf ("%s", text_short);
    } else {
        printf ("%s", text_long);
    }
}
