#ifndef PROTOS_H_
#define PROTOS_H_

#include "defines.h"

/* main.c */

void RAM (SpinnerMFSKTest)(void);
void RAM (SpinnerSweepTest)(void);
void RAM (SpinnerRTTYTest)(void);
void RAM (SpinnerMilliHertzTest)(void);
void RAM (SpinnerWide4FSKTest)(void);
void RAM (SpinnerGPSreferenceTest)(void);

void core1_entry();



/* conswrapper.c */

void ConsoleCommandsWrapper(char *cmd, int narg, char *params);
void PushErrorMessage(int id);
void PushStatusMessage(void);

#endif
