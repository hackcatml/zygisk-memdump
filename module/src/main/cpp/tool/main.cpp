#include <unistd.h>
#include <getopt.h>
#include <iostream>
#include <fstream>

#include "logcat.h"
#include "nlohmann/json.hpp"

using namespace std;
using json = nlohmann::json;

const char* short_options = "hcbwp:l:r:d:";
const struct option long_options[] = {
        {"help", no_argument, nullptr, 'h'},
        {"config", no_argument, nullptr, 'c'},
        {"block", no_argument, nullptr, 'b'},
        {"watch", no_argument, nullptr, 'w'},
        {"package", required_argument, nullptr, 'p'},
        {"lib", required_argument, nullptr, 'l'},
        {"regex", required_argument, nullptr, 'r'},
        {"delay", required_argument, nullptr, 'd'},
        {"delay-section", required_argument, nullptr, 111},
        {"onload", no_argument, nullptr, 222},
        {nullptr, 0, nullptr, 0}
};

void show_usage() {
    printf("Usage: ./zygisk-memdump -p <packageName> <option(s)>\n");
    printf(" Options:\n");
    printf("  -l --lib <library_name>               Library name to dump\n");
    printf("  -r --regex \"<expression>\"               Regex expression matching the library name to dump\n");
    printf("  -d --delay <microseconds>             Delay in microseconds before dumping the library (cannot be used with the --delay-section option)\n");
    printf("  --delay-section <microseconds>        Delay in microseconds before dumping the library's section (e.g., .text, il2cpp, .rodata)\n");
    printf("  --onload                              Watch or dump the library when it's on loading\n");
    printf("  -b --block                            Block the deletion of the library\n");
    printf("  -w --watch                            Watch for the library loading of the app\n");
    printf("  -c --config                           Print the current config\n");
    printf("  -h --help                             Show help\n");
}

json get_json(const std::string& path) {
    std::ifstream file(path);
    if (file.is_open()) {
        json j;
        file >> j;
        file.close();
        return j;
    } else {
        return nullptr;
    }
}

void update_json(json& j, const std::vector<std::string>& key_path, const json& value) {
    json* current = &j;
    // Navigate to the correct position in the JSON object
    for (size_t i = 0; i < key_path.size(); ++i) {
        const std::string& key = key_path[i];

        if (current->contains(key)) {
            if (i == key_path.size() - 1) {
                // Last key in the path, update the value
                (*current)[key] = value;
            } else {
                // Navigate deeper into the JSON object
                current = &((*current)[key]);
            }
        } else {
            std::cerr << "Key path element '" << key << "' not found in JSON." << std::endl;
            exit(-1);
        }
    }
}

void write_json(const json& j, const string& file_path) {
    ofstream file(file_path);
    if (!file.is_open()) {
        cerr << "Unable to write to JSON file: " << file_path << endl;
        exit(-1);
    }
    file << std::setw(4) << j << std::endl; // Pretty print with 4 spaces indentation
}

uint check_delay_optarg(char* option) {
    // Check if the input starts with a minus sign
    if (option[0] == '-') {
        std::cerr << "Negative value is not allowed: " << option << std::endl;
        return -1;
    }
    char *endptr;
    uint temp_value = strtoul(option, &endptr, 10);
    if (*endptr != '\0') {
        std::cerr << "Invalid characters found in the input: " << option << std::endl;
        return -1;
    }
    if (temp_value > UINT_MAX) {
        std::cerr << "Value out of range for unsigned int: " << option << std::endl;
        return -1;
    }
    return temp_value;
}

int main(int argc, char* argv[]) {
    uint uid = getuid();
    if (uid != 0) {
        cout << "Need root to run this program" << endl;
        return -1;
    }

    int option;
    string pkg, lib, regex;
    uint delay = 0, delay_section = 0;
    bool block = false, watch = false, on_load = false, isValidArg = true;

    while((option = getopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
        switch (option) {
            case 'p':
                pkg = optarg;
                break;
            case 'l':
                lib = optarg;
                break;
            case 'r':
                regex = optarg;
                break;
            case 'd': {
                delay = check_delay_optarg(optarg);
                if (delay == -1)
                    return -1;
                break;
            }
            case 111:
            {
                delay_section = check_delay_optarg(optarg);
                if (delay_section == -1)
                    return -1;
                break;
            }
            case 'b':
                block = true;
                break;
            case 'w':
                watch = true;
                break;
            case 'c':
            {
                json j = get_json(config_file_path);
                cout << j.dump(4) << endl;
                return -1;
            }
            case 'h':
                show_usage();
                return -1;
            case 222:
                on_load = true;
                break;
            default:
                isValidArg = false;
                break;
        }
    }

    if (!isValidArg || pkg.empty()) {
        printf("Wrong Arguments, Please Check!!\n");
        show_usage();
        return -1;
    }

    if (delay && delay_section) {
        printf("Cannot use -d, --delay-section options together.\n");
        show_usage();
        return -1;
    }

    json j = get_json(config_file_path);
    std::vector<std::string> key_path;
    key_path = {"package", "name"};
    update_json(j, key_path, pkg);
    key_path = {"package", "mode",  "watch"};
    update_json(j, key_path, watch);
    key_path = {"package" ,"mode" ,"dump" ,"dump_target" ,"so_name"};
    update_json(j, key_path, lib);
    key_path = {"package" ,"mode" ,"dump" ,"dump_target" ,"regex"};
    update_json(j, key_path, regex);
    key_path = {"package" ,"mode" ,"dump" ,"dump_target" ,"delay"};
    update_json(j, key_path, delay);
    key_path = {"package" ,"mode" ,"dump" ,"dump_target" ,"delay_section"};
    update_json(j, key_path, delay_section);
    key_path = {"package" ,"mode" ,"dump" ,"dump_target" ,"block_deletion"};
    update_json(j, key_path, block);
    key_path = {"package" ,"mode" ,"dump" ,"dump_target" ,"on_load"};
    update_json(j, key_path, on_load);

    write_json(j, config_file_path);

    if (watch) {
        cout << "Watch lib loading for " << pkg << endl;
    }

    logcat();

    return 0;
}
