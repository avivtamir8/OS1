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
#include <unordered_set>


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
 * @param cmd_line_cstr The raw command line input as a C-string.
 * @return A pointer to the created Command object.
 */
Command *SmallShell::CreateCommand(const char *cmd_line_cstr) {
  string cmd_s = _trim(string(cmd_line_cstr));
  regex alias_definition_regex("^alias [a-zA-Z0-9_]+='[^']*'$");

  // 1. Check if the entire command is an alias definition
  if (regex_match(cmd_s, alias_definition_regex)) {
    return new AliasCommand(cmd_s.c_str(), aliasMap);
  }

  // 2. Attempt alias expansion (one level)
  // Extract the first word for potential expansion
  string first_word_for_expansion = cmd_s.substr(0, cmd_s.find_first_of(WHITESPACE));
  auto aliasIt = aliasMap.find(first_word_for_expansion);

  if (aliasIt != aliasMap.end()) { // first_word_for_expansion is an alias
    string base_alias_command = aliasIt->second;
    string remaining_args_str;
    size_t first_word_len = first_word_for_expansion.length();

    if (cmd_s.length() > first_word_len) {
        // Check if character after firstWord is whitespace, implying arguments follow
        char next_char = cmd_s[first_word_len];
        // Check for any whitespace character using isspace
        if (isspace(static_cast<unsigned char>(next_char))) { 
            remaining_args_str = cmd_s.substr(first_word_len); // Includes leading whitespace
        }
    }
    cmd_s = _trim(base_alias_command + remaining_args_str);
    // cmd_s is now the expanded command string.
  }

  // 3. Proceed with parsing the (potentially expanded) command string cmd_s
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(WHITESPACE));

  // Handle redirection commands
  if (cmd_s.find(">") != string::npos || cmd_s.find(">>") != string::npos) {
    _removeBackgroundSign(&cmd_s[0]); // Remove background sign if present
    return new RedirectionCommand(cmd_s.c_str());
  }
  // Handle pipe commands
  else if (cmd_s.find("|") != string::npos || cmd_s.find("|&") != string::npos) {
    _removeBackgroundSign(&cmd_s[0]); // Remove background sign if present
    return new PipeCommand(cmd_s.c_str());
  }
  // Handle built-in commands
  else if (firstWord == "chprompt") {
    _removeBackgroundSign(&cmd_s[0]); // Remove background sign if present
    return new ChPromptCommand(cmd_s.c_str());
  } else if (firstWord == "showpid") {
    _removeBackgroundSign(&cmd_s[0]); // Remove background sign if present
    return new ShowPidCommand(cmd_s.c_str());
  } else if (firstWord == "pwd") {
    _removeBackgroundSign(&cmd_s[0]); // Remove background sign if present
    return new GetCurrDirCommand(cmd_s.c_str());
  } else if (firstWord == "cd") {
    _removeBackgroundSign(&cmd_s[0]); // Remove background sign if present
    return new ChangeDirCommand(cmd_s.c_str());
  } else if (firstWord == "jobs") {
    _removeBackgroundSign(&cmd_s[0]); // Remove background sign if present
    return new JobsCommand(cmd_s.c_str(), jobs);
  } else if (firstWord == "alias") {
    // This handles "alias" (to list aliases) or invalid alias definitions
    // that were not caught by the regex, e.g., "alias name" or "alias name="
    _removeBackgroundSign(&cmd_s[0]); // Remove background sign if present
    return new AliasCommand(cmd_s.c_str(), aliasMap);
  } else if (firstWord == "unalias") {
    _removeBackgroundSign(&cmd_s[0]); // Remove background sign if present
    return new UnAliasCommand(cmd_s.c_str(), aliasMap);
  } else if (firstWord == "kill") {
    _removeBackgroundSign(&cmd_s[0]); // Remove background sign if present
    return new KillCommand(cmd_s.c_str(), jobs);
  } else if (firstWord == "quit") {
    _removeBackgroundSign(&cmd_s[0]); // Remove background sign if present
    return new QuitCommand(cmd_s.c_str(), jobs);
  } else if (firstWord == "fg") {
    _removeBackgroundSign(&cmd_s[0]); // Remove background sign if present
    return new ForegroundCommand(cmd_s.c_str(), jobs);
  } else if (firstWord == "unsetenv") {
    _removeBackgroundSign(&cmd_s[0]); // Remove background sign if present
    return new UnSetEnvCommand(cmd_s.c_str());
  } else if (firstWord == "watchproc") {
    _removeBackgroundSign(&cmd_s[0]); // Remove background sign if present
    return new WatchProcCommand(cmd_s.c_str());
  } else if (firstWord == "du")  {
    _removeBackgroundSign(&cmd_s[0]); // Remove background sign if present
    return new DiskUsageCommand(cmd_s.c_str());
  } else if (firstWord == "whoami") {
    _removeBackgroundSign(&cmd_s[0]); // Remove background sign if present
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


/*******************************************************
 *            BUILT-IN COMMAND IMPLEMENTATION          *
 *******************************************************/

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
    perror("smash error: getcwd failed");
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

  int effective_job_id_to_use;

  // Determine effective_job_id_to_use based on arguments
  if (args.size() == 1) { // Case: fg (no arguments)
    if (jobs.isEmpty()) {
      cerr << "smash error: fg: jobs list is empty" << endl;
      return;
    }
    effective_job_id_to_use = jobs.getLargestJobId();
  } else if (args.size() == 2) { // Case: fg <job-id>
    try {
      effective_job_id_to_use = stoi(args[1]);
    } catch (const invalid_argument &e) {
      cerr << "smash error: fg: invalid arguments" << endl; // Non-numeric job-id
      return;
    } catch (const out_of_range &e) { // Number too large/small for int
      cerr << "smash error: fg: invalid arguments" << endl;
      return;
    }
  } else { // Case: Too many arguments
    cerr << "smash error: fg: invalid arguments" << endl;
    return;
  }

  // Get the job using the determined effective_job_id_to_use
  JobsList::JobEntry *job = jobs.getJobById(effective_job_id_to_use);

  if (job == nullptr) {
    cerr << "smash error: fg: job-id " << effective_job_id_to_use << " does not exist" << endl;
    return;
  }

  // Print the command line and PID
  cout << job->getCmdLine() << " " << job->getPid() << endl;

  // Set the foreground PID in SmallShell
  SmallShell &smash = SmallShell::getInstance();
  smash.setForegroundPid(job->getPid());

  // Wait for the job's process to finish
  int status;
  if (waitpid(job->getPid(), &status, WUNTRACED) == -1) {
    perror("smash error: waitpid failed");
    smash.clearForegroundPid(); // Clear foreground PID as waiting failed
    return;
  }

  if (WIFEXITED(status) || WIFSIGNALED(status)) {
    jobs.removeJobById(job->getJobId());
    smash.clearForegroundPid();
  } else if (WIFSTOPPED(status)) {
    smash.clearForegroundPid();
  }
  return;
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
        return;
      }
    }

    // Clear the jobs list
    jobs.clearJobs();
  }

  // Exit the shell
  return;
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
    if (signum <= 0 || signum >= NSIG) { // Validate signal number range
      cerr << "smash error: kill: invalid signal number" << endl;
      return;
    }
  } catch (const invalid_argument &e) {
    cerr << "smash error: kill: invalid arguments" << endl;
    return;
  } catch (const out_of_range &e) {
    cerr << "smash error: kill: invalid signal number" << endl;
    return;
  }

  // Parse the job ID
  int jobId;
  try {
    jobId = stoi(args[2]);
    if (jobId <= 0) { // Validate job ID is positive
      cerr << "smash error: kill: invalid job-id" << endl;
      return;
    }
  } catch (const invalid_argument &e) {
    cerr << "smash error: kill: invalid arguments" << endl;
    return;
  } catch (const out_of_range &e) {
    cerr << "smash error: kill: invalid job-id" << endl;
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
        return;
      }
    }

    // Clear the jobs list
    jobs.clearJobs();
  }

  // Exit the shell
  return;
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
    if (signum <= 0 || signum >= NSIG) { // Validate signal number range
      cerr << "smash error: kill: invalid signal number" << endl;
      return;
    }
  } catch (const invalid_argument &e) {
    cerr << "smash error: kill: invalid arguments" << endl;
    return;
  } catch (const out_of_range &e) {
    cerr << "smash error: kill: invalid signal number" << endl;
    return;
  }

  // Parse the job ID
  int jobId;
  try {
    jobId = stoi(args[2]);
    if (jobId <= 0) { // Validate job ID is positive
      cerr << "smash error: kill: invalid job-id" << endl;
      return;
    }
  } catch (const invalid_argument &e) {
    cerr << "smash error: kill: invalid arguments" << endl;
    return;
  } catch (const out_of_range &e) {
    cerr << "smash error: kill: invalid job-id" << endl;
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
  string commandLine = _trim(cmd_line);

  // Case 1: If the command is exactly "alias" with no arguments
  if (commandLine == "alias") {
    // Print all aliases in the map
    for (const auto& alias_pair : aliasMap) { // Renamed 'alias' to 'alias_pair' to avoid conflict
      cout << alias_pair.first << "='" << alias_pair.second << "'" << endl;
    }
    return; // Exit after printing
  }

  // Case 2: Handle alias creation (alias <name>='<command>')
  // This part assumes CreateCommand passed a string that either is "alias" 
  // or starts with "alias " but might not be a fully valid definition yet.
  // The regex check in CreateCommand handles strictly valid definitions.
  // This code handles cases like "alias name" or "alias name=" which are invalid.

  size_t equalPos = commandLine.find('=');
  // The check for `equalPos < 6` (i.e., `alias ` is 6 chars) ensures `aliasName` is not empty.
  if (equalPos == string::npos || commandLine.rfind("alias ", 0) != 0 || equalPos < strlen("alias ")) { 
    cerr << "smash error: alias: invalid alias format" << endl;
    return;
  }
  
  string aliasName = _trim(commandLine.substr(strlen("alias "), equalPos - strlen("alias ")));
  string aliasCommandWithQuotes = _trim(commandLine.substr(equalPos + 1));

  // Validate alias name format (alphanumeric and underscores)
  if (aliasName.empty() || !regex_match(aliasName, regex("^[a-zA-Z0-9_]+$"))) {
    cerr << "smash error: alias: invalid alias format" << endl;
    return;
  }

  // Check for proper quotes around the alias command
  if (aliasCommandWithQuotes.length() < 2 || aliasCommandWithQuotes.front() != '\'' || aliasCommandWithQuotes.back() != '\'') {
    cerr << "smash error: alias: invalid alias format" << endl;
    return;
  }

  // Remove surrounding quotes from the command
  string aliasCommandValue = aliasCommandWithQuotes.substr(1, aliasCommandWithQuotes.length() - 2);

  // PDF Note: "No need to check if <command> is legal."
  // REMOVED: Validate that the command exists in the system's PATH
  // string commandToCheck = aliasCommandValue.substr(0, aliasCommandValue.find(' ')); 
  // if (system(("command -v " + commandToCheck + " > /dev/null 2>&1").c_str()) != 0) {
  //   cerr << "smash error: alias: command '" << commandToCheck << "' not found" << endl;
  //   return;
  // }

  // Check for reserved keywords
  static const set<string> reservedKeywords = {
    "quit", "fg", "bg", "jobs", "kill", "cd", "listdir", "chprompt", "alias", "unalias", "pwd", "showpid"
    // "listdir" might be an example, ensure this list matches your actual built-in commands.
    // Added "bg" as it's a common shell command, though not explicitly in the PDF's list for this homework.
    // For safety, stick to the PDF's examples or your implemented commands.
    // The PDF example was "eg., `quit`, `lisdir` etc…)", so the current list is a good interpretation.
  };

  if (reservedKeywords.count(aliasName) || aliasMap.count(aliasName)) {
    cerr << "smash error: alias: " << aliasName << " already exists or is a reserved command" << endl;
    return;
  }

  // Add the alias to the map
  aliasMap[aliasName] = aliasCommandValue;
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
 * @brief Unsets environment variables specified in the command arguments.
 * 
 * This function removes one or more environment variables from the environment
 * by directly manipulating the `__environ` array. If a variable does not exist
 * or an error occurs during removal, an appropriate error message is displayed.
 * 
 * @param None (uses the command-line arguments stored in the `args` member).
 * @return None (outputs error messages to standard error if applicable).
 */
void UnSetEnvCommand::execute() {
  // Static set to track removed variables
  static unordered_set<string> removedVariables;

  // Check if arguments are provided
  if (args.size() < 2) {
    cerr << "smash error: unsetenv: not enough arguments" << endl;
    return;
  }

  // Iterate through the provided environment variable names
  for (size_t i = 1; i < args.size(); ++i) {
    const string &varName = args[i];

    // Check if the variable has already been removed
    if (removedVariables.find(varName) != removedVariables.end()) {
      cerr << "smash error: unsetenv: " << varName << " does not exist" << endl;
      continue;
    }

    // Check if the environment variable exists by scanning /proc/<pid>/environ
    string environ_path = "/proc/" + to_string(getpid()) + "/environ";
    int fd = open(environ_path.c_str(), O_RDONLY);
    if (fd == -1) {
      perror("smash error: open failed");
      return;
    }

    char buffer[4096];
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes_read == -1) {
      perror("smash error: read failed");
      if (close(fd) == -1) {
        perror("smash error: close failed");
      }
      return;
    }

    if (close(fd) == -1) {
      perror("smash error: close failed");
      return;
    }

    buffer[bytes_read] = '\0'; // Null-terminate the buffer
    bool var_exists = false;
    char *entry = buffer; // Start scanning from the beginning of the buffer
    while (*entry != '\0') {
      if (strncmp(entry, varName.c_str(), varName.size()) == 0 && entry[varName.size()] == '=') {
        var_exists = true;
        break;
      }
      entry += strlen(entry) + 1; // Move to the next entry
    }

    if (!var_exists) {
      cerr << "smash error: unsetenv: " << varName << " does not exist" << endl;
      removedVariables.insert(varName); // Mark as removed to avoid future checks
      continue;
    }

    // Remove the environment variable by directly manipulating __environ
    extern char **environ;
    size_t env_size = 0;
    while (environ[env_size] != nullptr) {
      ++env_size;
    }

    bool removed = false;
    for (size_t j = 0; j < env_size; ++j) {
      if (strncmp(environ[j], varName.c_str(), varName.size()) == 0 && environ[j][varName.size()] == '=') {
        // Shift the remaining environment variables down
        for (size_t k = j; k < env_size - 1; ++k) {
          environ[k] = environ[k + 1];
        }
        environ[env_size - 1] = nullptr; // Null-terminate the array
        removed = true;
        break;
      }
    }

    if (!removed) {
      cerr << "smash error: unsetenv: failed to remove " << varName << endl;
      return;
    }

    // Add the variable to the removed set
    removedVariables.insert(varName);
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
    if (pid <= 0) { // Ensure PID is positive
      cerr << "smash error: watchproc: invalid arguments" << endl;
      return;
    }
  } catch (const invalid_argument &e) {
    cerr << "smash error: watchproc: invalid arguments" << endl;
    return;
  } catch (const out_of_range &e) {
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
  if (close(fd) == -1) {
    perror("smash error: close failed");
  }

  // Read initial CPU and system times
  long long prev_proc_time = 0, prev_total_time = 0;
  if (!readCpuTimes(pid, prev_proc_time, prev_total_time)) {
    cerr << "smash error: watchproc failed" << endl;
    return;
  }

  // Wait for 1 second to calculate deltas
  struct timespec req = {1, 0}; // 1 second, 0 nanoseconds
  if (nanosleep(&req, nullptr) == -1) {
    perror("smash error: nanosleep failed");
    return;
  }
  
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
    perror("smash error: open failed");
    return false;
  }

  char buffer[1024]; // Buffer size sufficient for /proc/<pid>/stat content
  ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
  if (close(fd) == -1) {
    perror("smash error: close failed");
  }

  if (bytes_read <= 0) {
    return false;
  }
  buffer[bytes_read] = '\0'; // Null-terminate the buffer

  istringstream iss(buffer);
  vector<string> tokens((istream_iterator<string>(iss)), istream_iterator<string>());

  if (tokens.size() < 17) {
    return false;
  }

  // Parse user and kernel mode times
  long utime, stime;
  try {
    utime = stoll(tokens[13]); // User mode time
    stime = stoll(tokens[14]); // Kernel mode time
  } catch (const invalid_argument &e) {
    cerr << "smash error: watchproc: invalid arguments" << endl;
    return false;
  } catch (const out_of_range &e) {
    cerr << "smash error: watchproc: invalid arguments" << endl;
    return false;
  }
  proc_time = utime + stime;

  // Read total CPU time from /proc/stat
  fd = open("/proc/stat", O_RDONLY);
  if (fd == -1) {
    perror("smash error: open failed");
    return false;
  }

  bytes_read = read(fd, buffer, sizeof(buffer) - 1);
  if (close(fd) == -1) {
    perror("smash error: close failed");
  }
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
    perror("smash error: open failed");
    return 0.0;
  }

  // Read the contents of the status file
  char buffer[4096];
  ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
  if (close(fd) == -1) {
    perror("smash error: close failed");
  }
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

/*******************************************************
 *            EXTERNAL COMMANDS IMPLEMENTATION         *
 *******************************************************/

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
      return;
    }

    // Free allocated memory (in case execvp fails unexpectedly)
    for (int i = 0; i < argsCount; ++i) {
      free(args[i]);
    }
    return;
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
      return;
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


/*******************************************************
 *              SPECIAL COMMANDS IMPLEMENTATION        *
 *******************************************************/

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
    if (close(original_stdout_fd) == -1) {
      perror("smash error: close failed");
    }
    return;
  }

  // Redirect stdout to the target file
  if (dup2(file_fd, STDOUT_FILENO) == -1) {
    perror("smash error: dup2 failed");
    if (close(original_stdout_fd) == -1) {
      perror("smash error: close failed");
    }
    if (close(file_fd) == -1) {
      perror("smash error: close failed");
    }
    return;
  }

  // Close the file descriptor for the target file
  if (close(file_fd) == -1) {
    perror("smash error: close failed");
    if (dup2(original_stdout_fd, STDOUT_FILENO) == -1) {  // Restore original stdout
      perror("smash error: dup2 failed");
    }
    if (close(original_stdout_fd) == -1) {
      perror("smash error: close failed");
    }
    return;
  }

  // Execute the command
  fflush(stdout);
  SmallShell &smash = SmallShell::getInstance();
  smash.executeCommand(command_to_run.c_str());
  fflush(stdout); // Ensure all buffered output is written to the file

  // Restore the original stdout
  if (dup2(original_stdout_fd, STDOUT_FILENO) == -1) {
    perror("smash error: dup2 failed");
    if (close(original_stdout_fd) == -1) {
      perror("smash error: close failed");
    }
    return;
  }

  // Close the backup file descriptor
  if (close(original_stdout_fd) == -1) {
    perror("smash error: close failed");
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
    if (close(pipe_fd[0]) == -1) {
      perror("smash error: close failed");
    }    
    if (close(pipe_fd[1]) == -1) {
      perror("smash error: close failed");
    }
    return;
  }

  if (pid1 == 0) {
    // Child 1: Executes command_1
    setpgrp();
    int target_fd_for_cmd1_output = error_mode ? STDERR_FILENO : STDOUT_FILENO;
    if (dup2(pipe_fd[1], target_fd_for_cmd1_output) == -1) {
      perror("smash error: dup2 failed");
      if (close(pipe_fd[0]) == -1) {
        perror("smash error: close failed");
      }    
      if (close(pipe_fd[1]) == -1) {
        perror("smash error: close failed");
      }
      return;
    }
    if (close(pipe_fd[0]) == -1) {
      perror("smash error: close failed");
    }    
    if (close(pipe_fd[1]) == -1) {
      perror("smash error: close failed");
    }
    smash.executeCommand(command_1.c_str());
    return;
  }

  // Fork the second child process
  pid_t pid2 = fork();
  if (pid2 == -1) {
    perror("smash error: fork failed");
    kill(pid1, SIGINT); // Clean up first child if it was successfully forked
    waitpid(pid1, nullptr, 0);
    if (close(pipe_fd[0]) == -1) {
      perror("smash error: close failed");
    }    
    if (close(pipe_fd[1]) == -1) {
      perror("smash error: close failed");
    }
    return;
  }

  if (pid2 == 0) {
    // Child 2: Executes command_2
    setpgrp();
    if (dup2(pipe_fd[0], STDIN_FILENO) == -1) {
      perror("smash error: dup2 failed");
      if (close(pipe_fd[0]) == -1) {
        perror("smash error: close failed");
      }    
      if (close(pipe_fd[1]) == -1) {
        perror("smash error: close failed");
      }
      return;
    }
    if (close(pipe_fd[0]) == -1) {
      perror("smash error: close failed");
    }    
    if (close(pipe_fd[1]) == -1) {
      perror("smash error: close failed");
    }
    smash.executeCommand(command_2.c_str());
    return;
  }

  // Parent process: Close pipe and wait for both children
  if (close(pipe_fd[0]) == -1) {
    perror("smash error: close failed");
  }    
  if (close(pipe_fd[1]) == -1) {
    perror("smash error: close failed");
  }

  int status1, status2;
  if (waitpid(pid1, &status1, 0) == -1) {
    perror("smash error: waitpid failed");
  }
  if (waitpid(pid2, &status2, 0) == -1) {
    perror("smash error: waitpid failed");
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
    perror("smash error: open failed");
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
        perror("smash error: lstat failed");
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
  if (close(dir_fd) == -1) {
    perror("smash error: close failed");
  }
  return total_size_kb;
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
  // apparently we cant use getenv(). well have to read /proc/<pid>/environ
  // to get the USER and HOME environment variables
  
  // Construct the path to /proc/<pid>/environ
  string environ_path = "/proc/" + to_string(getpid()) + "/environ";

  // Open the environ file
  int fd = open(environ_path.c_str(), O_RDONLY);
  if (fd == -1) {
    perror("smash error: open failed");
    return;
  }

  // Read the contents of the environ file
  char buffer[4096];
  ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
  if (bytes_read == -1) {
    perror("smash error: read failed");
    if (close(fd) == -1) {
      perror("smash error: close failed");
    }
    return;
  }

  // Null-terminate the buffer
  buffer[bytes_read] = '\0';

  // Close the file descriptor
  if (close(fd) == -1) {
    perror("smash error: close failed");
    return;
  }

  // Parse the buffer to find USER and HOME
  string user, home;
  char* entry = buffer; // Start scanning from the beginning of the buffer
  while (*entry != '\0') {
    string env_entry(entry); // start scannin from current entry pointer until the next null terminator
    size_t equal_pos = env_entry.find('=');
    if (equal_pos != string::npos) {
      string key = env_entry.substr(0, equal_pos);
      string value = env_entry.substr(equal_pos + 1);

      if (key == "USER") {
        user = value;
      } else if (key == "HOME") {
        home = value;
      }
    }
    entry += strlen(entry) + 1; // Move to the next entry
  }

  // Check if both USER and HOME were found
  if (!user.empty() && !home.empty()) {
    cout << user << " " << home << endl;
  } else {
    cerr << "smash error: whoami: failed to retrieve user information" << endl;
  }
}