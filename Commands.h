// Ver: 10-4-2025
#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <string>
#include <vector>
#include <memory>
#include <ctime>
#include <sys/wait.h>
#include <map>

#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

using namespace std;

/*
 * JobsList and Related Commands
 */

/*
 * Command Class Definition
 */
class Command {
    protected:
        string cmd_line;
        vector<string> args;
        bool is_background;
    
    public:
        explicit Command(const char *cmd_line);
        virtual ~Command() = default;
    
        virtual void execute() = 0;
        const string &getCmdLine() const { return cmd_line; }
        const vector<string> &getArgs() const { return args; }
        bool isBackground() const { return is_background; }
        std::string getAlias() const;
        void setAlias(const std::string& aliasCommand);

    
    };
class JobsList {
public:
    class JobEntry {
        int jobId;
        pid_t pid;
        bool isStopped;
        string cmdLine;

    public:
        JobEntry(int jobId, pid_t pid, const string& cmdLine, bool isStopped = false)
            : jobId(jobId), pid(pid), isStopped(isStopped), cmdLine(cmdLine) {}

        int getJobId() const { return jobId; }
        pid_t getPid() const { return pid; }
        bool getIsStopped() const { return isStopped; }
        void setStopped(bool val) { isStopped = val; }
        const std::string& getCmdLine() const { return cmdLine; }
    };

private:
    vector<JobEntry*> jobs;

    int getLargestJobId() {
        removeFinishedJobs();
        if (jobs.empty()) {
            return 0;
        }
        int maxId = 0;
        for (const JobEntry* job : jobs) {
            if (job->getJobId() > maxId) {
                maxId = job->getJobId();
            }
        }
        return maxId;
    }

public:
    JobsList() {}
    ~JobsList() {
        for (JobEntry* job : jobs) {
            delete job;
        }
    }

    void addJob(string cmdLine, pid_t pid, bool isStopped = false) {
        int jobId = getLargestJobId() + 1;
        jobs.push_back(new JobEntry(jobId, pid, cmdLine, isStopped));
    }

    void printJobsList() const {
        for (const JobEntry* job : jobs) {
            cout << "[" << job->getJobId() << "] " << job->getCmdLine() << endl;
        }
    }

    void killAllJobs() {
        for (JobEntry* job : jobs) {
            if (kill(job->getPid(), SIGKILL) == -1) {
                perror("smash error: kill failed");
            }
        }
        for (JobEntry* job : jobs) {
            delete job;
        }
        jobs.clear();
    }

    void removeFinishedJobs() {
        vector<JobEntry*> updatedJobs; // Temporary vector to store non-finished jobs
        if (jobs.empty()) {
            return;
        }

        for (JobEntry* job : jobs) {
            if (waitpid(job->getPid(), nullptr, WNOHANG) > 0) {
                // Job has finished, delete it
                delete job;
            } else {
                // Job is still running, keep it
                updatedJobs.push_back(job);
            }
        }

        // Replace the original jobs vector with the updated one
        jobs = std::move(updatedJobs);
    }

    JobEntry* getJobById(int jobId) {
        for (JobEntry* job : jobs) {
            if (job->getJobId() == jobId) {
                return job;
            }
        }
        return nullptr;
    }

    void removeJobById(int jobId) {
        for (auto it = jobs.begin(); it != jobs.end(); ++it) {
            if ((*it)->getJobId() == jobId) {
                delete *it;
                jobs.erase(it);
                break;
            }
        }
    }

    JobEntry* getLastJob(int* lastJobId) {
        if (jobs.empty()) {
            return nullptr;
        }
        *lastJobId = jobs.back()->getJobId();
        return jobs.back();
    }

    JobEntry* getLastStoppedJob(int* jobId) {
        for (auto it = jobs.rbegin(); it != jobs.rend(); ++it) {
            if ((*it)->getIsStopped()) {
                *jobId = (*it)->getJobId();
                return *it;
            }
        }
        return nullptr;
    }
};


/*
 * Built-In Command Base Class
 */
class BuiltInCommand : public Command {
public:
    explicit BuiltInCommand(const char *cmd_line) : Command(cmd_line) {}
    virtual ~BuiltInCommand() = default;
};

/*
 * Command Family Definitions
 */
class ExternalCommand : public Command {
private:
    JobsList *jobs;
public:
    explicit ExternalCommand(const char *cmd_line, JobsList* jobs) : Command(cmd_line), jobs(jobs) {};
    virtual ~ExternalCommand() = default;

    void execute() override;
};

class RedirectionCommand : public Command {
public:
    explicit RedirectionCommand(const char *cmd_line);
    virtual ~RedirectionCommand() = default;

    void execute() override;
};

class PipeCommand : public Command {
public:
    explicit PipeCommand(const char *cmd_line);
    virtual ~PipeCommand() = default;

    void execute() override;
};

class DiskUsageCommand : public Command {
public:
    explicit DiskUsageCommand(const char *cmd_line);
    virtual ~DiskUsageCommand() = default;

    void execute() override;
};

class WhoAmICommand : public Command {
public:
    explicit WhoAmICommand(const char *cmd_line);
    virtual ~WhoAmICommand() = default;

    void execute() override;
};

class NetInfo : public Command {
public:
    explicit NetInfo(const char *cmd_line);
    virtual ~NetInfo() = default;

    void execute() override;
};

/*
 * Built-In Commands Definitions
 */
class ChPromptCommand : public BuiltInCommand {
public:
    explicit ChPromptCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
    virtual ~ChPromptCommand() = default;

    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
public:
    explicit ChangeDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
    virtual ~ChangeDirCommand() = default;

    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    explicit GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
    virtual ~GetCurrDirCommand() = default;

    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    explicit ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
    virtual ~ShowPidCommand() = default;

    void execute() override;
};

class JobsList;

class QuitCommand : public BuiltInCommand {
    // TODO: Add your data members public:
    QuitCommand(const char *cmd_line, JobsList *jobs);

    virtual ~QuitCommand() {
    }

    void execute() override;
};

class JobsCommand : public BuiltInCommand {
private:
    JobsList *jobs;

public:
    JobsCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs) {};
    virtual ~JobsCommand() = default;

    void execute() override;
};

class QuitCommand : public BuiltInCommand {
private:
    JobsList *jobs;

public:
    QuitCommand(const char *cmd_line, JobsList *jobs);
    virtual ~QuitCommand() = default;

    void execute() override;
};

class KillCommand : public BuiltInCommand {
private:
    JobsList *jobs;

public:
    KillCommand(const char *cmd_line, JobsList *jobs);
    virtual ~KillCommand() = default;

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
private:
    JobsList *jobs;

public:
    ForegroundCommand(const char *cmd_line, JobsList *jobs);
    virtual ~ForegroundCommand() = default;

    void execute() override;
};

/*
 * Alias and Environment Commands
 */
class AliasCommand : public BuiltInCommand {
private:
std::map<std::string, std::string>& aliasMap;
public:
    AliasCommand(const char* cmd_line, std::map<std::string, std::string>& aliasMap)
	: BuiltInCommand(cmd_line), aliasMap(aliasMap) {}
    virtual ~AliasCommand() {}

    void execute() override;
};

class UnAliasCommand : public BuiltInCommand {
public:
    explicit UnAliasCommand(const char *cmd_line);
    virtual ~UnAliasCommand() = default;

    void execute() override;
};

class UnSetEnvCommand : public BuiltInCommand {
public:
    explicit UnSetEnvCommand(const char *cmd_line);
    virtual ~UnSetEnvCommand() = default;

    void execute() override;
};

class WatchProcCommand : public BuiltInCommand {
public:
    explicit WatchProcCommand(const char *cmd_line);
    virtual ~WatchProcCommand() = default;

    void execute() override;
};

/*
 * SmallShell Singleton Class
 */
class SmallShell {
private:
    string prompt;
    string lastWorkingDir;
    string prevWorkingDir;
    JobsList* jobs;

    map<string, string> aliasMap;
    SmallShell();

public:
    SmallShell(SmallShell const &) = delete;
    void operator=(SmallShell const &) = delete;

    static SmallShell &getInstance() {
        static SmallShell instance;
        return instance;
    }

    ~SmallShell();

    Command *CreateCommand(const char *cmd_line);
    void executeCommand(const char *cmd_line);

    void setPrompt(const string &newPrompt) { prompt = newPrompt; }
    string getPrompt() const { return prompt; }

    string getLastDir() const;
    void setLastDir(const string &dir);

    JobsList &getJobsList() { return *jobs; }
    void setAlias(const std::string& aliasName, const std::string& aliasCommand);
    void removeAlias(const std::string& aliasName);
    std::string getAlias(const std::string& aliasName) const;
    void printAliases() const;
};

#endif // SMASH_COMMAND_H_
