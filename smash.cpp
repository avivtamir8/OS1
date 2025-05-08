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
        if (!std::getline(std::cin, cmd_line)) {
            // Handle EOF or other input errors
            if (std::cin.eof()) {
                // End of file reached, exit gracefully
                break;
            } else {
                // Other getline error
                perror("smash error: getline failed");
                exit(1); // Or handle error as appropriate
            }
        }

        // If cmd_line is empty (e.g., user just pressed Enter),
        // executeCommand will handle it (likely by doing nothing or creating an EmptyCommand).
        // The loop will then print the prompt again, which is the correct behavior for an empty command.
        smash.executeCommand(cmd_line.c_str());
    }

    return 0;
}

