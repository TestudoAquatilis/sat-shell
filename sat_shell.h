#ifndef __sat_shell_h__
#define __sat_shell_h__

typedef struct sat_shell *SatShell;

SatShell sat_shell_new ();
void sat_shell_free (SatShell *sat);

void sat_shell_run_shell (SatShell sat);
void sat_shell_run_script (SatShell sat, const char *script);

#endif
