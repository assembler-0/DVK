// ProjectCloner.cpp
#include "ProjectCloner.hpp"
#include "print.hpp"
#include "execute.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <fstream> // For permission check
#include <utility>

const std::vector<std::string> m_excludePatterns = {
    "build", "Build", "cmake-build-*", "out", "bin", "obj", "node_modules",
    "__pycache__", ".pytest_cache", "target", "dist", ".git", ".svn", ".hg",
    ".vscode", ".idea", "*.swp", "*.swo", "*~", "*.o", "*.obj", "*.exe",
    "*.dll", "*.so", "*.dylib", "*.class", "*.pyc", "*.pyo", ".tmp", "*.tmp",
    "*.temp", ".DS_Store", "Thumbs.db", "*.log", "logs"
};

ProjectCloner::ProjectCloner(const int argc, char* argv[], std::string  command_name)
    : m_argc(argc), m_argv(argv), m_commandName(std::move(command_name)) {}


void ProjectCloner::run() {
    parseArguments();

    m_currentPath = std::filesystem::current_path();
    m_parentPath = m_currentPath.parent_path();
    m_sourceDirName = m_currentPath.filename().string();

    validateEnvironment();

    prepareBackupDestination();

    performBackup();

    printFinalSummary();
}

bool ProjectCloner::parseArguments() {
    const std::vector<std::string> args(m_argv + 1, m_argv + m_argc);
    std::string suffix_arg;

    for (const auto& arg : args) {
        if (arg == "-h" || arg == "--help") {
            showUsage();
            return false;
        }
        if (arg == "-c" || arg == "--compress") {
            m_compress = true;
        } else if (arg.rfind('-', 0) == 0) {
            print::error("Unknown option: {}", arg);
            showUsage();
            return false;
        } else {
            if (!suffix_arg.empty()) {
                print::error("Too many arguments. Only one suffix is allowed.");
                showUsage();
                return false;
            }
            suffix_arg = arg;
        }
    }

    if (suffix_arg.empty()) {
        const auto now = std::chrono::system_clock::now();
        const auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << "clean_" << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S");
        m_suffix = ss.str();
    } else {
        m_suffix = suffix_arg;
    }

    return true;
}

void ProjectCloner::validateEnvironment() const {
    // Check if we're not in root directory
    if (m_currentPath == m_currentPath.root_path()) {
        print::error("Cannot backup root directory.");
    }

    // Check if parent directory is writable by trying to create a temp file
    const auto temp_file_path = m_parentPath / ".projclone_writetest";
    std::ofstream test_file(temp_file_path);
    if (!test_file.is_open()) {
        print::error("Cannot write to parent directory '{}'", m_parentPath.string());
    }
    test_file.close();
    std::filesystem::remove(temp_file_path);

}

bool ProjectCloner::prepareBackupDestination() {
    std::string backup_name = m_sourceDirName + "_" + m_suffix;
    if (m_compress) {
        backup_name += ".tar.gz";
    }
    m_backupPath = m_parentPath / backup_name;

    if (std::filesystem::exists(m_backupPath)) {
        print::warn("Backup '{}'", m_backupPath.filename().string());
        std::cout << "Overwrite? (y/N): ";
        char response;
        std::cin >> response;
        if (response != 'y' && response != 'Y') {
            print::info("Backup cancelled.");
            return false;
        }
        print::info("Removing existing backup...");
        std::error_code ec;
        std::filesystem::remove_all(m_backupPath, ec);
        if (ec) {
            print::error("Failed to remove existing backup: {}", ec.message());
            return false;
        }
    }
    return true;
}

void ProjectCloner::performBackup() const {
    print::info("Creating clean backup: {}", m_backupPath.filename().string());
    print::info("Source: {}", m_currentPath.string());
    print::info("Target: {}", m_backupPath.string());

    if (m_compress) {
        return performTarBackup();
    }
    return performRsyncBackup();
}

void ProjectCloner::performRsyncBackup() const {
    print::info("Using rsync for clean copy...");
    std::stringstream cmd_ss;
    cmd_ss << "rsync -av ";
    for (const auto& pattern : m_excludePatterns) {
        cmd_ss << "--exclude='" << pattern << "' ";
    }
    // Add single quotes for robustness with paths containing spaces
    cmd_ss << "'" << m_currentPath.string() << "/' '" << m_backupPath.string() << "/'";
    if (execute(cmd_ss.str()).exit_code != 0) {
        print::error("Failed to execute command");
    }
}

void ProjectCloner::performTarBackup() const {
    print::info("Using tar for compressed archive...");
    std::stringstream cmd_ss;
    // -c: create, -z: gzip, -v: verbose, -f: file
    cmd_ss << "tar -czvf ";
    // Output file (the archive)
    cmd_ss << "'" << m_backupPath.string() << "' ";
    // Add exclude patterns
    for (const auto& pattern : m_excludePatterns) {
        cmd_ss << "--exclude='" << pattern << "' ";
    }
    cmd_ss << "-C '" << m_parentPath.string() << "' '" << m_sourceDirName << "'";
    if (execute(cmd_ss.str()).exit_code != 0) {
        print::error("Failed to execute command");
    }
}

void ProjectCloner::printFinalSummary() const {
    if (!std::filesystem::exists(m_backupPath)) {
        print::error("Verification failed: Backup file/directory not found.");
        return;
    }

    std::error_code ec;
    const uintmax_t size_bytes = m_compress ?
        std::filesystem::file_size(m_backupPath, ec) :
        0; // Calculating dir size is slow, we'll skip for rsync

    std::string size_str;
    if (!ec && size_bytes > 0) {
        size_str = " (" + std::to_string(size_bytes / 1024) + " KB)";
    }

    print::success("Clean backup created successfully {}", size_str);
    print::info("  Excluded: build dirs, compiled files, IDE configs, etc.");

    const auto rel_path = std::filesystem::relative(m_backupPath, m_currentPath);
    print::info("  Location: {}", rel_path.string());
}

void ProjectCloner::showUsage() const {
    std::cout << "Usage: dvk " << m_commandName << " [-c] [suffix]" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Backs up current directory excluding build/temp files." << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -c, --compress    Create a compressed .tar.gz archive instead of a directory copy." << std::endl;
    std::cout << "  -h, --help        Show this help message." << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  dvk " << m_commandName << "                    # Creates clean backup with timestamp" << std::endl;
    std::cout << "  dvk " << m_commandName << " -c                # Creates compressed backup with timestamp" << std::endl;
    std::cout << "  dvk " << m_commandName << " my-version      # Creates clean backup with custom suffix" << std::endl;
    std::cout << "  dvk " << m_commandName << " -c my-version   # Creates compressed backup with custom suffix" << std::endl;
}