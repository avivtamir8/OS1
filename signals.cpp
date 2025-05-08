#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlCHandler(int sig_num) {
    cout << "smash: got ctrl-C" << endl;

    SmallShell& smash = SmallShell::getInstance();
    pid_t fg_pid = smash.getForegroundPid();

    if (fg_pid > 0) {
        // If a foreground process exists, send SIGINT to it
        if (kill(fg_pid, SIGINT) == -1) {
            perror("smash error: kill failed");
        } else {
            std::cout << "smash: process " << fg_pid << " was killed" << std::endl;
        }
        smash.clearForegroundPid();
    } 
}
