#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

// TODO: Add your implementation for classes in Commands.h 

SmallShell::SmallShell() : prompt("smash") {
// TODO: add your implementation
}

SmallShell::~SmallShell() {
// TODO: add your implementation
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {
    // For example:
  
    string cmd_s = _trim(string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

    if (firstWord.compare("chprompt") == 0) {
      return new ChPromptCommand(cmd_line);
    }
    else if (firstWord.compare("showpid") == 0) {
      return new ShowPidCommand(cmd_line);
    }
    else if (firstWord.compare("pwd") == 0) {
      return new GetCurrDirCommand(cmd_line);
    }
    else if (firstWord.compare("cd") == 0) {
      return new ChPromptCommand(cmd_line);
    }
    // else if (firstWord.compare("jobs") == 0) {
    //   return new JobsList(cmd_line);
    // }
    // else if (firstWord.compare("kill") == 0) {
    //   return new KillCommand(cmd_line);
    // }
    // else if (firstWord.compare("fg") == 0) {
    //   return new ForegroundCommand(cmd_line);
    // }
    // else if (firstWord.compare("bg") == 0) {
    //   return new BackgroundCommand(cmd_line);
    // }
    // else if (firstWord.compare("quit") == 0) {
    //   return new QuitCommand(cmd_line, jobs);
    // }
    // else if (firstWord.compare("alias") == 0) {
    //   return new AliasCommand(cmd_line);
    // }
    // else if (firstWord.compare("unalias") == 0) {
    //   return new UnAliasCommand(cmd_line);
    // }
    // else if (firstWord.compare("setenv") == 0) {
    //   return new SetEnvCommand(cmd_line);
    // }
    // else if (firstWord.compare("unsetenv") == 0) {
    //   return new UnSetEnvCommand(cmd_line);
    // }
    // else if (firstWord.compare("watchproc") == 0) {
    //   return new WatchProcCommand(cmd_line);
    // }
    // else {
    //   return new ExternalCommand(cmd_line);
    // }
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
    delete cmd; // Free the command object after execution
}

/*
* Command class methods
*/
Command::Command(const char *cmd_line) : cmd_line(cmd_line), is_background(false) {
    // Parse the command line into arguments
    char *raw_args[COMMAND_MAX_ARGS + 1]; // Temporary array for parsing
    int argsCount = _parseCommandLine(cmd_line, raw_args);

    // Convert parsed arguments to std::string and store in args vector
    for (int i = 0; i < argsCount; ++i) {
        args.push_back(std::string(raw_args[i]));
        free(raw_args[i]); // Free allocated memory
    }

    // Check if the command is a background command
    is_background = _isBackgroundComamnd(cmd_line);

    // Remove the background sign if present
    if (is_background && !args.empty()) {
        _removeBackgroundSign(&args.back()[0]);
    }
}


/*
* BuiltInCommand class methods
*/
void ChPromptCommand::execute() {
  if (args.size() > 1) {
    // Set the new prompt to the first argument after the command
    SmallShell::getInstance().setPrompt(args[1]);
  } else {
    // Reset to default prompt "smash"
    SmallShell::getInstance().setPrompt("smash");
  }
}

void ShowPidCommand::execute() {
  // Print the process ID of the current shell
  cout << "smash pid is " << getpid() << endl;
}

void GetCurrDirCommand::execute() {
  // Get the current working directory
  char cwd[PATH_MAX];
  if (getcwd(cwd, sizeof(cwd)) != nullptr) {
    cout << cwd << endl;
  } else {
    perror("getcwd() error");
  }
}

std::string SmallShell::getLastDir() const {
	return lastWorkingDir;
}
void SmallShell::setLastDir(const std::string& dir) {
	lastWorkingDir = dir;
}

void ChangeDirCommand::execute() {
	SmallShell& shell = SmallShell::getInstance();

	// No arguments: Do nothing
	if (args.size() == 1) {
		return;
	}

	// Too many arguments: Print error and return
	if (args.size() > 2) {
		std::cerr << "smash error: cd: too many arguments" << std::endl;
		return;
	}

	const std::string& targetDir = args[1];
	char* currentDir = getcwd(nullptr, 0); // Get the current working directory
	if (!currentDir) {
		perror("smash error: getcwd failed");
		return;
	}

	// Handle "cd -": Change to the last working directory
	if (targetDir == "-") {
		if (shell.getLastDir().empty()) {
			std::cerr << "smash error: cd: OLDPWD not set" << std::endl;
			free(currentDir);
			return;
		}
		if (chdir(shell.getLastDir().c_str()) == -1) {
			perror("smash error: chdir failed");
		}
		else {
			shell.setLastDir(currentDir); // Update lastWorkingDir with the previous directory
		}
		free(currentDir);
		return;
	}

	// Handle "cd ..": Move up one directory
	if (targetDir == "..") {
		if (chdir("..") == -1) {
			perror("smash error: chdir failed");
		}
		else {
			shell.setLastDir(currentDir); // Update lastWorkingDir
		}
		free(currentDir);
		return;
	}

	// Handle regular path: Change directory
	if (chdir(targetDir.c_str()) == -1) {
		perror("smash error: chdir failed");
	}
	else {
		shell.setLastDir(currentDir); // Update lastWorkingDir
	}
	free(currentDir);
}


/*
* ExternalCommand class methods
*/

/*
 * RedirectionCommand class methods 
 */







