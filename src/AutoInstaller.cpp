#include "AutoInstaller.hpp"
#include "print.hpp"
#include "execute.hpp" // Assuming your thread-safe execute function is here
#include <stdexcept>
#include <algorithm>

#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
#else
    #include <unistd.h>
    #include <sys/stat.h>
#endif

void AutoInstaller::run(const char* target, const char* flags) {
    try {
        parseArguments(target, flags);

        if (!validateSourceFile()) {
            return;
        }

        if (!checkTargetDirectory()) {
            return;
        }

        if (!performInstallation()) {
            return;
        }

        if (const fs::path targetPath = fs::path(m_targetDir) / getFilename(m_sourceFile); !verifyInstallation(targetPath)) {
            print::error("Installation validation failed.");
        }
    } catch (const std::exception& e) {
        print::error("An error occurred: {}", e.what());
    }
}

bool AutoInstaller::install(const std::string& sourceFile, const InstallMode mode, const std::string& customTargetDir) {
    m_sourceFile = sourceFile;
    m_mode = mode;

    if (!customTargetDir.empty()) {
        m_targetDir = customTargetDir;
    } else if (mode == InstallMode::Auto) {
        m_targetDir = autoDetectPath();
        if (m_targetDir.empty()) {
            return false;
        }
    }

    return validateSourceFile() &&
           checkTargetDirectory() &&
           performInstallation() &&
           verifyInstallation(fs::path(m_targetDir) / getFilename(m_sourceFile));
}

void AutoInstaller::showUsage(const std::string& programName) {
    print::info("Usage: {} <file> [--link|--auto]", programName);
    print::info("");
    print::info("Options:");
    print::info("  (default)  Copy file to /usr/local/bin");
    print::info("  --link     Create symbolic link instead of copying");
    print::info("  --auto     Auto-detect bin path using existing commands");
    print::info("");
    print::info("Examples:");
    print::info("  {} myscript              # Copy to /usr/local/bin/myscript", programName);
    print::info("  {} myscript --link       # Link to /usr/local/bin/myscript", programName);
    print::info("  {} myscript --auto       # Auto-detect path and copy", programName);
}

void AutoInstaller::parseArguments(const char* target, const char* flags) {
    if (target != nullptr) {
        m_sourceFile = std::string(target);
    }
    if (!fs::exists(m_sourceFile)) {
        print::error("Source {} not found", m_sourceFile);
        throw std::runtime_error("Source not found");
    }
    if (flags != nullptr) {
        if (std::string(flags) == "--link") {
            m_mode = InstallMode::Link;
        } else if (std::string(flags) == "--auto") {
            m_mode = InstallMode::Auto;
            m_targetDir = autoDetectPath();
            if (m_targetDir.empty()) {
                throw std::runtime_error("Could not auto-detect suitable bin directory");
            }
        } else {
            print::error("Unknown option '{}'", flags);
            showUsage("autoinstall");
            throw std::runtime_error("Invalid option");
        }
    }
}

std::string AutoInstaller::autoDetectPath() {
    for (const std::vector<std::string> testCommands = {"ls", "cat", "echo", "sh", "which"}; const auto& cmd : testCommands) {
        if (CommandResult result = execute(fmt::format("which {}", cmd)); result.exit_code == 0 && !result.stdout_output.empty()) {
            // Remove newline if present
            std::erase(result.stdout_output, '\n');

            if (fs::path cmdPath(result.stdout_output); fs::exists(cmdPath) && fs::is_regular_file(cmdPath)) {
                if (fs::path binDir = cmdPath.parent_path(); hasWritePermission(binDir) || isRoot()) {
                    return binDir.string();
                }
            }
        }
    }

    // Fallback paths

    for (const std::vector<std::string> fallbackPaths = {"/usr/local/bin", "/usr/bin", "/bin"}; const auto& path : fallbackPaths) {
        if (fs::path pathObj(path); fs::is_directory(pathObj) && (hasWritePermission(pathObj) || isRoot())) {
            return path;
        }
    }

    print::error("Could not find a suitable bin directory");
    return "";
}

bool AutoInstaller::validateSourceFile() const {
    if (!fileExists(m_sourceFile)) {
        print::error("Source file '{}' does not exist", m_sourceFile);
        return false;
    }

    return true;
}

bool AutoInstaller::makeExecutable(const fs::path& filePath) {
#ifdef _WIN32
    // Windows doesn't have the same executable permission concept
    return true;
#else
    struct stat st{};
    if (stat(filePath.c_str(), &st) != 0) {
        return false;
    }

    if (!(st.st_mode & S_IXUSR)) {
        print::info("Making '{}' executable...", filePath.string());
        if (chmod(filePath.c_str(), st.st_mode | S_IXUSR | S_IXGRP | S_IXOTH) != 0) {
            print::error("Failed to make file executable");
            return false;
        }
    }
    return true;
#endif
}

bool AutoInstaller::checkTargetDirectory() const {
    if (!fs::is_directory(m_targetDir)) {
        print::error("Target directory '{}' does not exist", m_targetDir);
        return false;
    }

    if (!hasWritePermission(m_targetDir) && !isRoot()) {
        print::error("No write permission to '{}'. Try running with sudo.", m_targetDir);
        return false;
    }

    return true;
}

bool AutoInstaller::performInstallation() const {
    const fs::path sourcePath = getAbsolutePath(m_sourceFile);
    const fs::path targetPath = fs::path(m_targetDir) / getFilename(m_sourceFile);

    // Make source executable if it isn't already
    if (!makeExecutable(sourcePath)) {
        return false;
    }

    // Remove existing target if it exists
    if (!removeExisting(targetPath)) {
        return false;
    }

    std::error_code ec;

    switch (m_mode) {
        case InstallMode::Copy:
        case InstallMode::Auto:
            print::info("Copying '{}' to '{}'...", sourcePath.string(), targetPath.string());
            fs::copy_file(sourcePath, targetPath, ec);
            if (ec) {
                print::error("Failed to copy file: {}", ec.message());
                return false;
            }
            if (!makeExecutable(targetPath)) {
                return false;
            }
            break;

        case InstallMode::Link:
            print::info("Creating symbolic link '{}' -> '{}'...", targetPath.string(), sourcePath.string());
            fs::create_symlink(sourcePath, targetPath, ec);
            if (ec) {
                print::error("Failed to create symbolic link: {}", ec.message());
                return false;
            }
            break;
    }

    print::success("Successfully installed '{}' to '{}'", getFilename(m_sourceFile), m_targetDir);
    return true;
}

bool AutoInstaller::verifyInstallation(const fs::path& targetPath) const {
    if (!fs::exists(targetPath)) {
        print::error("Installation failed - target does not exist");
        return false;
    }

#ifndef _WIN32
    if (!isCommandExecutable(targetPath)) {
        print::error("Installation failed - target is not executable");
        return false;
    }
#endif

    print::success("Installation verified: '{}' is executable", getFilename(m_sourceFile));

    return true;
}

bool AutoInstaller::hasWritePermission(const fs::path& directory) {
#ifdef _WIN32
    // Simplified Windows check
    return _access(directory.string().c_str(), 2) == 0;
#else
    return access(directory.c_str(), W_OK) == 0;
#endif
}

bool AutoInstaller::isRoot() {
#ifdef _WIN32
    // On Windows, check if running as administrator
    return false; // Simplified - would need more complex check
#else
    return getuid() == 0;
#endif
}

fs::path AutoInstaller::getAbsolutePath(const std::string& path) {
    std::error_code ec;
    fs::path absPath = fs::absolute(path, ec);
    if (ec) {
        return fs::path{path};
    }
    return absPath;
}

std::string AutoInstaller::getFilename(const std::string& path) {
    return fs::path(path).filename().string();
}

bool AutoInstaller::fileExists(const fs::path& path) {
    return fs::exists(path) && fs::is_regular_file(path);
}

bool AutoInstaller::removeExisting(const fs::path& path) {
    if (fs::exists(path) || fs::is_symlink(path)) {
        print::info("Removing existing '{}'...", path.string());
        std::error_code ec;
        fs::remove(path, ec);
        if (ec) {
            print::error("Failed to remove existing file: {}", ec.message());
            return false;
        }
    }
    return true;
}