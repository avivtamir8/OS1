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

/*******************************************************
 *                 GIVEN UTILITIES                     *
 *******************************************************/
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

/*******************************************************
 *                 OUR IMPLEMENTATIONS                 *
 *******************************************************/

/**
 * @brief Constructs a SmallShell object and initializes its member variables.
 * 
 * This constructor sets up the initial state of the SmallShell object, including
 * the foreground process ID, foreground running flag, shell prompt, and working directories.
 * 
 * @param None.
 * @return None.
 */
SmallShell::SmallShell() 
  : foreground_pid(-1), 
    is_foreground_running(false), 
    prompt("smash"), 
    lastWorkingDir(""), 
    prevWorkingDir("") 
{}

/**
 * @brief Creates and returns the appropriate Command object based on the given command line input.
 * 
 * This function parses the command line, checks for aliases, and determines the type of command
 * (e.g., built-in, redirection, pipe, external). It then constructs and returns the corresponding
 * Command object.
 * 
 * @param cmd_line The raw command line input as a C-string.
 * @return A pointer to the created Command object.
 */
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

  // Handle redirection commands
  if (cmd_s.find(">") != string::npos || cmd_s.find(">>") != string::npos) {
    return new RedirectionCommand(cmd_s.c_str());
  }
  // Handle pipe commands
  else if (cmd_s.find("|") != string::npos || cmd_s.find("|&") != string::npos) {
    return new PipeCommand(cmd_s.c_str());
  }
  // Handle built-in commands
  else if (firstWord == "chprompt") {
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
  } else if (firstWord == "du")  {
    return new DiskUsageCommand(cmd_s.c_str());
  } else if (firstWord == "whoami") {
    return new WhoAmICommand(cmd_s.c_str());
  }
  // Handle external commands
  else {
    if (cmd_s.find('?') != string::npos || cmd_s.find('*') != string::npos) {
      return new ComplexExternalCommand(cmd_s.c_str(), jobs);
    } else {
      return new SimpleExternalCommand(cmd_s.c_str(), jobs);
    }
  }
}

/**
 * @brief Executes a command by creating and running the appropriate Command object.
 * 
 * This function parses the given command line, creates the corresponding Command object,
 * executes it, and then cleans up the allocated memory for the command.
 * 
 * @param cmd_line The command line input as a C-string.
 * @return None.
 */
void SmallShell::executeCommand(const char *cmd_line) {
  // Create the appropriate Command object based on the command line input
  Command *cmd = CreateCommand(cmd_line);

  // Execute the command
  cmd->execute();

  // Clean up the allocated Command object
  delete cmd;
}

/**
 * @brief Retrieves the last working directory stored in the shell.
 * 
 * This function returns the last directory path that was set in the shell.
 * It is used to track the previous working directory for commands like "cd -".
 * 
 * @return A string representing the last working directory.
 */
string SmallShell::getLastDir() const {
  return lastWorkingDir;
}

/**
 * @brief Sets the last working directory for the shell.
 * 
 * This function updates the lastWorkingDir member variable with the provided directory path.
 * 
 * @param dir The directory path to set as the last working directory.
 * @return None.
 */
void SmallShell::setLastDir(const string &dir) {
  lastWorkingDir = dir;
}

/**
 * @brief Sets the foreground process ID and marks it as running.
 * 
 * This function updates the foreground process ID and sets the 
 * foreground running flag to true.
 * 
 * @param pid The process ID to set as the foreground process.
 * @return None.
 */
void SmallShell::setForegroundPid(pid_t pid) {
  foreground_pid = pid;
  is_foreground_running = true;
}

/**
 * @brief Retrieves the PID of the current foreground process.
 * 
 * This function checks if a foreground process is currently running
 * and returns its PID. If no foreground process is running, it returns -1.
 * 
 * @return The PID of the foreground process, or -1 if no process is running.
 */
pid_t SmallShell::getForegroundPid() const {
  return is_foreground_running ? foreground_pid : -1; // Return -1 if no foreground process
}

/**
 * @brief Clears the foreground process ID and marks it as not running.
 * 
 * This function resets the foreground process ID to -1 and sets the 
 * foreground running flag to false, indicating that no process is 
 * currently running in the foreground.
 * 
 * @param None.
 * @return None.
 */
void SmallShell::clearForegroundPid() {
  foreground_pid = -1;
  is_foreground_running = false;
}

/**
 * @brief Constructs a Command object by parsing the command line input.
 * 
 * This constructor initializes the Command object, determines if the command
 * is a background command, and parses the command line into individual arguments.
 * 
 * @param cmd_line_input The raw command line input as a C-string.
 */
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

/**
 * @brief Retrieves the alias associated with the command.
 * 
 * This function returns the alias string that is associated with the command.
 * 
 * @return The alias string of the command.
 */
string Command::getAlias() const {
  return alias;
}

/**
 * @brief Sets the alias for the command.
 * 
 * This function assigns the provided alias string to the command's alias member.
 * 
 * @param aliasCommand The alias string to be set for the command.
 * @return None.
 */
void Command::setAlias(const string &aliasCommand) {
  alias = aliasCommand;
}

/**
 * @brief Executes the ChPromptCommand to change the shell prompt.
 * 
 * This function updates the shell's prompt to the specified argument.
 * If no argument is provided, it resets the prompt to the default value "smash".
 * 
 * @param None (uses the command-line arguments stored in the `args` member).
 * @return None (modifies the shell's prompt).
 */
void ChPromptCommand::execute() {
  if (args.size() > 1) {
    SmallShell::getInstance().setPrompt(args[1]);
  } else {
    SmallShell::getInstance().setPrompt("smash");
  }
}

/**
 * @brief Displays the process ID (PID) of the smash shell.
 * 
 * This function retrieves the PID of the current process using `getpid()`
 * and prints it to the standard output in a formatted message.
 * 
 * @param None.
 * @return None (outputs the PID to standard output).
 */
void ShowPidCommand::execute() {
  cout << "smash pid is " << getpid() << endl;
}

/**
 * @brief Retrieves and prints the current working directory.
 * 
 * This function uses the `getcwd` system call to obtain the absolute path
 * of the current working directory and prints it to the standard output.
 * If an error occurs, it prints an error message to the standard error.
 * 
 * @param None.
 * @return None (outputs the current working directory or an error message).
 */
void GetCurrDirCommand::execute() {
  char cwd[PATH_MAX];
  if (getcwd(cwd, sizeof(cwd)) != nullptr) {
    cout << cwd << endl;
  } else {
    perror("getcwd() error");
  }
}

/**
 * @brief Executes the ChangeDirCommand to change the current working directory.
 * 
 * This function handles changing the directory to a specified path, including
 * special cases like "-" (previous directory) and ".." (parent directory).
 * It updates the shell's last working directory and handles errors appropriately.
 * 
 * @param None (uses the command-line arguments stored in the `args` member).
 * @return None (outputs error messages to standard error if applicable).
 */
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

/**
 * @brief Executes the ForegroundCommand to bring a job to the foreground.
 * 
 * This function validates the input arguments, retrieves the specified job,
 * and waits for its process to finish. It also updates the shell's foreground
 * process state and handles errors appropriately.
 * 
 * @param None (uses the command-line arguments stored in the `args` member).
 * @return None (outputs error messages to standard error if applicable).
 */
void ForegroundCommand::execute() {
  jobs.removeFinishedJobs();

  if (args.size() > 2) {
    cerr << "smash error: fg: invalid arguments" << endl;
    return;
  }

  if (jobs.getJobById(jobId) == nullptr) {
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
  if (waitpid(jobs.getJobById(jobId)->getPid(), &status, WUNTRACED) == -1) {
    perror("smash error: waitpid failed");
    return;
  }

  if (WIFEXITED(status)) {
    jobs.removeJobById(jobId);
    smash.clearForegroundPid();
  }
  return;
}

/**
 * @brief Handles the alias command to create, list, or validate aliases.
 * 
 * This function processes the alias command to either list all aliases,
 * create a new alias, or validate the alias format. It ensures proper
 * formatting, checks for reserved keywords, and validates the existence
 * of the command in the system's PATH.
 * 
 * @param None (uses the command-line arguments stored in the `cmd_line` member).
 * @return None (outputs results or error messages to standard output or error).
 */
void AliasCommand::execute() {
  // Trim the command line to handle any spaces
  string commandLine = _trim(cmd_line); // TODO: no need the command line is already trimmed of spaces. how does this deal with &

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

/**
 * @brief Adds or updates an alias in the alias map.
 * 
 * This function checks for alias loops and adds the alias to the map.
 * 
 * @param aliasName The name of the alias.
 * @param aliasCommand The command associated with the alias.
 * @return None (outputs error messages if a loop is detected).
 */
void SmallShell::setAlias(const string& aliasName, const string& aliasCommand) {
  if (aliasCommand == aliasName) {
    cerr << "smash error: alias: alias loop detected" << endl;
    return;
  }
  aliasMap[aliasName] = aliasCommand;
}

/**
 * @brief Removes an alias from the alias map.
 * 
 * This function deletes an alias by its name. If the alias does not exist,
 * an error message is displayed.
 * 
 * @param aliasName The name of the alias to remove.
 * @return None (outputs error messages if the alias does not exist).
 */
void SmallShell::removeAlias(const string& aliasName) {
  if (aliasMap.erase(aliasName) == 0) {
    cerr << "smash error: unalias: alias \"" << aliasName << "\" does not exist" << endl;
  }
}

/**
 * @brief Retrieves the command associated with an alias.
 * 
 * This function searches for an alias by its name and returns the associated
 * command. If the alias does not exist, an empty string is returned.
 * 
 * @param aliasName The name of the alias to retrieve.
 * @return The command associated with the alias, or an empty string if not found.
 */
string SmallShell::getAlias(const string& aliasName) const {
  auto it = aliasMap.find(aliasName);
  if (it != aliasMap.end()) {
    return it->second;
  }
  return ""; // Alias not found
}

/**
 * @brief Prints all aliases in the alias map.
 * 
 * This function iterates through the alias map and prints each alias
 * and its associated command in the format "alias -> command".
 * 
 * @param None.
 * @return None (outputs the aliases to standard output).
 */
void SmallShell::printAliases() const {
  for (const auto& alias : aliasMap) {
    cout << alias.first << " -> " << alias.second << endl;
  }
}

/**
 * @brief Executes the JobsCommand to display the list of active jobs.
 * 
 * This function removes any finished jobs from the jobs list and then
 * prints the remaining active jobs to the standard output.
 * 
 * @param None (uses the jobs list stored in the `jobs` member).
 * @return None (outputs the jobs list to the standard output).
 */
void JobsCommand::execute() {
  jobs.removeFinishedJobs();
  jobs.printJobsList();
}

/**
 * @brief Executes an external command, handling both foreground and background processes.
 * 
 * This function forks a child process to execute the command. For foreground commands,
 * the parent process waits for the child to finish. For background commands, the child
 * process is added to the jobs list. The child process is separated into its own process group.
 * 
 * @param None (uses the command-line arguments stored in the `cmd_line` member).
 * @return None (outputs errors to standard error if applicable).
 */
void SimpleExternalCommand::execute() {
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
      jobs.addJob(cmd_line, pid);
    }
  }
}

/**
 * @brief Executes a complex external command using `/bin/bash`.
 * 
 * This function forks a child process to execute the command via `/bin/bash -c`.
 * For foreground commands, the parent process waits for the child to finish.
 * For background commands, the child process is added to the jobs list.
 * 
 * @param None (uses the command-line arguments stored in the `cmd_line` member).
 * @return None (outputs errors to standard error if applicable).
 */
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
      jobs.addJob(cmd_line, pid);
    }
  }
}

/**
 * @brief Removes one or more aliases from the alias map.
 * 
 * This function checks if the provided alias names exist in the alias map.
 * If an alias exists, it is removed. If an alias does not exist, an error
 * message is displayed.
 * 
 * @param None (uses the command-line arguments stored in the `args` member).
 * @return None (outputs error messages to standard error if applicable).
 */
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

/**
 * @brief Executes the KillCommand to send a signal to a specific job's process.
 * 
 * This function validates the input arguments, retrieves the job by its ID, and sends
 * the specified signal to the job's process. If the operation is successful, it prints
 * a confirmation message. Otherwise, it displays appropriate error messages.
 * 
 * @param None (uses the command-line arguments stored in the `args` member).
 * @return None (outputs success or error messages to standard output or error).
 */
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

/**
 * @brief Executes the QuitCommand to terminate the shell.
 * 
 * This function checks if the "kill" argument is provided. If so, it sends a SIGKILL signal
 * to all remaining jobs in the jobs list, prints their details, and clears the jobs list.
 * Finally, it exits the shell process.
 * 
 * @param None (uses the command-line arguments stored in the `args` member).
 * @return None (terminates the shell process).
 */
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

/**
 * @brief Unsets environment variables specified in the command arguments.
 * 
 * This function removes one or more environment variables from the environment.
 * If a variable does not exist or an error occurs during removal, an appropriate
 * error message is displayed.
 * 
 * @param None (uses the command-line arguments stored in the `args` member).
 * @return None (outputs error messages to standard error if applicable).
 */
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

/**
 * @brief Monitors the CPU and memory usage of a specific process.
 * 
 * This function takes a process ID (PID) as input, validates its existence, and calculates
 * the CPU and memory usage of the process over a 1-second interval. The results are displayed
 * in a formatted output.
 * 
 * @note The function assumes the PID is valid and accessible in the /proc filesystem.
 * 
 * @param None (uses the command-line arguments stored in the `args` member).
 * @return None (outputs the results to the standard output or error messages to standard error).
 */
void WatchProcCommand::execute() {
  // Validate the number of arguments
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

/**
 * @brief Reads the CPU times for a specific process and the total system CPU time.
 * 
 * This function retrieves the CPU usage information for a given process by reading
 * the /proc/<pid>/stat file and calculates the total system CPU time from /proc/stat.
 * 
 * @param pid The process ID whose CPU times are to be read.
 * @param proc_time Reference to store the process CPU time (user + kernel mode).
 * @param total_time Reference to store the total system CPU time.
 * @return True if the CPU times were successfully read, false otherwise.
 */
bool WatchProcCommand::readCpuTimes(pid_t pid, long long &proc_time, long long &total_time) {
  // Read process CPU time from /proc/<pid>/stat
  string proc_stat_path = "/proc/" + to_string(pid) + "/stat";
  int fd = open(proc_stat_path.c_str(), O_RDONLY);
  if (fd == -1) {
    return false;
  }

  char buffer[1024]; // Buffer size sufficient for /proc/<pid>/stat content
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

/**
 * @brief Reads the memory usage of a process in megabytes (MB).
 * 
 * This function retrieves the Resident Set Size (VmRSS) of a process
 * from the /proc/<pid>/status file, which represents the physical memory
 * used by the process.
 * 
 * @param pid The process ID whose memory usage is to be read.
 * @return The memory usage in MB as a double. Returns 0.0 if the process
 *         does not exist or an error occurs.
 */
double WatchProcCommand::readMemoryUsage(pid_t pid) {
  // Construct the path to the process's status file
  string proc_status_path = "/proc/" + to_string(pid) + "/status";

  // Open the status file
  int fd = open(proc_status_path.c_str(), O_RDONLY);
  if (fd == -1) {
    return 0.0;
  }

  // Read the contents of the status file
  char buffer[4096];
  ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
  close(fd);

  if (bytes_read <= 0) {
    return 0.0;
  }

  // Null-terminate the buffer
  buffer[bytes_read] = '\0';

  // Parse the file line by line
  istringstream iss(buffer);
  string line;
  while (getline(iss, line)) {
    // Look for the "VmRSS" field
    if (line.find("VmRSS:") == 0) {
      istringstream line_iss(line);
      string key;
      long memory_kb;
      string unit;

      // Extract the memory value in kilobytes
      line_iss >> key >> memory_kb >> unit;

      // Convert KB to MB and return
      return memory_kb / 1024.0;
    }
  }

  // Return 0.0 if VmRSS is not found
  return 0.0;
}

/**
 * @brief Executes a redirection command, redirecting the output of a command to a file.
 * 
 * This function handles both overwrite (>) and append (>>) redirection modes. It parses
 * the command line to extract the command to run and the target output file, then redirects
 * the standard output to the specified file. After execution, it restores the original
 * standard output.
 * 
 * @note The function assumes the command line is properly formatted for redirection.
 */
void RedirectionCommand::execute() {
  string command_to_run, output_file;
  bool append_mode = false;
  size_t redirect_pos;

  string cmd_line_copy = cmd_line;
  _removeBackgroundSign(&cmd_line_copy[0]);

  // Parse the command line for redirection operators
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

  // Validate parsed commands
  if (command_to_run.empty() || output_file.empty()) {
    cerr << "smash error: redirection: invalid format" << endl;
    return;
  }

  // Backup the original stdout file descriptor
  int original_stdout_fd = dup(STDOUT_FILENO);
  if (original_stdout_fd == -1) {
    perror("smash error: dup failed");
    return;
  }

  // Open the target output file
  int flags = O_CREAT | O_WRONLY;
  flags |= (append_mode) ? O_APPEND : O_TRUNC;
  int file_fd = open(output_file.c_str(), flags, 0666);
  if (file_fd == -1) {
    perror("smash error: open failed");
    close(original_stdout_fd);
    return;
  }

  // Redirect stdout to the target file
  if (dup2(file_fd, STDOUT_FILENO) == -1) {
    perror("smash error: dup2 failed");
    close(original_stdout_fd);
    close(file_fd);
    return;
  }

  // Close the file descriptor for the target file
  if (close(file_fd) == -1) {
    perror("smash error: close failed");
    dup2(original_stdout_fd, STDOUT_FILENO); // Restore original stdout
    close(original_stdout_fd);
    return;
  }

  // Execute the command
  fflush(stdout);
  SmallShell &smash = SmallShell::getInstance();
  smash.executeCommand(command_to_run.c_str());
  fflush(stdout); // Ensure all buffered output is written to the file

  // Restore the original stdout
  if (dup2(original_stdout_fd, STDOUT_FILENO) == -1) {
    perror("smash error: dup2 failed to restore stdout");
    close(original_stdout_fd);
    return;
  }

  // Close the backup file descriptor
  if (close(original_stdout_fd) == -1) {
    perror("smash error: close failed for original stdout fd");
  }
}

/**
 * @brief Executes a pipe command, connecting the output of one command to the input of another.
 * 
 * This function handles both standard output (|) and standard error (|&) redirection between two commands.
 * It forks two child processes to execute the commands and uses a pipe to connect their input/output.
 * 
 * @note The function assumes the command line is properly formatted for a pipe operation.
 */
void PipeCommand::execute() {
  string command_1, command_2;
  bool error_mode = false; // true for |& (stderr), false for | (stdout)
  size_t pipe_pos;

  string cmd_line_copy = cmd_line;
  _removeBackgroundSign(&cmd_line_copy[0]);

  // Parse the command line for pipe operators
  if ((pipe_pos = cmd_line_copy.find("|&")) != string::npos) {
    error_mode = true;
    command_1 = _trim(cmd_line_copy.substr(0, pipe_pos));
    command_2 = _trim(cmd_line_copy.substr(pipe_pos + 2));
  } else if ((pipe_pos = cmd_line_copy.find('|')) != string::npos) {
    error_mode = false;
    command_1 = _trim(cmd_line_copy.substr(0, pipe_pos));
    command_2 = _trim(cmd_line_copy.substr(pipe_pos + 1));
  } else {
    cerr << "smash error: pipe: invalid format" << endl;
    return;
  }

  // Validate parsed commands
  if (command_1.empty() || command_2.empty()) {
    cerr << "smash error: pipe: invalid format" << endl;
    return;
  }

  // Create a pipe
  int pipe_fd[2];
  if (pipe(pipe_fd) == -1) {
    perror("smash error: pipe failed");
    return;
  }

  SmallShell &smash = SmallShell::getInstance();

  // Fork the first child process
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

  // Fork the second child process
  pid_t pid2 = fork();
  if (pid2 == -1) {
    perror("smash error: fork failed for command2");
    kill(pid1, SIGINT); // Clean up first child if it was successfully forked
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

  // Parent process: Close pipe and wait for both children
  close(pipe_fd[0]);
  close(pipe_fd[1]);

  int status1, status2;
  if (waitpid(pid1, &status1, 0) == -1) {
    perror("smash error: waitpid failed for command1");
  }
  if (waitpid(pid2, &status2, 0) == -1) {
    perror("smash error: waitpid failed for command2");
  }
}

/**
 * @brief Executes the WhoAmICommand to retrieve and display the current user's information.
 *
 * This function retrieves the username and home directory of the current user
 * by accessing the "USER" and "HOME" environment variables, respectively. If both
 * variables are successfully retrieved, it prints the username and home directory
 * to the standard output. If either variable cannot be retrieved, it prints an error
 * message to the standard error output.
 *
 * @note This command relies on the presence of the "USER" and "HOME" environment variables.
 *       If these variables are not set, the function will fail to retrieve the user information.
 */
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


/**
 * @brief Executes the DiskUsageCommand to calculate and display the total disk usage of a directory.
 * 
 * This function validates the input arguments, checks if the specified path is a directory,
 * and calculates the total disk usage in kilobytes. It includes the size of the directory itself
 * and all its contents (recursively). If no path is provided, it defaults to the current directory.
 * 
 * @param None (uses the command-line arguments stored in the `args` member).
 * @return None (outputs the total disk usage to standard output or error messages to standard error).
 */
void DiskUsageCommand::execute() {
  // Validate the number of arguments
  if (args.size() > 2) {
    cerr << "smash error: du: too many arguments" << endl;
    return;
  }

  // Determine the target path (default to current directory if no argument is provided)
  const char* path = (args.size() == 1) ? "." : args[1].c_str();

  // Check if the specified path exists and is a directory
  struct stat statbuf;
  if (lstat(path, &statbuf) == -1 || !S_ISDIR(statbuf.st_mode)) {
    cerr << "smash error: du: directory " << path << " does not exist" << endl;
    return;
  }

  // Start with the size of the initial directory itself, based on its blocks
  long long total_usage_kb = (statbuf.st_blocks * 512LL + 1023) / 1024;

  // Add the sum of the sizes of its contents (calculated recursively)
  total_usage_kb += calculateDiskUsage(path);

  // Output the total disk usage
  cout << "Total disk usage: " << total_usage_kb << " KB" << endl;
}


/**
 * @brief Recursively calculates the total disk usage of a directory and its contents.
 * 
 * This function traverses the specified directory, including its subdirectories,
 * and calculates the total disk usage in kilobytes. It uses the `SYS_getdents64` 
 * system call to read directory entries and `lstat` to retrieve file metadata.
 * 
 * @param path The path to the directory whose disk usage is to be calculated.
 * @return The total disk usage in kilobytes as a long long integer.
 */
long long DiskUsageCommand::calculateDiskUsage(const char* path) {
  long long total_size_kb = 0;

  // Open the directory
  int dir_fd = open(path, O_RDONLY | O_DIRECTORY);
  if (dir_fd == -1) {
    return 0;
  }

  char buffer[4096];
  struct linux_dirent64 *d_entry;
  struct stat entry_statbuf;
  int bytes_read;

  // Read directory entries in batches
  while ((bytes_read = syscall(SYS_getdents64, dir_fd, buffer, sizeof(buffer))) > 0) {
    for (int offset = 0; offset < bytes_read;) {
      char* entry_ptr = buffer + offset;
      d_entry = (struct linux_dirent64*)entry_ptr;
      string entry_name = d_entry->d_name;

      // Skip "." and ".." entries
      if (entry_name == "." || entry_name == "..") {
        offset += d_entry->d_reclen;
        continue;
      }

      // Construct the full path of the entry
      string full_path = string(path) + "/" + entry_name;

      // Retrieve metadata for the entry
      if (lstat(full_path.c_str(), &entry_statbuf) == -1) {
        offset += d_entry->d_reclen;
        continue;
      }

      // Add the size of the current entry (file or directory itself)
      total_size_kb += (entry_statbuf.st_blocks * 512LL + 1023) / 1024;

      // If the entry is a directory, recursively calculate its size
      if (S_ISDIR(entry_statbuf.st_mode)) {
        total_size_kb += calculateDiskUsage(full_path.c_str());
      }

      // Move to the next directory entry
      offset += d_entry->d_reclen;
    }
  }

  // Close the directory
  close(dir_fd);
  return total_size_kb;
}
