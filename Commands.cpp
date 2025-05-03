#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include <regex>
#include <string>
#include <set>
#include <map>
#include "Commands.h"

using namespace std;

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

    if (firstWord == "chprompt") {
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
    } else if (firstWord == "fg") {
        return new ForegroundCommand(cmd_s.c_str(), jobs);
    } else {
        return new ExternalCommand(cmd_s.c_str(), jobs);
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
    string commandLine = _trim(cmd_line);

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

void ExternalCommand::execute() {
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
