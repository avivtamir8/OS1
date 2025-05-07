#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <sys/stat.h>
#include <iomanip>
#include <regex>
#include <string>
#include <set>
#include <map>
#include <fstream>
#include <fcntl.h>
#include "Commands.h"
#include <iterator>
#include <time.h> 
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>
#include <sys/syscall.h>
#include <math.h>

using namespace std;

// Define linux_dirent64 structure manually
struct linux_dirent64 {
    ino64_t d_ino;          // Inode number
    off64_t d_off;          // Offset to the next dirent
    unsigned short d_reclen; // Length of this record
    unsigned char d_type;   // File type
    char d_name[];          // Filename (null-terminated)
};

const string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  cout << __PRETTY_FUNCTION__ << " --> " << endl;
#define FUNC_EXIT()   cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

// Utility functions for string trimming
string _ltrim(const string &s) {
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == string::npos) ? "" : s.substr(start);
}

string _rtrim(const string &s) {
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const string &s) {
  return _rtrim(_ltrim(s));
}

// Command line parsing
int _parseCommandLine(const char *cmd_line, char **args) {
  FUNC_ENTRY()
  int i = 0;
  istringstream iss(_trim(string(cmd_line)).c_str());
  for (string s; iss >> s;) {
    args[i] = (char *)malloc(s.length() + 1);
    memset(args[i], 0, s.length() + 1);
    strcpy(args[i], s.c_str());
    args[++i] = NULL;
  }
  FUNC_EXIT()
  return i;
}

// Background command utilities
bool _isBackgroundComamnd(const char *cmd_line) {
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
  const string str(cmd_line);
  unsigned int idx = str.find_last_not_of(WHITESPACE);
  if (idx == string::npos || cmd_line[idx] != '&') {
    return;
  }
  cmd_line[idx] = ' ';
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

SmallShell::SmallShell() 
  : foreground_pid(-1), is_foreground_running(false), prompt("smash"),
    lastWorkingDir(""), prevWorkingDir("")
{}



Command *SmallShell::CreateCommand(const char *cmd_line) {
    string cmd_s = _trim(string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

    // Handle alias expansion
    auto aliasIt = aliasMap.find(firstWord);
    if (aliasIt != aliasMap.end()) {
        string expandedCommand = aliasIt->second;

        // Append remaining arguments
        string remainingArgs = cmd_s.substr(cmd_s.find_first_of(" \n") + 1);
        if (!remainingArgs.empty()) {
            expandedCommand += " " + _trim(remainingArgs);
        }

        // Re-assign the expanded command to cmd_s
        cmd_s = _trim(expandedCommand);
        firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
    }

    if (cmd_s.find(">") != string::npos || cmd_s.find(">>") != string::npos) {
        return new RedirectionCommand(cmd_s.c_str());
    } else if (cmd_s.find("|") != string::npos || cmd_s.find("|&") != string::npos) {
      return new PipeCommand(cmd_s.c_str());
    } else if (firstWord == "chprompt") {
        return new ChPromptCommand(cmd_s.c_str());
    } else if (firstWord == "showpid") {
        return new ShowPidCommand(cmd_s.c_str());
    } else if (firstWord == "pwd") {
        return new GetCurrDirCommand(cmd_s.c_str());
    } else if (firstWord == "cd") {
        return new ChangeDirCommand(cmd_s.c_str());
    } else if (firstWord == "jobs") {
        return new JobsCommand(cmd_s.c_str(), jobs);
    } else if (firstWord == "alias") {
        return new AliasCommand(cmd_s.c_str(), aliasMap);
    } else if (firstWord == "unalias") {
        return new UnAliasCommand(cmd_s.c_str(), aliasMap);
    } else if (firstWord == "kill") {
        return new KillCommand(cmd_s.c_str(), jobs);
    } else if (firstWord == "quit") {
        return new QuitCommand(cmd_s.c_str(), jobs); 
    } else if (firstWord == "fg") {
        return new ForegroundCommand(cmd_s.c_str(), jobs);
    } else if (firstWord == "unsetenv") {
        return new UnSetEnvCommand(cmd_s.c_str());
    } else if (firstWord == "watchproc") {
        return new WatchProcCommand(cmd_s.c_str());
    }
    else if (firstWord == "whoami") {
      return new WhoAmICommand(cmd_s.c_str());
    } 
    else {
        if(cmd_s.find('?') != string::npos || cmd_s.find('*') != string::npos) {
          return new ComplexExternalCommand(cmd_s.c_str(), jobs);
        }
        else {
          return new SimpleExternalCommand(cmd_s.c_str(), jobs);
        }
    }
}

void SmallShell::executeCommand(const char *cmd_line) {
  // TODO: Add your implementation here
  // for example:
  // Command* cmd = CreateCommand(cmd_line);
  // cmd->execute();
  // Please note that you must fork smash process for some commands (e.g., external commands....)

  Command *cmd = CreateCommand(cmd_line);
  cmd->execute();
  delete cmd;
}

string SmallShell::getLastDir() const {
  return lastWorkingDir;
}

void SmallShell::setLastDir(const string &dir) {
  lastWorkingDir = dir;
}

void SmallShell::setForegroundPid(pid_t pid) {
  foreground_pid = pid;
  is_foreground_running = true;
}

pid_t SmallShell::getForegroundPid() const {
  return is_foreground_running ? foreground_pid : -1; // Return -1 if no foreground process
}

void SmallShell::clearForegroundPid() {
  foreground_pid = -1;
  is_foreground_running = false;
}

// Command class implementation
Command::Command(const char *cmd_line_input) 
  : cmd_line(_trim(string(cmd_line_input))), is_background(false), alias("") {
  // Determine if the command is a background command
  is_background = _isBackgroundComamnd(cmd_line.c_str());

  // Parse the command line into arguments
  char *raw_args[COMMAND_MAX_ARGS + 1];
  int argsCount = _parseCommandLine(this->cmd_line.c_str(), raw_args);

  // Store the parsed arguments in the args vector
  for (int i = 0; i < argsCount; ++i) {
      args.push_back(string(raw_args[i]));
      free(raw_args[i]); // Free the allocated memory for each argument
  }
}

string Command::getAlias() const { return alias; }

void Command::setAlias(const string& aliasCommand) { alias = aliasCommand; }

// BuiltInCommand class implementations
void ChPromptCommand::execute() {
  if (args.size() > 1) {
    SmallShell::getInstance().setPrompt(args[1]);
  } else {
    SmallShell::getInstance().setPrompt("smash");
  }
}

void ShowPidCommand::execute() {
  cout << "smash pid is " << getpid() << endl;
}

void GetCurrDirCommand::execute() {
  char cwd[PATH_MAX];
  if (getcwd(cwd, sizeof(cwd)) != nullptr) {
    cout << cwd << endl;
  } else {
    perror("getcwd() error");
  }
}

void ChangeDirCommand::execute() {
  SmallShell &shell = SmallShell::getInstance();

  if (args.size() == 1) {
    return;
  }

  if (args.size() > 2) {
    cerr << "smash error: cd: too many arguments" << endl;
    return;
  }

  const string &targetDir = args[1];
  char *currentDir = getcwd(nullptr, 0);
  if (!currentDir) {
    perror("smash error: getcwd failed");
    return;
  }

  if (targetDir == "-") {
    if (shell.getLastDir().empty()) {
      cerr << "smash error: cd: OLDPWD not set" << endl;
      free(currentDir);
      return;
    }
    if (chdir(shell.getLastDir().c_str()) == -1) {
      perror("smash error: chdir failed");
    } else {
      shell.setLastDir(currentDir);
    }
    free(currentDir);
    return;
  }

  if (targetDir == "..") {
    if (chdir("..") == -1) {
      perror("smash error: chdir failed");
    } else {
      shell.setLastDir(currentDir);
    }
    free(currentDir);
    return;
  }

  if (chdir(targetDir.c_str()) == -1) {
    perror("smash error: chdir failed");
  } else {
    shell.setLastDir(currentDir);
  }
  free(currentDir);
}

void ForegroundCommand::execute() {
  jobs.removeFinishedJobs();

  if(args.size() > 2) {
    cerr << "smash error: fg: invalid arguments" << endl;
    return;
  }

  if(jobs.getJobById(jobId) == nullptr) {
    cerr << "smash error: fg: job-id " << jobId << " does not exist" << endl;
    return;
  }
  if (args.size() == 1 && jobs.isEmpty()) {
    cerr << "smash error: fg: jobs list is empty" << endl;
    return;    
  }
  
  // Print the command line and PID
  cout << jobs.getJobById(jobId)->getCmdLine() << " : " << jobs.getJobById(jobId)->getPid() << endl;

  // Set the foreground PID in SmallShell
  SmallShell &smash = SmallShell::getInstance();
  smash.setForegroundPid(jobs.getJobById(jobId)->getPid());

  // Wait for the job's process to finish
  int status;
  if(waitpid(jobs.getJobById(jobId)->getPid(), &status, WUNTRACED) == -1) {
    perror("smash error: waitpid failed");
    return;
  }

  if(WIFEXITED(status)) {
    jobs.removeJobById(jobId);
    smash.clearForegroundPid();
  }
  return;
}

// AliasCommand Class

void AliasCommand::execute() {
    // Trim the command line to handle any spaces
    string commandLine = _trim(cmd_line); //TODO: no need the command line is already trimmed of spaces. how does this deal with &

    // Case 1: If the command is exactly "alias" with no arguments
    if (commandLine == "alias") {
        // Print all aliases in the map
        for (const auto& alias : aliasMap) {
            cout << alias.first << "='" << alias.second << "'" << endl;
        }
        return; // Exit after printing
    }

    // Case 2: Handle alias creation (alias <name>='<command>')
    size_t equalPos = commandLine.find('=');
    if (equalPos == string::npos || equalPos < 6) { // Missing '=' or alias name
        cerr << "smash error: alias: invalid alias format" << endl;
        return;
    }

    // Extract the alias name and command
    string aliasName = _trim(commandLine.substr(6, equalPos - 6));
    string aliasCommand = _trim(commandLine.substr(equalPos + 1));

    // Validate alias name format
    if (!regex_match(aliasName, regex("^[a-zA-Z0-9_]+$"))) {
        cerr << "smash error: alias: invalid alias format" << endl;
        return;
    }

    // Check for proper quotes around the alias command
    if (aliasCommand.length() < 2 || aliasCommand.front() != '\'' || aliasCommand.back() != '\'') {
        cerr << "smash error: alias: invalid alias format" << endl;
        return;
    }

    // Remove surrounding quotes from the command
    aliasCommand = aliasCommand.substr(1, aliasCommand.length() - 2);

    // Validate that the command exists in the system's PATH
    string commandToCheck = aliasCommand.substr(0, aliasCommand.find(' ')); // Extract the base command
    if (system(("command -v " + commandToCheck + " > /dev/null 2>&1").c_str()) != 0) {
        cerr << "smash error: alias: command '" << commandToCheck << "' not found" << endl;
        return;
    }

    // Check for reserved keywords
    static const set<string> reservedKeywords = {
        "quit", "fg", "bg", "jobs", "kill", "cd", "listdir", "chprompt", "alias", "unalias", "pwd", "showpid"
    };

    if (reservedKeywords.count(aliasName) || aliasMap.count(aliasName)) {
        cerr << "smash error: alias: " << aliasName << " already exists or is a reserved command" << endl;
        return;
    }

    // Add the alias to the map
    aliasMap[aliasName] = aliasCommand;
}

void SmallShell::setAlias(const string& aliasName, const string& aliasCommand) {
	if (aliasCommand == aliasName) {
		cerr << "smash error: alias: alias loop detected" << endl;
		return;
	}
	aliasMap[aliasName] = aliasCommand;
}
void SmallShell::removeAlias(const string& aliasName) {
	if (aliasMap.erase(aliasName) == 0) {
		cerr << "smash error: unalias: alias \"" << aliasName << "\" does not exist" << endl;
	}
}
string SmallShell::getAlias(const string& aliasName) const {
	auto it = aliasMap.find(aliasName);
	if (it != aliasMap.end()) {
		return it->second;
	}
	return ""; // Alias not found
}
void SmallShell::printAliases() const {
	for (const auto& alias : aliasMap) {
		cout << alias.first << " -> " << alias.second << endl;
	}
}

void JobsCommand::execute() {
  jobs.removeFinishedJobs();
  jobs.printJobsList();
};

void SimpleExternalCommand::execute() {
  /*
   * Executes an external command.
   * Handles both foreground and background processes.
   * Separates the child process into its own process group.
   */

  pid_t pid = fork();
  if (pid < 0) {
    perror("smash error: fork failed");
    return;
  }

  if (pid == 0) {
    // Child process
    setpgrp(); 

    string cmd_line_copy = cmd_line;
    if (is_background) {
      _removeBackgroundSign(&cmd_line_copy[0]);
    }

    char *args[COMMAND_MAX_ARGS + 1];
    int argsCount = _parseCommandLine(cmd_line_copy.c_str(), args);
    args[argsCount] = nullptr;

    if (execvp(args[0], args) == -1) {
      perror("smash error: execvp failed");

      // Free allocated memory before exiting
      for (int i = 0; i < argsCount; ++i) {
        free(args[i]);
      }
      exit(1);
    }

    // Free allocated memory (in case execvp fails unexpectedly)
    for (int i = 0; i < argsCount; ++i) {
      free(args[i]);
    }
    exit(0);
  } else {
    // Parent process
    SmallShell &smash = SmallShell::getInstance();

    if (!is_background) {
      // Foreground execution: Wait for the child process to finish
      smash.setForegroundPid(pid);
      int status;
      if (waitpid(pid, &status, 0) == -1) {
        perror("smash error: waitpid failed");
      }
      smash.clearForegroundPid(); // Clear the foreground PID after the process finishes
    } else {
      // Background execution: Add the job to the jobs list
      jobs.addJob(cmd_line, pid, false);
    }
  }
}

void ComplexExternalCommand::execute() {
  pid_t pid = fork();
  if (pid < 0) {
    perror("smash error: fork failed");
    return;
  }

  if (pid == 0) {
    // Child process
    setpgrp();

    string cmd_line_copy = cmd_line;
    if (is_background) {
      _removeBackgroundSign(&cmd_line_copy[0]);
    }

    const char *args[] = {"/bin/bash", "-c", cmd_line_copy.c_str(), nullptr};
    if (execvp(args[0], const_cast<char *const *>(args)) == -1) {
      perror("smash error: execvp failed");
      exit(1);
    }
  } else {
    // Parent process
    SmallShell &smash = SmallShell::getInstance();

    if (!is_background) {
      // Foreground execution: Wait for the child process to finish
      smash.setForegroundPid(pid);
      int status;
      if (waitpid(pid, &status, WUNTRACED) == -1) {
        perror("smash error: waitpid failed");
      }
      smash.clearForegroundPid(); // Clear the foreground PID after the process finishes
    } else {
      // Background execution: Add the job to the jobs list
      jobs.addJob(cmd_line, pid, false);
    }
  }

  return;
}


// UnAliasCommand Class
void UnAliasCommand::execute() {
    // Check if arguments are provided
    if (args.size() < 2) {
        cerr << "smash error: unalias: not enough arguments" << endl;
        return;
    }

    // Iterate through the provided alias names
    for (size_t i = 1; i < args.size(); ++i) {
        const string& aliasName = args[i];

        // Check if the alias exists
        if (aliasMap.find(aliasName) == aliasMap.end()) {
            cerr << "smash error: unalias: " << aliasName << " alias does not exist" << endl;
            return;
        }

        // Remove the alias from the map
        aliasMap.erase(aliasName);
    }
}

// KillCommand Class
void KillCommand::execute() {
    // Validate the number of arguments
    if (args.size() != 3 || args[1][0] != '-') {
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }

    // Parse the signal number
    int signum;
    try {
        signum = stoi(args[1].substr(1)); // Remove the '-' and convert to integer
    } catch (const invalid_argument &e) {
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }

    // Parse the job ID
    int jobId;
    try {
        jobId = stoi(args[2]);
    } catch (const invalid_argument &e) {
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }

    // Find the job by ID
    JobsList::JobEntry *job = jobs.getJobById(jobId);
    if (!job) {
        cerr << "smash error: kill: job-id " << jobId << " does not exist" << endl;
        return;
    }

    // Send the signal to the job's process
    if (kill(job->getPid(), signum) == -1) {
        perror("smash error: kill failed");
        return;
    }

    // Print success message
    cout << "signal number " << signum << " was sent to pid " << job->getPid() << endl;
}

// QuitCommand Class
void QuitCommand::execute() {
    // Check if the "kill" argument is provided
    bool killFlag = (args.size() > 1 && args[1] == "kill");

    if (killFlag) {
        // Remove finished jobs before killing
        jobs.removeFinishedJobs();

        // Get the list of remaining jobs
        vector<JobsList::JobEntry*> remainingJobs = jobs.getJobs();

        // Print the number of jobs to be killed
        cout << "smash: sending SIGKILL signal to " << remainingJobs.size() << " jobs:" << endl;

        // Iterate through the jobs and send SIGKILL
        for (JobsList::JobEntry* job : remainingJobs) {
            cout << job->getPid() << ": " << job->getCmdLine() << endl;
            if (kill(job->getPid(), SIGKILL) == -1) {
                perror("smash error: kill failed");
            }
        }

        // Clear the jobs list
        jobs.clearJobs();
    }

    // Exit the shell
    exit(0);
}

void UnSetEnvCommand::execute() {
    // Check if arguments are provided
    if (args.size() < 2) {
        cerr << "smash error: unsetenv: not enough arguments" << endl;
        return;
    }

    // Iterate through the provided environment variable names
    for (size_t i = 1; i < args.size(); ++i) {
        const string &varName = args[i];

        // Check if the environment variable exists
        if (getenv(varName.c_str()) == nullptr) {
            cerr << "smash error: unsetenv: " << varName << " does not exist" << endl;
            return;
        }

        // Remove the environment variable using unsetenv()
        if (unsetenv(varName.c_str()) != 0) {
            perror("smash error: unsetenv failed");
            return;
        }
    }
}


void WatchProcCommand::execute() {
    if (args.size() != 2) {
        cerr << "smash error: watchproc: invalid arguments" << endl;
        return;
    }

    // Parse the PID
    pid_t pid;
    try {
        pid = stoi(args[1]);
    } catch (const invalid_argument &e) {
        cerr << "smash error: watchproc: invalid arguments" << endl;
        return;
    }

    // Check if the process exists
    string proc_path = "/proc/" + to_string(pid) + "/stat";
    int fd = open(proc_path.c_str(), O_RDONLY);
    if (fd == -1) {
        cerr << "smash error: watchproc: pid " << pid << " does not exist" << endl;
        return;
    }
    close(fd);

    // Read initial CPU and system times
    long long prev_proc_time = 0, prev_total_time = 0;
    if (!readCpuTimes(pid, prev_proc_time, prev_total_time)) {
        cerr << "smash error: watchproc: failed to read process or system CPU times" << endl;
        return;
    }

    // Wait for 1 second to calculate deltas
    struct timespec req = {1, 0}; // 1 second, 0 nanoseconds
    nanosleep(&req, nullptr);

    // Read current CPU and system times
    long long curr_proc_time = 0, curr_total_time = 0;
    if (!readCpuTimes(pid, curr_proc_time, curr_total_time)) {
        cerr << "smash error: watchproc: pid " << pid << " does not exist" << endl;
        return;
    }

    // Calculate CPU usage
    double cpu_usage = 100.0 * (curr_proc_time - prev_proc_time) / (curr_total_time - prev_total_time);

    // Read memory usage
    double memory_usage = readMemoryUsage(pid);

    // Display the results
    cout << "PID: " << pid
         << " | CPU Usage: " << fixed << setprecision(1) << cpu_usage << "%"
         << " | Memory Usage: " << fixed << setprecision(1) << memory_usage << " MB" << endl;
}

bool WatchProcCommand::readCpuTimes(pid_t pid, long long &proc_time, long long &total_time) {
    // Read process CPU time from /proc/<pid>/stat
    string proc_stat_path = "/proc/" + to_string(pid) + "/stat";
    int fd = open(proc_stat_path.c_str(), O_RDONLY);
    if (fd == -1) {
        return false;
    }

    char buffer[1024]; // accourding to the internet this size for the buffer should be enough. its usually aroun 0.5KB
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);

    if (bytes_read <= 0) {
        return false;
    }
    buffer[bytes_read] = '\0'; // Null-terminate the buffer

    istringstream iss(buffer);
    vector<string> tokens((istream_iterator<string>(iss)), istream_iterator<string>());

    if (tokens.size() < 17) {
        return false;
    }

    long utime = stoll(tokens[13]); // User mode time
    long stime = stoll(tokens[14]); // Kernel mode time
    proc_time = utime + stime;

    // Read total CPU time from /proc/stat
    fd = open("/proc/stat", O_RDONLY);
    if (fd == -1) {
        return false;
    }

    bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);

    if (bytes_read <= 0) {
        return false;
    }
    buffer[bytes_read] = '\0'; // Null-terminate the buffer

    iss.clear();
    iss.str(buffer);
    string line;
    getline(iss, line); // Read the first line (CPU stats)

    istringstream stat_iss(line);
    string cpu_label;
    stat_iss >> cpu_label; // Skip "cpu"

    long long user, nice, system, idle, iowait, irq, softirq, steal;
    stat_iss >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

    total_time = user + nice + system + idle + iowait + irq + softirq + steal;

    return true;
}

double WatchProcCommand::readMemoryUsage(pid_t pid) {
    // Read memory usage from /proc/<pid>/status
    string proc_status_path = "/proc/" + to_string(pid) + "/status";
    int fd = open(proc_status_path.c_str(), O_RDONLY);
    if (fd == -1) {
        return 0.0;
    }

    char buffer[4096];
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);

    if (bytes_read <= 0) {
        return 0.0;
    }
    buffer[bytes_read] = '\0'; // Null-terminate the buffer

    istringstream iss(buffer);
    string line;
    while (getline(iss, line)) {
        if (line.find("VmRSS:") == 0) { // Look for the "VmRSS" field. should be the first one
            istringstream line_iss(line);
            string key;
            long memory_kb;
            string unit;
            line_iss >> key >> memory_kb >> unit;
            return memory_kb / 1024.0; // Convert KB to MB
        }
    }

    return 0.0;
}

void RedirectionCommand::execute() {
  string command_to_run, output_file;
  bool append_mode = false;
  size_t redirect_pos;

  string cmd_line_copy = cmd_line;
  _removeBackgroundSign(&cmd_line_copy[0]);

  if ((redirect_pos = cmd_line_copy.find(">>")) != string::npos) {
    append_mode = true;
    command_to_run = _trim(cmd_line_copy.substr(0, redirect_pos));
    output_file = _trim(cmd_line_copy.substr(redirect_pos + 2));
  } else if ((redirect_pos = cmd_line_copy.find('>')) != string::npos) {
    append_mode = false;
    command_to_run = _trim(cmd_line_copy.substr(0, redirect_pos));
    output_file = _trim(cmd_line_copy.substr(redirect_pos + 1));
  } else {
    cerr << "smash error: redirection: invalid format" << endl;
    return;
  }

  if (command_to_run.empty() || output_file.empty()) {
    cerr << "smash error: redirection: invalid format" << endl;
    return;
  }


  int original_stdout_fd = dup(STDOUT_FILENO);
  if (original_stdout_fd == -1) {
    perror("smash error: dup failed");
    return;
  }
  int flags = O_CREAT | O_WRONLY;
  flags |= (append_mode) ? O_APPEND : O_TRUNC;
  int file_fd = open(output_file.c_str(), flags, 0666);
  if (file_fd == -1) {
    perror("smash error: open failed");
    close(original_stdout_fd);
    return;
  }
  if(dup2(file_fd, STDOUT_FILENO) == -1) {
    perror("smash error: dup2 failed");
    close(original_stdout_fd);
    close(file_fd);
    return;
  }
  if(close(file_fd) == -1) {
    perror("smash error: close failed");
    dup2(original_stdout_fd, STDOUT_FILENO); //automatically close what was in STDOUT_FILENO and restore to original
    close(original_stdout_fd);
    return;
  }
  fflush(stdout);
  SmallShell &smash = SmallShell::getInstance();
  smash.executeCommand(command_to_run.c_str());
  fflush(stdout); // Ensure all buffered output is written to the file
  if(dup2(original_stdout_fd, STDOUT_FILENO) == -1) {
    perror("smash error: dup2 failed to restore stdout");
    close(original_stdout_fd);
    return;
  }
  if(close(original_stdout_fd) == -1) {
    perror("smash error: close failed for original stdout fd");
  }
}

void PipeCommand::execute() {
  string command_1, command_2;
  bool error_mode = false; // true for |& (stderr), false for | (stdout)
  size_t pipe_pos;

  string cmd_line_copy = cmd_line;
  _removeBackgroundSign(&cmd_line_copy[0]); 

  if ((pipe_pos = cmd_line_copy.find("|&")) != string::npos) {
    error_mode = true;
    command_1 = _trim(cmd_line_copy.substr(0, pipe_pos));
    command_2 = _trim(cmd_line_copy.substr(pipe_pos + 2));
  } else if ((pipe_pos = cmd_line_copy.find('|')) != string::npos) {
    error_mode = false;
    command_1 = _trim(cmd_line_copy.substr(0, pipe_pos));
    command_2 = _trim(cmd_line_copy.substr(pipe_pos + 1));
  } else {
    // This case should ideally be caught by CreateCommand logic
    cerr << "smash error: pipe: invalid format" << endl;
    return;
  }

  if (command_1.empty() || command_2.empty()) {
    cerr << "smash error: pipe: invalid format" << endl;
    return;
  }

  int pipe_fd[2];
  if (pipe(pipe_fd) == -1) {
    perror("smash error: pipe failed");
    return;
  }

  SmallShell &smash = SmallShell::getInstance();
  pid_t pid1 = fork();
  if (pid1 == -1) {
    perror("smash error: fork failed");
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    return;
  }
  if (pid1 == 0) {
    // Child 1: Executes command_1
    setpgrp(); 
    int target_fd_for_cmd1_output = error_mode ? STDERR_FILENO : STDOUT_FILENO;
    if (dup2(pipe_fd[1], target_fd_for_cmd1_output) == -1) {
      perror("smash error: dup2 failed for command1 output");
      close(pipe_fd[0]);
      close(pipe_fd[1]);
      exit(1);
    }
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    smash.executeCommand(command_1.c_str());
    exit(0);
  }

  // Parent process continues to fork the second child
  pid_t pid2 = fork();
  if (pid2 == -1) {
    perror("smash error: fork failed for command2");
    // Clean up first child if it was successfully forked
    kill(pid1, SIGINT);
    waitpid(pid1, nullptr, 0);
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    return;
  }
  if (pid2 == 0) {
    // Child 2: Executes command_2
    setpgrp();
    if (dup2(pipe_fd[0], STDIN_FILENO) == -1) {
      perror("smash error: dup2 failed for command2 input");
      close(pipe_fd[0]);
      close(pipe_fd[1]);
      exit(1);
    }
    close(pipe_fd[1]);
    close(pipe_fd[0]);
    smash.executeCommand(command_2.c_str());
    exit(0);
  }

  // Parent process:
  close(pipe_fd[0]);
  close(pipe_fd[1]);  
  int status1, status2;
  if (waitpid(pid1, &status1, 0) == -1) {
    perror("smash error: waitpid failed for command1");
  }
  if (waitpid(pid2, &status2, 0) == -1) {
    perror("smash error: waitpid failed for command2");
  }
}// WhoamiCommand Class


void WhoAmICommand::execute() {
  char* username = getenv("USER"); // Retrieve the USER environment variable
	char* homeDir = getenv("HOME");  // Retrieve the HOME environment variable

	if (username && homeDir) {
		std::cout << username << " " << homeDir << std::endl;
	}
	else {
		std::cerr << "smash error: whoami: failed to retrieve user information" << std::endl;
	}
}


void DiskUsageCommand::execute() {
  // Check if too many arguments are provided
  if (args.size() > 2) {
    cerr << "smash error: du: too many arguments" << endl;
    return;
  }

  const char* path = (args.size() == 1) ? "." : args[1].c_str();
  struct stat statbuf;
  if (stat(path, &statbuf) == -1 || !S_ISDIR(statbuf.st_mode)) {
    cerr << "smash error: du: directory " << path << " does not exist" << endl;
    return;
  }

  long long total_usage_kb = calculateDiskUsage(path);
  cout << "Total disk usage: " << total_usage_kb << " KB" << endl;
}

long long DiskUsageCommand::calculateDiskUsage(const char* path) {
  /*
  * we can use stat to get the size of file/dir
  * but we need another way access the internal files / dirs
  * we want to use opendir, readdir but were not allowed :(
  * thats why well read this data "by hand" */
  
  long long total_size_kb = 0;

  int dir_fd = open(path, O_RDONLY | O_DIRECTORY);
  if (dir_fd == -1) {
    return 0;
  }

  char buffer[4096];
  struct linux_dirent64 *d_entry;
  struct stat entry_statbuf;
  int bytes_read;

  while ((bytes_read = syscall(SYS_getdents64, dir_fd, buffer, sizeof(buffer))) > 0) { // read batch of entries
    for (int offset = 0; offset < bytes_read;) { // iterate through the entries
      
      char* entry_ptr = buffer + offset; // Calculate the pointer to the current entry
      d_entry = (struct linux_dirent64*)entry_ptr; // Cast to the desired type
      string entry_name = d_entry->d_name;

      if (entry_name == "." || entry_name == "..") {
        continue;
      }

      string full_path = string(path) + "/" + entry_name;
      if (stat(full_path.c_str(), &entry_statbuf) == -1) {
        continue;
      }

      if (S_ISDIR(entry_statbuf.st_mode)) {
        total_size_kb += calculateDiskUsage(full_path.c_str());
      } else {
        total_size_kb += ceil(static_cast<double>(entry_statbuf.st_size) / 1024);
      }

      offset += d_entry->d_reclen;
    }
  }

  close(dir_fd);
  return total_size_kb;
}
