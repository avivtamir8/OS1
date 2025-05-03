#include "Utils.h"
#include <iostream>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

// Prints the process group ID of the smash shell
void printSmashProcessGroup() {
    pid_t smash_pgid = getpgrp(); // Get the process group ID of the smash shell
    cout << "Smash process group ID: " << smash_pgid << endl;
}

// Prints the process group ID of a given process
void printProcessGroup(pid_t pid) {
    pid_t process_pgid = getpgid(pid); // Get the process group ID of the given process
    if (process_pgid == -1) {
        perror("smash error: getpgid failed");
        return;
    }
    cout << "Process " << pid << " belongs to process group " << process_pgid << endl;
}

// Logs the process group to which a signal is sent
void logSignalToProcessGroup(pid_t pgid, int signal) {
    cout << "Sending signal " << signal << " to process group " << pgid << endl;
}

void checkProcessGroup(pid_t process_pid) {
    pid_t smash_pgid = getpgrp(); // Get the process group ID of the smash shell
    pid_t process_pgid = getpgid(process_pid); // Get the process group ID of the process

    if (process_pgid == -1) {
        perror("smash error: getpgid failed");
        return;
    }

    if (smash_pgid == process_pgid) {
        cout << "smash and the process are in the same process group." << endl;
    } else {
        cout << "smash and the process are in different process groups." << endl;
    }
}