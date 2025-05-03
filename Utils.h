#ifndef UTILS_H
#define UTILS_H

#include <unistd.h>
#include <iostream>

void checkProcessGroup(pid_t process_pid);
void printSmashProcessGroup();
void printProcessGroup(pid_t pid);
void logSignalToProcessGroup(pid_t pgid, int signal);

#endif // UTILS_H