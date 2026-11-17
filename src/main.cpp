#include <chrono>
#include <iostream>
#include <sstream>
#include <string>

#include "medsync/Config.hpp"
#include "medsync/DeviceController.hpp"
#include "medsync/Logger.hpp"

namespace {

#ifndef MEDSYNC_VERSION
#define MEDSYNC_VERSION "0.0.0"
#endif

constexpr const char* kVersion = MEDSYNC_VERSION;

void printBanner() {
    std::cout << "\nMedSync v" << kVersion << '\n'
              << "Educational robotic medical device simulation.\n"
              << "Software simulation only - not a medical device and not "
                 "clinically validated.\n\n";
}

void printHelp() {
    std::cout
        << "Commands:\n"
        << "  home                 Home the stage (Idle -> Ready)\n"
        << "  load <id> <x> <y>    Load a sample at a target position\n"
        << "  run                  Start positioning and scanning the loaded sample\n"
        << "  wait                 Wait up to 5 s for the current cycle to finish\n"
        << "  status               Show device state, position, and sample\n"
        << "  estop                Latch an emergency stop (always accepted)\n"
        << "  release              Clear the emergency stop cause\n"
        << "  reset                Reset after Fault or EmergencyStop\n"
        << "  help                 Show this list\n"
        << "  quit                 Shut down and exit\n\n";
}

void printError(const std::string& error) {
    std::cout << "error: " << error << '\n';
}

// Parses exactly two integers and rejects trailing tokens.
bool parsePosition(std::istringstream& input, medsync::Position& position) {
    int x = 0;
    int y = 0;
    if (!(input >> x >> y)) {
        return false;
    }

    std::string trailing;
    if (input >> trailing) {
        return false;
    }

    position = medsync::Position{x, y};
    return true;
}

} // namespace

int main() {
    medsync::Config config;
    std::string error;
    if (!config.validate(error)) {
        std::cerr << "invalid configuration: " << error << '\n';
        return 1;
    }

    printBanner();

    medsync::Logger logger(std::cout);
    medsync::DeviceController controller(config, logger);
    if (!controller.initialise(error)) {
        std::cerr << "failed to initialise device: " << error << '\n';
        return 1;
    }

    printHelp();

    bool running = true;
    while (running) {
        std::cout << "medsync> " << std::flush;

        std::string line;
        if (!std::getline(std::cin, line)) {
            std::cout << '\n';
            break;
        }

        std::istringstream input(line);
        std::string command;
        if (!(input >> command)) {
            continue;
        }

        if (command == "help") {
            printHelp();
        } else if (command == "home") {
            if (!controller.home(error)) {
                printError(error);
            }
        } else if (command == "load") {
            std::string id;
            medsync::Position target;
            if (!(input >> id) || !parsePosition(input, target)) {
                printError("usage: load <id> <x> <y>");
                continue;
            }
            if (!controller.loadSample(id, target, error)) {
                printError(error);
            }
        } else if (command == "run") {
            if (controller.runCycle(error)) {
                std::cout << "cycle started\n";
            } else {
                printError(error);
            }
        } else if (command == "wait") {
            if (controller.waitForCycleCompletion(std::chrono::milliseconds(5000))) {
                std::cout << "cycle complete\n";
            } else {
                printError("cycle did not complete within 5 s");
            }
        } else if (command == "status") {
            std::cout << controller.statusText() << '\n';
        } else if (command == "estop") {
            if (!controller.emergencyStop(error)) {
                printError(error);
            }
        } else if (command == "release") {
            if (!controller.releaseEmergencyStop(error)) {
                printError(error);
            }
        } else if (command == "reset") {
            if (!controller.reset(error)) {
                printError(error);
            }
        } else if (command == "quit" || command == "exit") {
            running = false;
        } else {
            printError("unknown command '" + command + "'; type help");
        }
    }

    if (!controller.shutdown(error)) {
        std::cerr << "shutdown error: " << error << '\n';
    }
    std::cout << "MedSync stopped.\n";
    return 0;
}
