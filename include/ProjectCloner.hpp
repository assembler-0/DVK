// ProjectCloner.h
#ifndef PROJECT_CLONER_H
#define PROJECT_CLONER_H

#include <string>
#include <vector>
#include <filesystem>

class ProjectCloner {
public:
    // Takes command-line arguments to configure the backup operation.
    ProjectCloner(int argc, char* argv[], std::string  command_name);

    // Executes the entire backup process. Returns true on success, false on failure.
    void run();

private:
    // --- Helper Methods ---
    void showUsage() const;
    bool parseArguments();
    void validateEnvironment() const;
    bool prepareBackupDestination();
    void performBackup() const;
    void performRsyncBackup() const;
    void performTarBackup() const;
    void printFinalSummary() const;

    // --- Member Variables ---
    int m_argc;
    char** m_argv;

    // Configuration from args
    std::string m_suffix;
    bool m_compress = false;

    // Path information
    std::filesystem::path m_currentPath;
    std::filesystem::path m_parentPath;
    std::filesystem::path m_backupPath;
    std::string m_sourceDirName;
    std::string m_commandName;

};

#endif // PROJECT_CLONER_H