#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cstring> // For memset
#include "Commands.h"

using namespace std;

void testWatchProcCommand() {
    cout << "Running tests for WatchProcCommand..." << endl;

    // Test 1: Valid PID
    pid_t valid_pid = getpid(); // Current process PID
    cout << "Test 1: Valid PID (" << valid_pid << ")" << endl;
    WatchProcCommand cmd1(("watchproc " + to_string(valid_pid)).c_str());
    cmd1.execute();

    // Test 2: Invalid PID
    cout << "Test 2: Invalid PID (99999)" << endl;
    WatchProcCommand cmd2("watchproc 99999");
    cmd2.execute();

    // Test 3: Missing Arguments
    cout << "Test 3: Missing Arguments" << endl;
    WatchProcCommand cmd3("watchproc");
    cmd3.execute();

    // Test 4: Non-Numeric PID
    cout << "Test 4: Non-Numeric PID (abc)" << endl;
    WatchProcCommand cmd4("watchproc abc");
    cmd4.execute();

    // Test 5: Process Termination During Watch
    cout << "Test 5: Process Termination During Watch" << endl;
    pid_t child_pid = fork();
    if (child_pid == 0) {
        // Child process: Sleep for 2 seconds and exit
        sleep(2);
        exit(0);
    } else {
        // Parent process: Watch the child process
        WatchProcCommand cmd5(("watchproc " + to_string(child_pid)).c_str());
        cmd5.execute();
        waitpid(child_pid, nullptr, 0); // Wait for the child to finish
    }

    // Test 6: Zombie Process
    cout << "Test 6: Zombie Process" << endl;
    pid_t zombie_pid = fork();
    if (zombie_pid == 0) {
        // Child process: Exit immediately to become a zombie
        exit(0);
    } else {
        // Parent process: Do not wait for the child, making it a zombie
        sleep(1); // Give time for the child to become a zombie
        WatchProcCommand cmd6(("watchproc " + to_string(zombie_pid)).c_str());
        cmd6.execute();
        waitpid(zombie_pid, nullptr, 0); // Clean up the zombie process
    }

    // Test 7: High CPU Usage Process
    cout << "Test 7: High CPU Usage Process" << endl;
    pid_t high_cpu_pid = fork();
    if (high_cpu_pid == 0) {
        // Child process: Consume CPU in an infinite loop
        while (true) {}
    } else {
        // Parent process: Watch the high CPU process
        sleep(1); // Allow the child to start consuming CPU
        WatchProcCommand cmd7(("watchproc " + to_string(high_cpu_pid)).c_str());
        cmd7.execute();
        kill(high_cpu_pid, SIGKILL); // Terminate the high CPU process
        waitpid(high_cpu_pid, nullptr, 0); // Clean up
    }

    // Test 8: High Memory Usage Process
    cout << "Test 8: High Memory Usage Process" << endl;
    pid_t high_memory_pid = fork();
    if (high_memory_pid == 0) {
        // Child process: Allocate a large amount of memory
        const size_t size = 500 * 1024 * 1024; // 500 MB
        void *memory = malloc(size);
        if (memory) {
            memset(memory, 0, size); // Touch the memory to ensure it's allocated
            sleep(10); // Keep the memory allocated for 10 seconds
            free(memory);
        }
        exit(0);
    } else {
        // Parent process: Watch the high memory process
        sleep(1); // Allow the child to allocate memory
        WatchProcCommand cmd8(("watchproc " + to_string(high_memory_pid)).c_str());
        cmd8.execute();
        kill(high_memory_pid, SIGKILL); // Terminate the high memory process
        waitpid(high_memory_pid, nullptr, 0); // Clean up
    }

    // Test 9: Idle Process
    cout << "Test 9: Idle Process" << endl;
    pid_t idle_pid = fork();
    if (idle_pid == 0) {
        // Child process: Sleep for a long time
        sleep(100);
        exit(0);
    } else {
        // Parent process: Watch the idle process
        sleep(1); // Allow the child to start sleeping
        WatchProcCommand cmd9(("watchproc " + to_string(idle_pid)).c_str());
        cmd9.execute();
        kill(idle_pid, SIGKILL); // Terminate the idle process
        waitpid(idle_pid, nullptr, 0); // Clean up
    }

    cout << "All tests completed." << endl;
}

int main() {
    SmallShell &smash = SmallShell::getInstance();
    testWatchProcCommand();
    return 0;
}