// Ver: 10-4-2025
#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <string>
#include <vector>
#include <memory>
#include <sys/wait.h>
#include <map>



#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

using namespace std;

/*
 * Command Class Definition
 */
class Command {
protected:
    string cmd_line;
    string cmd_line_unedited;
    vector<string> args;
    bool is_background;
    string alias;

public:
    explicit Command(const char *cmd_line);
    virtual ~Command() = default;

    virtual void execute() = 0;
    const string &getCmdLine() const { return cmd_line; }
    const vector<string> &getArgs() const { return args; }
    bool isBackground() const { return is_background; }
    string getAlias() const {
        return alias;
    }
    void setAlias(const string& aliasCommand) {
        alias = aliasCommand;
    }
};

/*
 * JobsList Class
 * 
 * This class manages a list of jobs (processes) running in the background.
 * It provides functionality to add, remove, and retrieve jobs, as well as
 * to clean up finished jobs and print the current job list.
 */
class JobsList {
public:
    /*
     * JobEntry Class
     * Represents a single job entry with its ID, process ID, and command line.
     */
    class JobEntry {
        int jobId;
        pid_t pid;
        string cmdLine;

    public:
        JobEntry(int jobId, pid_t pid, const string& cmdLine)
            : jobId(jobId), pid(pid), cmdLine(cmdLine) {}

        int getJobId() const { return jobId; }
        pid_t getPid() const { return pid; }
        const string& getCmdLine() const { return cmdLine; }
    };

private:
    vector<JobEntry*> jobs; // List of job entries

public:
    
    JobsList() {}

    ~JobsList() {
        for (JobEntry* job : jobs) {
            delete job;
        }
    }

    /*
     * Adds a new job to the list.
     * Removes finished jobs before adding the new job.
     * 
     * Parameters:
     * - cmdLine: The command line of the job.
     * - pid: The process ID of the job.
     */
    void addJob(string cmdLine, pid_t pid) {
        removeFinishedJobs();
        int jobId = getLargestJobId() + 1;
        jobs.push_back(new JobEntry(jobId, pid, cmdLine));
    }

    /*
     * Prints the list of jobs to the standard output.
     */
    void printJobsList() const {
        for (const JobEntry* job : jobs) {
            cout << "[" << job->getJobId() << "] " << job->getCmdLine() << endl;
        }
    }

    /*
     * Removes finished jobs from the list.
     * Uses waitpid with WNOHANG to check if jobs have finished.
     */
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

    /*
     * Retrieves a job by its ID.
     * 
     * Parameters:
     * - jobId: The ID of the job to retrieve.
     * 
     * Returns:
     * - A pointer to the JobEntry if found, or nullptr if not found.
     */
    JobEntry* getJobById(int jobId) {
        for (JobEntry* job : jobs) {
            if (job->getJobId() == jobId) {
                return job;
            }
        }
        return nullptr;
    }

    /*
     * Removes a job from the list by its ID.
     * 
     * Parameters:
     * - jobId: The ID of the job to remove.
     */
    void removeJobById(int jobId) {
        for (auto it = jobs.begin(); it != jobs.end(); ++it) {
            if ((*it)->getJobId() == jobId) {
                delete *it;
                jobs.erase(it);
                break;
            }
        }
    }

    /*
     * Retrieves the largest job ID in the list.
     * 
     * Returns:
     * - The largest job ID, or 0 if the list is empty.
     */
    int getLargestJobId() {
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

    bool isEmpty() const {
        return jobs.empty();
    }

    const vector<JobEntry*>& getJobs() const { return jobs; }

    /*
     * Clears all jobs from the list.
     * Deletes all dynamically allocated JobEntry objects.
     */
    void clearJobs() {
        for (JobEntry* job : jobs) {
            delete job;
        }
        jobs.clear();
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
protected:
    JobsList &jobs;

public:
    explicit ExternalCommand(const char *cmd_line, JobsList& jobs) : Command(cmd_line), jobs(jobs) {};
    virtual ~ExternalCommand() = default;

};

class SimpleExternalCommand : public ExternalCommand {
public:
    explicit SimpleExternalCommand(const char *cmd_line, JobsList& jobs) : ExternalCommand(cmd_line, jobs) {};
    virtual ~SimpleExternalCommand() = default;

    void execute() override;
};

class ComplexExternalCommand : public ExternalCommand {
public:
    explicit ComplexExternalCommand(const char *cmd_line, JobsList& jobs) : ExternalCommand(cmd_line, jobs) {};
    virtual ~ComplexExternalCommand() = default;

    void execute() override;
};

class RedirectionCommand : public Command {
public:
    explicit RedirectionCommand(const char *cmd_line) : Command(cmd_line) {};
    virtual ~RedirectionCommand() = default;

    void execute() override;
};

class PipeCommand : public Command {
public:
    explicit PipeCommand(const char *cmd_line) : Command(cmd_line) {};
    virtual ~PipeCommand() = default;

    void execute() override;
};

class DiskUsageCommand : public Command {
private:
    long long calculateDiskUsage(const char* path);

public:
    explicit DiskUsageCommand(const char *cmd_line) : Command(cmd_line) {};
    virtual ~DiskUsageCommand() = default;

    void execute() override;
};

class WhoAmICommand : public Command {
public:
    explicit WhoAmICommand(const char *cmd_line) : Command(cmd_line) {};
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

class JobsCommand : public BuiltInCommand {
private:
    JobsList& jobs;

public:
    JobsCommand(const char *cmd_line, JobsList& jobs) : BuiltInCommand(cmd_line), jobs(jobs) {};
    virtual ~JobsCommand() = default;

    void execute() override;
};

class QuitCommand : public BuiltInCommand {
private:
    JobsList &jobs;

public:
    QuitCommand(const char *cmd_line, JobsList &jobs) : BuiltInCommand(cmd_line), jobs(jobs) {};
    virtual ~QuitCommand() = default;

    void execute() override;
};

class KillCommand : public BuiltInCommand {
private:
    JobsList &jobs;

public:
    KillCommand(const char *cmd_line, JobsList &jobs) : BuiltInCommand(cmd_line), jobs(jobs) {
        jobs.removeFinishedJobs();
    };
    virtual ~KillCommand() = default;

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
private:
    JobsList &jobs;
    int jobId;

public:
    ForegroundCommand(const char *cmd_line, JobsList &jobs) : BuiltInCommand(cmd_line), jobs(jobs) {
        jobs.removeFinishedJobs();
        jobId = jobs.getLargestJobId();
    };
    ForegroundCommand(const char *cmd_line, JobsList &jobs, int jobId ) : BuiltInCommand(cmd_line), jobs(jobs), jobId(jobId) {}
    virtual ~ForegroundCommand() = default;

    void execute() override;
};

/*
 * Alias and Environment Commands
 */
class AliasCommand : public BuiltInCommand {
private:
    map<string, string>& aliasMap;

public:
    AliasCommand(const char* cmd_line, map<string, string>& aliasMap)
        : BuiltInCommand(cmd_line), aliasMap(aliasMap) {}
    virtual ~AliasCommand() {}

    void execute() override;
};

class UnAliasCommand : public BuiltInCommand {
private:
    map<string, string>& aliasMap;

public:
    UnAliasCommand(const char *cmd_line, map<string, string>& aliasMap)
        : BuiltInCommand(cmd_line), aliasMap(aliasMap) {}
    virtual ~UnAliasCommand() = default;

    void execute() override;
};

class UnSetEnvCommand : public BuiltInCommand {
public:
    explicit UnSetEnvCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
    virtual ~UnSetEnvCommand() = default;

    void execute() override;
};

class WatchProcCommand : public BuiltInCommand {
public:
    explicit WatchProcCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {};
    virtual ~WatchProcCommand() = default;

    void execute() override;

private:
    bool readCpuTimes(pid_t pid, long long &proc_time, long long &total_time);
    double readMemoryUsage(pid_t pid);
};

/*
 * SmallShell Singleton Class
 */
class SmallShell {
private:
    pid_t foreground_pid;
    bool is_foreground_running;
    string prompt;
    string lastWorkingDir;
    string prevWorkingDir;
    JobsList jobs;
    map<string, string> aliasMap;

    SmallShell();

public:
    SmallShell(SmallShell const &) = delete;
    void operator=(SmallShell const &) = delete;

    static SmallShell &getInstance() {
        static SmallShell instance;
        return instance;
    }

    ~SmallShell() = default;

    Command *CreateCommand(const char *cmd_line);
    void executeCommand(const char *cmd_line);

    void setPrompt(const string &newPrompt) { prompt = newPrompt; }
    string getPrompt() const { return prompt; }

    string getLastDir() const {
        return lastWorkingDir;
    }
    void setLastDir(const string &dir) {
        lastWorkingDir = dir;
    }

    JobsList &getJobsList() { return jobs; }

    void setAlias(const string& aliasName, const string& aliasCommand);
    void removeAlias(const string& aliasName);
    string getAlias(const string& aliasName) const;
    void printAliases() const;

    void setForegroundPid(pid_t pid) {
        foreground_pid = pid;
        is_foreground_running = true;
    }
    pid_t getForegroundPid() const {
        return is_foreground_running ? foreground_pid : -1;
    }
    void clearForegroundPid() {
        foreground_pid = -1;
        is_foreground_running = false;
    }

};

#endif // SMASH_COMMAND_H_
