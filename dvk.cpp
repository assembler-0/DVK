#include <fmt/base.h>
#include "ProjectCreator.hpp"
#include "AutoInstaller.hpp"
#include "ProjectCloner.hpp"
#include "print.hpp"

#define TIME __TIME__
#define DATE __DATE__

std::mutex g_output_mutex;
static constexpr auto PROJCLONE_PATH = "./scripts/projclone";
class DVK {
public:
    static int start(const int argc, char * argv[]) {
        const std::string cmd = argv[1];
        if (cmd == "create") {
            try {
                ProjectCreator wizard;
                wizard.run();
            } catch (...) {
                print::error("A critical error has occurred.");
                return 1;
            }
            return 0;
        }
        if (cmd == "install") {
            try {
                AutoInstaller install;
                install.run(argv[2], argv[3]);
            } catch (...) {
                print::error("A critical error has occurred.");
                return 1;
            }
        }
        if (cmd == "clone") {
            try {
                ProjectCloner clone(argc, argv, "clone");
                clone.run();
            } catch (...) {
                print::error("A critical error has occurred.");
                return 1;
            }
        }
        if (cmd == "help") {
            try {
                help();
            } catch (...) {
                print::error("A critical error has occurred.");
                return 1;
            }
        }
        if (cmd == "bootstrap") {
            try {
                print::warn("You might need to use sudo.");
                const auto path = PROJCLONE_PATH;
                if (!fs::exists(path)) {
                    print::error("Script path not found, are you in DVK project root?");
                    return 1;
                }
                AutoInstaller install;
                install.install(path, InstallMode::Copy, "/usr/local/bin");
            } catch (...) {
                print::error("A critical error has occurred.");
                return 1;
            }
        }
        return 0;
    }
    static void help() {
        print::info("DVK v0.0.1 compile on {} at {}.", DATE, TIME);
        print::info("Commands:");
        print::info("\t install");
        print::info("\t create");
    }
};

int main(const int argc, char * argv[]) {
    if (argc < 2) {
        DVK::help();
        return 0;
    }
    if (const int ec = DVK::start(argc, argv) != 0) {
        print::error("dvk.start() failed with exitcode {}", ec);
        return ec;
    }
    return 0;
}