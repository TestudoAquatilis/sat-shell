#include "sat_shell.h"
#include "sat_problem.h"

#include <tclln.h>
#include <tcl.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>

struct sat_shell {
    TclLN tclln;
    SatProblem sat;
};

static int sat_shell_command_add_clause      (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static int sat_shell_command_add_encoding    (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static int sat_shell_command_add_formula     (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static int sat_shell_command_solve           (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static int sat_shell_command_reset           (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static int sat_shell_command_get_var_result  (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static int sat_shell_command_get_var_mapping (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static int sat_shell_command_get_clauses     (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);

struct sat_shell * sat_shell_new ()
{
    struct sat_shell *result = (struct sat_shell *) malloc (sizeof (struct sat_shell));

    if (result == NULL) return NULL;

    result->tclln = tclln_new ("sat-shell");
    if (result->tclln == NULL) {
        sat_shell_free (&result);
        return NULL;
    }

    tclln_provide_completion_command (result->tclln, NULL);

    tclln_add_command (result->tclln, "add_clause",      (const char * const []) {"-clause", "-list", "-help", NULL}, sat_shell_command_add_clause,      (ClientData) result, NULL);
    tclln_add_command (result->tclln, "add_encoding",    (const char * const []) {"-literals", "-encoding", NULL},    sat_shell_command_add_encoding,    (ClientData) result, NULL);
    tclln_add_command (result->tclln, "add_formula",     (const char * const []) {"-formula", "-mapping", NULL},      sat_shell_command_add_formula,     (ClientData) result, NULL);
    tclln_add_command (result->tclln, "solve",
            (const char * const []) {"-tempfile_keep", "-tempfile_clean", "-tempfile_base", "-compress_cnf", "-plain_cnf", "-solver_binary", "-solution_on_stdout", "-help", NULL},
            sat_shell_command_solve, (ClientData) result, NULL);
    tclln_add_command (result->tclln, "reset",           (const char * const []) {"-help", NULL},                     sat_shell_command_reset,           (ClientData) result, NULL);
    tclln_add_command (result->tclln, "get_var_result",  (const char * const []) {"-var", "-help", NULL},             sat_shell_command_get_var_result,  (ClientData) result, NULL);
    tclln_add_command (result->tclln, "get_var_mapping", (const char * const []) {"-name", "-number", "-help", NULL}, sat_shell_command_get_var_mapping, (ClientData) result, NULL);
    tclln_add_command (result->tclln, "get_clauses",     (const char * const []) {"-help", NULL},                     sat_shell_command_get_clauses,     (ClientData) result, NULL);

    tclln_set_prompt  (result->tclln, "sat-shell> ",
                                      "         : ");

    result->sat = sat_problem_new ();
    if (result->sat == NULL) {
        sat_shell_free (&result);
        return NULL;
    }

    return result;
}

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

    free (s);
    *sat = NULL;
}

void sat_shell_run_shell (struct sat_shell *sat)
{
    if (sat == NULL) return;
    if (sat->tclln == NULL) return;

    tclln_run (sat->tclln);
}

void sat_shell_run_script (struct sat_shell *sat, const char *script)
{
    if (sat == NULL) return;
    if (sat->tclln == NULL) return;

    tclln_run_file (sat->tclln, script, false);
}

static int sat_shell_tcl_string_list_parse (ClientData client_data, Tcl_Obj *obj, void *dest_ptr)
{
    GList **list_dest = (GList **) dest_ptr;
    if (dest_ptr == NULL) {
        return 1;
    }

    GList *result = NULL;

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
            g_list_free (result);
            return -1;
        }
        if (lit == NULL) {
            g_list_free (result);
            return -1;
        }
        char *lit_str = Tcl_GetString (lit);

        result = g_list_prepend (result, lit_str);
    }

    *list_dest = g_list_reverse(result);
    return 1;
}

static int sat_shell_tcl_string_list_list_parse (ClientData client_data, Tcl_Obj *obj, void *dest_ptr)
{
    GList **list_dest = (GList **) dest_ptr;
    if (dest_ptr == NULL) {
        return 1;
    }

    GList *result = NULL;

    if (obj == NULL) {
        return 1;
    }

    Tcl_Obj *in_list = obj;

    int len = 0;
    int temp_result = Tcl_ListObjLength (NULL, in_list, &len);
    if (temp_result != TCL_OK) return -1;

    for (int i = 0; i < len; i++) {
        GList *result_sub = NULL;
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
                g_list_free (result_sub);
                goto string_list_list_parse_error_exit;
            }
            if (lit == NULL) {
                g_list_free (result_sub);
                goto string_list_list_parse_error_exit;
            }
            char *lit_str = Tcl_GetString (lit);

            result_sub = g_list_prepend (result_sub, lit_str);
        }
        result_sub = g_list_reverse (result_sub);
        result = g_list_prepend (result, result_sub);
    }

    *list_dest = g_list_reverse (result);
    return 1;

string_list_list_parse_error_exit:
    for (GList *li = result; li != NULL; li = li->next) {
        g_list_free (li->data);
    }
    g_list_free (result);
    return -1;
}

static int sat_shell_command_add_clause (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    GList *clause = NULL;
    GList *clist  = NULL;
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
        sat_problem_add_clause_glist (sat, clause);

        g_list_free (clause);
    }

    if (clist != NULL) {
        for (GList *li = clist; li != NULL; li = li->next) {
            sat_problem_add_clause_glist (sat, (GList *) li->data);
            g_list_free ((GList *) li->data);
        }
        g_list_free (clist);
    }

    return TCL_OK;
}

static int sat_shell_command_add_encoding (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    GList *lit_list = NULL;
    SatProblem sat = ((struct sat_shell *) client_data)->sat;
    const char *encoding = NULL;

    Tcl_ArgvInfo arg_table [] = {
        {TCL_ARGV_FUNC,     "-literals", (void*) (Tcl_ArgvFuncProc*) sat_shell_tcl_string_list_parse, (void *) &lit_list, "the list of literals to apply encoding to", NULL},
        {TCL_ARGV_STRING,   "-encoding", NULL,                                                        (void *) &encoding, "the encoding to apply: one of \"1ofn\", \"2ofn\", \"1ofn_order\"", NULL},
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
        g_list_free (lit_list);
        return TCL_ERROR;
    }

    if (strcmp (encoding, "1ofn") == 0) {
        sat_problem_add_1ofn_direct_encoding (sat, lit_list);
    } else if (strcmp (encoding, "1ofn_order") == 0) {
        sat_problem_add_1ofn_order_encoding (sat, lit_list);
    } else if (strcmp (encoding, "2ofn") == 0) {
        sat_problem_add_2ofn_direct_encoding (sat, lit_list);
    } else {
        Tcl_SetObjResult (interp, Tcl_NewStringObj ("error: encoding has to be one of \"1ofn\", \"2ofn\", \"1ofn_order\"", -1));
        g_list_free (lit_list);
        return TCL_ERROR;
    }

    g_list_free (lit_list);

    return TCL_OK;
}

static int sat_shell_command_add_formula (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    SatProblem sat = ((struct sat_shell *) client_data)->sat;

    GList *mapping_list = NULL;
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
        g_list_free (mapping_list);
        return TCL_ERROR;
    }
    if (mapping_list == NULL) {
        Tcl_SetObjResult (interp, Tcl_NewStringObj ("error: expected a list of literals for mapping", -1));
        return TCL_ERROR;
    }

    if (!sat_problem_add_formula_mapping (sat, formula, mapping_list)) {
        g_list_free (mapping_list);
        Tcl_SetObjResult (interp, Tcl_NewStringObj ("error: encoding + mapping failed", -1));
        return TCL_ERROR;
    }

    g_list_free (mapping_list);

    return TCL_OK;
}

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

    printf ("DEBUG: cleanup = %d\n", cleanup);

    int result = Tcl_ParseArgsObjv (interp, arg_table, &objc, objv, NULL);
    if (result != TCL_OK) return result;

    printf ("DEBUG: cleanup = %d\n", cleanup);

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

static int sat_shell_command_get_var_result (ClientData client_data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    SatProblem sat = ((struct sat_shell *) client_data)->sat;
    const char *var_name = NULL;

    Tcl_ArgvInfo arg_table [] = {
        {TCL_ARGV_STRING,   "-var", NULL, (void *) &var_name, "var name to look up", NULL},
        TCL_ARGV_AUTO_HELP,
        TCL_ARGV_TABLE_END
    };

    int result = Tcl_ParseArgsObjv (interp, arg_table, &objc, objv, NULL);
    if (result != TCL_OK) return result;

    bool error = false;
    bool retval = sat_problem_var_result (sat, var_name, &error);

    if (error) {
        Tcl_SetObjResult (interp, Tcl_NewStringObj ("error while lookup up variable", -1));
        return TCL_ERROR;
    }

    Tcl_SetObjResult (interp, Tcl_NewBooleanObj (retval));

    return TCL_OK;
}

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
    GList *clause_list        = sat_problem_get_clauses_mapped (sat);

    for (GList *li = clause_list; li != NULL; li = li->next) {
        long int *clause = (long int *) li->data;
        if (clause == NULL) continue;

        Tcl_Obj *tcl_clause = Tcl_NewListObj (0, NULL);
        for (int i = 0; clause[i] != 0; i++) {
            long int lit = clause[i];

            Tcl_Obj *lit_obj = NULL;
            bool exists = g_hash_table_lookup_extended (lit_to_tclobj, GINT_TO_POINTER (lit), NULL, (void *) &lit_obj);
            if (!exists) {
                if (lit < 0) {
                    const char *var_name = sat_problem_get_varname_from_number (sat, -lit);
                    g_string_printf (temp_str, "-%s", var_name);
                } else {
                    const char *var_name = sat_problem_get_varname_from_number (sat, lit);
                    g_string_printf (temp_str, "%s", var_name);
                }
                lit_obj = Tcl_NewStringObj (temp_str->str, -1);
                g_hash_table_insert (lit_to_tclobj, GINT_TO_POINTER (lit), lit_obj);
            }

            Tcl_ListObjAppendElement (interp, tcl_clause, lit_obj);
        }
        Tcl_ListObjAppendElement (interp, retval, tcl_clause);
    }

    /* cleanup and return */
    g_string_free (temp_str, true);
    g_list_free (clause_list);
    g_hash_table_destroy (lit_to_tclobj);

    Tcl_SetObjResult (interp, retval);
    return TCL_OK;
}
