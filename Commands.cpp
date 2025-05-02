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

// SmallShell class implementation
SmallShell::SmallShell() : prompt("smash"), jobs(new JobsList()) {
    // Initialize other fields if necessary
}

SmallShell::~SmallShell() {
    delete jobs;
}

Command *SmallShell::CreateCommand(const char *cmd_line) {
  // For example:
  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

  /* check whats wrong */
  // Handle alias expansion
  auto aliasIt = aliasMap.find(firstWord);
  if (aliasIt != aliasMap.end()) {
    string expandedCommand = aliasIt->second;

    // Append remaining arguments
    string remainingArgs;
    istringstream iss(_trim(string(cmd_line)).c_str());
    getline(iss, remainingArgs);
    if (!remainingArgs.empty()) {
      expandedCommand += " " + _trim(remainingArgs);
    }

    cmd_s = _trim(expandedCommand);
	}
  /* check whats wrong */


  if (firstWord == "chprompt") {
    return new ChPromptCommand(cmd_line);
  } else if (firstWord == "showpid") {
    return new ShowPidCommand(cmd_line);
  } else if (firstWord == "pwd") {
    return new GetCurrDirCommand(cmd_line);
  } else if (firstWord == "cd") {
    return new ChangeDirCommand(cmd_line);
  }  else if (firstWord == "jobs") {
    return new JobsCommand(cmd_line, jobs);
  } else if (firstWord == "alias") {
    return new AliasCommand(cmd_line, aliasMap);
  } else if (firstWord == "fg") {
    return new ForegroundCommand(cmd_line, jobs);
  } 
  else {
    return new ExternalCommand(cmd_line, jobs);
  }
  return nullptr;
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
  jobs->removeFinishedJobs();
  if(args.size() > 2) {
    cerr << "smash error: fg: invalid arguments" << endl;
    return;
  }

  if(jobs->getJobById(jobId) == nullptr) {
    cerr << "smash error: fg: job-id " << jobId << " does not exist" << endl;
    return;
  }
  if (args.size() == 1 && jobs->isEmpty()) {
    cerr << "smash error: fg: jobs list is empty" << endl;
    return;    
  }
  
  // Print the command line and PID
  cout << jobs->getJobById(jobId)->getCmdLine() << " : " << jobs->getJobById(jobId)->getPid() << endl;

  // Wait for the job's process to finish
  int status;
  if(waitpid(jobs->getJobById(jobId)->getPid(), &status, WUNTRACED) == -1) {
    perror("smash error: waitpid failed");
    return;
  }
  if(WIFEXITED(status)) {
    jobs->removeJobById(jobId);
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
  jobs->removeFinishedJobs();
  jobs->printJobsList();
};

void ExternalCommand::execute() {
  pid_t pid = fork();
  if (pid == -1) {
      perror("smash error: fork failed");
      return;
  }

  if (pid == 0) {
      // Child process
      setpgrp(); // Disconnect from the parent's process group for background commands
      // Use the inherited args field directly
      vector<char*> c_args; 
      for (const auto &arg : args) {
          c_args.push_back(const_cast<char *>(arg.c_str()));
      }
      if (_isBackgroundComamnd(c_args.back())) {
          _removeBackgroundSign(c_args.back()); // Remove the background sign if present
      }
      c_args.back() = strdup(_trim(c_args.back()).c_str()); // Trim the last argument and update it
      c_args.push_back(nullptr); // Null-terminate the arguments array

      if (execvp(c_args[0], c_args.data()) == -1) {
          perror("smash error: execvp failed");
          exit(1); // Exit the child process if execvp fails
      }
  } else {
      // Parent process
      if (!is_background) {
          // Foreground execution: Wait for the child process to finish
          int status;
          if (waitpid(pid, &status, 0) == -1) {
              perror("smash error: waitpid failed");
          }
      } else {
          // Background execution: Create a job instance and add it to the jobs list
          jobs->addJob(cmd_line, pid, false); // Add the job to the jobs list
          cout << "Background process started with PID: " << pid << endl;
      }
  }
}