#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"

int main(int argc, char *argv[]) {
    if (signal(SIGINT, ctrlCHandler) == SIG_ERR) {
        perror("smash error: failed to set ctrl-C handler");
    }


    SmallShell &smash = SmallShell::getInstance();
    while (true) {
        std::cout << smash.getPrompt() << "> ";
        std::string cmd_line;
        std::getline(std::cin, cmd_line);
        smash.executeCommand(cmd_line.c_str());
    }

    // // Create a JobsList instance
    // JobsList jobsList;

    // // Add a job with a fake PID (12345)
    // std::cout << "Adding job with PID 12345..." << std::endl;
    // jobsList.addJob("sleep 10", 12345, false);

    // // Add another job with a fake PID (12346)
    // std::cout << "Adding job with PID 12346..." << std::endl;
    // jobsList.addJob("sleep 20", 12346, false);

    // // Print the jobs list
    // std::cout << "Jobs list before removing finished jobs:" << std::endl;
    // jobsList.printJobsList();

    // // Simulate removing finished jobs
    // std::cout << "Removing finished jobs..." << std::endl;
    // jobsList.removeFinishedJobs();

    // // Print the jobs list again
    // std::cout << "Jobs list after removing finished jobs:" << std::endl;
    // jobsList.printJobsList();

    // // Add more jobs and test further
    // std::cout << "Adding job with PID 12347..." << std::endl;
    // jobsList.addJob("sleep 30", 12347, false);

    // std::cout << "Jobs list after adding another job:" << std::endl;
    // jobsList.printJobsList();

    // // Simulate killing all jobs
    // std::cout << "Killing all jobs..." << std::endl;
    // jobsList.killAllJobs();

    // // Print the jobs list after killing all jobs
    // std::cout << "Jobs list after killing all jobs:" << std::endl;
    // jobsList.printJobsList();


    return 0;
}

