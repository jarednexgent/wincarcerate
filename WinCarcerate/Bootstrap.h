// Bootstrap.h
#ifndef BOOTSTRAP_H
#define BOOTSTRAP_H

#include <windows.h>

BOOL CheckForDebugger(void);
BOOL CheckSingleInstance(void);
VOID ConfigureProcessExecution(void);
BOOL KillNonCriticalProcesses(void);

#endif