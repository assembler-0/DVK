#include "ProjectCreator.hpp"
#include <iostream>
#include <vector>
#include <cstdlib> // for getenv, system
#include <algorithm> // for std::all_of
#include <cctype>    // for std::isalnum
#include <fstream>
#include <regex>
#include <stdexcept>

#include "print.hpp"
#include "input.hpp"
#include "findNreplace.hpp"
// Anonymous namespace for internal linkage (private to this file)

// --- Public Methods ---

void ProjectCreator::run() {
    try {
        print::info("DVK Project Creation Wizard");

        getProjectInfo();
        getWorkspaceDir();
        getBuildSystem();

        if (confirmSettings()) {
            print::info("Creating Project");
            createProjectStructure();

            print::info("Success!");
            print::success("Project '{}' created successfully!", m_projectName);
            print::info("Location: {}", m_projectPath.string());

        } else {
            print::warn("Cancelled.");
        }
    } catch (const std::runtime_error& e) {
        if (std::string(e.what()) == "Cancelled") {
             print::warn("Cancelled.");
        } else {
            print::error("An unexpected error occurred: {}", std::string(e.what()));
        }
    } catch (const std::exception& e) {
        print::error("An unexpected error occurred: {}", std::string(e.what()));
    }
}


fs::path ProjectCreator::expandUserPath(const std::string &path_str) {
    if (path_str.empty() || path_str[0] != '~') {
        return fs::path{path_str};
    }
    const char* home = nullptr;
    #ifdef _WIN32
        home = std::getenv("USERPROFILE");
    #else
        home = std::getenv("HOME");
    #endif
    if (home) {
        return fs::path(home) / path_str.substr(1);
    }
    return fs::path{path_str}; // Fallback
}

void ProjectCreator::getWorkspaceDir() {
    print::info("Workspace Selection");
    std::vector<fs::path> common_workspaces;
    #ifdef _WIN32
        const char* home_drive = std::getenv("HOMEDRIVE");
        const char* home_path = std::getenv("HOMEPATH");
        if (home_drive && home_path) {
            fs::path user_home = std::string(home_drive) + std::string(home_path);
            common_workspaces.push_back(user_home / "source/repos");
            common_workspaces.push_back(user_home / "Documents/projects");
        }
    #else
    if (const char* home = std::getenv("HOME")) {
            fs::path user_home(home);
            common_workspaces.push_back(user_home / "workspace");
            common_workspaces.push_back(user_home / "projects");
            common_workspaces.push_back(user_home / "code");
            common_workspaces.push_back(user_home / "dev");
        }
    #endif
    common_workspaces.push_back(fs::current_path());

    std::vector<fs::path> existing_workspaces;
    for (const auto& ws : common_workspaces) {
        if (fs::is_directory(ws)) {
             // Avoid duplicates
            bool found = false;
            for(const auto& ews : existing_workspaces) {
                if(fs::equivalent(ws, ews)) {
                    found = true;
                    break;
                }
            }
            if(!found) existing_workspaces.push_back(ws);
        }
    }

    if (!existing_workspaces.empty()) {
        print::info("Found potential workspaces: ");
        for (size_t i = 0; i < existing_workspaces.size(); ++i) {
            print::info( "{}. {} ", i + 1, existing_workspaces[i].string());
        }
        print::info( "{}. Enter custom path", existing_workspaces.size() + 1);

        while (true) {
            print::info( "Select workspace (1-{}): ", existing_workspaces.size() + 1);
            std::string choice_str = input::getInput();
            if (choice_str.empty()) continue;
            try {
                const int choice = std::stoi(choice_str);
                if (choice >= 1 && choice <= static_cast<int>(existing_workspaces.size())) {
                    m_workspacePath = existing_workspaces[choice - 1];
                    m_projectPath = m_workspacePath / m_projectName;
                    return;
                }
                if (choice == static_cast<int>(existing_workspaces.size()) + 1) {
                    break; // Fall through to custom path input
                }
            } catch (const std::invalid_argument&) { /* Fallthrough to error */ }
            print::error("Invalid choice, try again");
        }
    }

    // Custom path input
    while (true) {
        print::info( "Enter workspace path: ");
        std::string path_str = input::getInput();
        if (path_str.empty()) continue;
        fs::path custom_path = expandUserPath(path_str);
        if (fs::is_directory(custom_path)) {
            m_workspacePath = custom_path;
            m_projectPath = m_workspacePath / m_projectName;
            return;
        }
        print::error("Directory doesn't exist: {}", custom_path.string());
    }
}

bool ProjectCreator::isValidProjectName(const std::string& name) {
    if (name.empty()) return false;
    return std::ranges::all_of(name, [](const char c) {
        return std::isalnum(c) || c == '_' || c == '-';
    });
}

void ProjectCreator::getProjectInfo() {
    print::info("Project Information");
    while (true) {
        print::info( "Project name: ");
        m_projectName = input::getInput();
        if (isValidProjectName(m_projectName)) break;
        print::error("Invalid name. Use letters, numbers, hyphens, and underscores only.");
    }

    print::info( "Project types: ");
    const std::vector<std::pair<ProjectType, std::string>> types = {
        {ProjectType::C, "C project (.c files, gcc)"},
        {ProjectType::CPP, "C++ project (.cpp files, g++)"},
        {ProjectType::Mixed, "Mixed C/C++ project"},
        {ProjectType::ASM, "Assembly project (.s/.asm files) "}
    };
    for (size_t i = 0; i < types.size(); ++i) {
        print::info( " {}.{}", i + 1, types[i].second);
    }

    while (true) {
        print::info( "Select type (1-{}): ", types.size());
        std::string choice_str = input::getInput();
        if (choice_str.empty()) continue;
        try {
            if (const int choice = std::stoi(choice_str); choice >= 1 && choice <= static_cast<int>(types.size())) {
                m_projectType = types[choice - 1].first;
                return;
            }
        } catch (const std::invalid_argument&) { /* Fallthrough */ }
        print::error("Invalid choice, try again.");
    }
}

void ProjectCreator::getBuildSystem() {
    print::info("Build System");
    const std::vector<std::pair<BuildSystem, std::string>> systems = {
        {BuildSystem::Make, "Makefile (simple, traditional)"},
        {BuildSystem::CMake, "CMake (modern, cross-platform)"},
        {BuildSystem::Autocc, "autocc (minimal, fast and smart)"},
        {BuildSystem::Manual, "No build system (manual compilation)"}
    };
    for (size_t i = 0; i < systems.size(); ++i) {
        print::info( " {}.{}", i + 1, systems[i].second);
    }

    while (true) {
        print::info( "Select type (1-{}): ", systems.size());
        std::string choice_str = input::getInput();
        if (choice_str.empty()) continue;
        try {
            if (const int choice = std::stoi(choice_str); choice >= 1 && choice <= static_cast<int>(systems.size())) {
                m_buildSystem = systems[choice - 1].first;
                return;
            }
        } catch (const std::invalid_argument&) { /* Fallthrough */ }
        print::error("Invalid choice, try again.");
    }
}

bool ProjectCreator::confirmSettings() {
    print::info("Confirm (Yn)? ");
    const std::string confirm = input::getInput();
    return confirm.empty() || confirm == "y" || confirm == "Y" || confirm == "yes";
}


// 2. Project & File System Operations

void ProjectCreator::writeFile(const fs::path& path, const std::string& content) {
    std::ofstream file(path);
    if (!file) {
        throw std::runtime_error("Failed to open file for writing: " + path.string());
    }
    file << content;
}

void ProjectCreator::createProjectStructure() const {
    if (fs::exists(m_projectPath)) {
        print::warn("Directory {} exists.", m_projectPath.string());
    }
    print::info("Creating project at: {}", std::string(m_projectPath.string()));

    fs::create_directory(m_projectPath);
    const fs::path srcDir = m_projectPath / "src";
    fs::create_directory(srcDir);

    // Create main file(s)
    if (m_projectType == ProjectType::C || m_projectType == ProjectType::Mixed) {
        writeFile(srcDir / "main.c", getCMainContent());
        print::success("Created main.c");
    }
    if (m_projectType == ProjectType::CPP || m_projectType == ProjectType::Mixed) {
        writeFile(srcDir / "main.cpp", getCPPMainContent());
        print::success("Created main.cpp");
    }
    if (m_projectType == ProjectType::ASM) {
        writeFile(srcDir / "main.s", getASMMainContent());
        print::success("Created main.s");
    }

    // Create build system files
    switch (m_buildSystem) {
        case BuildSystem::Make:
            writeFile(m_projectPath / "Makefile", getMakefileContent());
            print::success("Created Makefile");
            break;
        case BuildSystem::CMake:
            writeFile(m_projectPath / "CMakeLists.txt", getCMakeContent());
            print::success("Created CMakeLists.txt");
            break;
        case BuildSystem::Autocc:
            writeFile(m_projectPath / "autocc.toml", getAutoccContent());
            print::success("Created autocc.toml");
            break;
        case BuildSystem::Manual:
        default:
            break; // Do nothing
    }

    // Create .gitignore and README
    writeFile(m_projectPath / ".gitignore", getGitignoreContent());
    print::success("Created .gitignore");
    writeFile(m_projectPath / "README.md", getReadmeContent());
    print::success("Created README.md");
}

// 3. Content Generation

std::string ProjectCreator::getCMainContent() {
    return R"(#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    printf("Hello, World!\n");
    return 0;
}
)";
}

std::string ProjectCreator::getCPPMainContent() {
    return R"(#include <iostream>

int main(int argc, char *argv[]) {
    print::info( "Hello, World!" << std::endl;
    return 0;
}
)";
}

std::string ProjectCreator::getASMMainContent() {
    return R"(.section .data
    msg: .ascii "Hello, World!\n"
    msg_len = . - msg

.section .text
    .global _start

_start:
    # write system call
    mov $1, %rax        # sys_write
    mov $1, %rdi        # stdout
    mov $msg, %rsi      # message
    mov $msg_len, %rdx  # length
    syscall

    # exit system call
    mov $60, %rax       # sys_exit
    mov $0, %rdi        # exit status
    syscall
)";
}

std::string ProjectCreator::getMakefileContent() const {
    std::string content;
    switch (m_projectType) {
        case ProjectType::C:
            content = R"(CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
TARGET = _PROJECT_NAME_
SRCDIR = src
OBJDIR = build
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

.PHONY: all clean run debug install

all: $(OBJDIR) $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET)

run: all
	./$(TARGET)

debug: CFLAGS += -DDEBUG
debug: all

install: all
	cp $(TARGET) /usr/local/bin/
)";
            break;
        case ProjectType::CPP:
            content = R"(CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -g
TARGET = _PROJECT_NAME_
SRCDIR = src
OBJDIR = build
SOURCES = $(wildcard $(SRCDIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

.PHONY: all clean run debug install

all: $(OBJDIR) $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET)

run: all
	./$(TARGET)

debug: CXXFLAGS += -DDEBUG
debug: all

install: all
	cp $(TARGET) /usr/local/bin/
)";
            break;
        case ProjectType::Mixed: // FIX: Added support for mixed projects
            content = R"(CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -std=c99 -g
CXXFLAGS = -Wall -Wextra -std=c++17 -g
TARGET = _PROJECT_NAME_
SRCDIR = src
OBJDIR = build

C_SOURCES = $(wildcard $(SRCDIR)/*.c)
CXX_SOURCES = $(wildcard $(SRCDIR)/*.cpp)
OBJECTS = $(C_SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o) $(CXX_SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

.PHONY: all clean run debug install

all: $(OBJDIR) $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET)

run: all
	./$(TARGET)

debug: CFLAGS += -DDEBUG
debug: CXXFLAGS += -DDEBUG
debug: all

install: all
	cp $(TARGET) /usr/local/bin/
)";
            break;
        case ProjectType::ASM:
            content = R"(AS = as
LD = ld
TARGET = _PROJECT_NAME_
SRCDIR = src
OBJDIR = build
SOURCES = $(wildcard $(SRCDIR)/*.s)
OBJECTS = $(SOURCES:$(SRCDIR)/%.s=$(OBJDIR)/%.o)

.PHONY: all clean run install

all: $(OBJDIR) $(TARGET)

$(TARGET): $(OBJECTS)
	$(LD) $(OBJECTS) -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.s
	$(AS) $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET)

run: all
	./$(TARGET)

install: all
	cp $(TARGET) /usr/local/bin/
)";
            break;
        default: break;
    }
    findAndReplaceAll(content, "_PROJECT_NAME_", m_projectName);
    return content;
}

std::string ProjectCreator::getCMakeContent() const {
    std::string content;
    switch (m_projectType) {
        case ProjectType::C:
            content = R"(cmake_minimum_required(VERSION 3.10)
project(_PROJECT_NAME_ C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -g")

file(GLOB SOURCES "src/*.c")

add_executable(_PROJECT_NAME_ ${SOURCES})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
)";
            break;
        case ProjectType::CPP:
             content = R"(cmake_minimum_required(VERSION 3.10)
project(_PROJECT_NAME_ CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g")

file(GLOB SOURCES "src/*.cpp")

add_executable(_PROJECT_NAME_ ${SOURCES})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
)";
            break;
        case ProjectType::Mixed: // FIX: Added support for mixed projects
            content = R"(cmake_minimum_required(VERSION 3.10)
project(_PROJECT_NAME_ C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g")

file(GLOB SOURCES "src/*.c" "src/*.cpp")

add_executable(_PROJECT_NAME_ ${SOURCES})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
)";
            break;
        default: break; // ASM isn't handled by this simple CMake template
    }
    findAndReplaceAll(content, "_PROJECT_NAME_", m_projectName);
    return content;
}

std::string ProjectCreator::getAutoccContent() const {
    std::string main_file;
    std::string sources = "[ ";

    if (m_projectType == ProjectType::C) {
        main_file = "'./src/main.c'";
        sources += "'./src/main.c' ]";
    } else if (m_projectType == ProjectType::CPP) {
        main_file = "'./src/main.cpp'";
        sources += "'./src/main.cpp' ]";
    } else if (m_projectType == ProjectType::Mixed) { // FIX: Added support for mixed projects
        main_file = "'./src/main.cpp'"; // Link with C++
        sources += "'./src/main.c', './src/main.cpp' ]";
    } else if (m_projectType == ProjectType::ASM) {
        main_file = "'./src/main.s'";
        sources += "'./src/main.s' ]";
    }

    std::string content = R"(# CONFIGURATION FILE 'autocc.toml' IS WRITTEN MANUALLY BY DVK, NOT BY AUTOCC. EDIT WITH CAUTION.
[compilers]
as = 'as'
cc = 'clang'
cxx = 'clang++'

[paths]
exclude_patterns = []
include_dirs = []

[project]
build_dir = "build"
default_target = "_PROJECT_NAME_"

[[targets]]
name = "_PROJECT_NAME_"
main_file = _MAIN_FILE_
sources = _SOURCES_
output_name = "_PROJECT_NAME_"
cflags = "-Wall -Wextra -g"
cxxflags = "-Wall -Wextra -g"
)";

    findAndReplaceAll(content, "_PROJECT_NAME_", m_projectName);
    findAndReplaceAll(content, "_MAIN_FILE_", main_file);
    findAndReplaceAll(content, "_SOURCES_", sources);
    return content;
}

std::string ProjectCreator::getGitignoreContent() {
    return R"(# Build artifacts
build/
*.o
*.obj
*.exe
*.out
a.out
_PROJECT_NAME_

# IDE files
.vscode/
.idea/
*.swp
*.swo
compile_commands.json

# System files
.DS_Store
Thumbs.db
)";
}

std::string ProjectCreator::getReadmeContent() const {
    std::string content = "# " + m_projectName + "\n\n";

    content += "A new project created with the C/C++ Project Wizard.\n\n";
    content += "## Build\n\n";

    switch (m_buildSystem) {
        case BuildSystem::Make:
            content += "```bash\nmake\n```\n";
            content += "\n## Run\n\n```bash\nmake run\n```\n";
            content += "\n## Clean\n\n```bash\nmake clean\n```\n";
            break;
        case BuildSystem::CMake:
            content += "```bash\nmkdir -p build && cd build\ncmake ..\nmake\n```\n";
            content += "\n## Run\n\n```bash\n./build/_PROJECT_NAME_\n```\n";
            break;
        case BuildSystem::Manual:
        default:
            switch(m_projectType) {
                case ProjectType::C:
                    content += "```bash\ngcc src/main.c -o _PROJECT_NAME_\n```\n";
                    break;
                case ProjectType::CPP:
                    content += "```bash\ng++ src/main.cpp -o _PROJECT_NAME_\n```\n";
                    break;
                case ProjectType::Mixed:
                    content += "```bash\ng++ src/main.c src/main.cpp -o _PROJECT_NAME_\n```\n";
                    break;
                case ProjectType::ASM:
                     content += "```bash\nas src/main.s -o main.o\nld main.o -o _PROJECT_NAME_\n```\n";
                    break;
                default: break;
            }
            content += "\n## Run\n\n```bash\n./_PROJECT_NAME_\n```\n";
            break;
    }
    findAndReplaceAll(content, "_PROJECT_NAME_", m_projectName);
    return content;
}


// 4. Utility Helpers

std::string ProjectCreator::projectTypeToString(const ProjectType type) {
    switch (type) {
        case ProjectType::C: return "C";
        case ProjectType::CPP: return "C++";
        case ProjectType::Mixed: return "Mixed C/C++";
        case ProjectType::ASM: return "Assembly";
        default: return "Unknown";
    }
}

std::string ProjectCreator::buildSystemToString(const BuildSystem system) {
    switch (system) {
        case BuildSystem::Make: return "Makefile";
        case BuildSystem::CMake: return "CMake";
        case BuildSystem::Autocc: return "autocc";
        case BuildSystem::Manual: return "Manual";
        default: return "Unknown";
    }
}

