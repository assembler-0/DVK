#pragma once
#ifndef PROJECT_CREATOR_H
#define PROJECT_CREATOR_H

#include <string>
#include <filesystem>

// C++17 filesystem alias
namespace fs = std::filesystem;

class ProjectCreator {
public:
    /**
     * @brief Runs the interactive project creation wizard.
     */
    void run();

private:
    // Enums for strongly-typed choices
    enum class ProjectType { C, CPP, Mixed, ASM, Unknown };
    enum class BuildSystem { Make, CMake, Autocc, Manual, Unknown };

    // Project properties stored as member variables
    std::string m_projectName;
    ProjectType m_projectType = ProjectType::Unknown;
    BuildSystem m_buildSystem = BuildSystem::Unknown;
    fs::path m_workspacePath;
    fs::path m_projectPath;

    // --- Private Helper Functions ---

    // 1. User Interaction & Input
    void getWorkspaceDir();
    void getProjectInfo();
    void getBuildSystem();

    static bool confirmSettings();

    // 2. Project & File System Operations
    void createProjectStructure() const;
    static void writeFile(const fs::path& path, const std::string& content);

    // 3. Content Generation
    [[nodiscard]] static std::string getCMainContent() ;
    [[nodiscard]] static std::string getCPPMainContent() ;
    [[nodiscard]] static std::string getASMMainContent() ;
    [[nodiscard]] std::string getMakefileContent() const;
    [[nodiscard]] std::string getAutoccContent() const;
    [[nodiscard]] std::string getCMakeContent() const;
    [[nodiscard]] static std::string getGitignoreContent() ;
    [[nodiscard]] std::string getReadmeContent() const;

    // 4. Utility Helpers
    [[nodiscard]] static fs::path expandUserPath(const std::string &path_str);
    [[nodiscard]] static bool isValidProjectName(const std::string& name) ;
    [[nodiscard]] static std::string projectTypeToString(ProjectType type) ;
    [[nodiscard]] static std::string buildSystemToString(BuildSystem system);

};

#endif // PROJECT_CREATOR_H