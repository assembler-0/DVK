#pragma once

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

enum class InstallMode {
    Copy,
    Link,
    Auto
};

class AutoInstaller {
public:
    // Constructor
    AutoInstaller() = default;

    // Main interface
    void run(const char* target, const char* flags);

    // Individual operations
    bool install(const std::string& sourceFile, InstallMode mode = InstallMode::Copy, const std::string& customTargetDir = "");

private:
    // Configuration
    std::string m_sourceFile;
    InstallMode m_mode = InstallMode::Copy;
    std::string m_targetDir = "/usr/local/bin";

    // Internal methods
    static void showUsage(const std::string& programName) ;

    void parseArguments(const char* target, const char* flags);
    [[nodiscard]] static std::string autoDetectPath() ;
    [[nodiscard]] bool validateSourceFile() const;
    [[nodiscard]] static bool makeExecutable(const fs::path& filePath) ;
    [[nodiscard]] bool checkTargetDirectory() const;
    [[nodiscard]] bool performInstallation() const;
    [[nodiscard]] bool verifyInstallation(const fs::path& targetPath) const;

    static bool hasWritePermission(const fs::path& directory);
    [[nodiscard]] static bool isRoot();

    // Helper methods
    [[nodiscard]] static fs::path getAbsolutePath(const std::string& path);
    [[nodiscard]] static std::string getFilename(const std::string& path);
    [[nodiscard]] static bool fileExists(const fs::path& path);
    [[nodiscard]] static bool removeExisting(const fs::path& path);
};