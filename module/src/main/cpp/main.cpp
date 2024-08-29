#include <unistd.h>
#include <fstream>

#include "zygisk.hpp"
#include "log.h"
#include "gumhook.h"
#include "frida-gum.h"
#include "nlohmann/json.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

using json = nlohmann::json;

void writeString(int fd, const std::string& str) {
    size_t length = str.size() + 1;
    write(fd, &length, sizeof(length));
    write(fd, str.c_str(), length);
}

std::string readString(int fd) {
    size_t length;
    read(fd, &length, sizeof(length));
    std::vector<char> buffer(length);
    read(fd, buffer.data(), length);
    return {buffer.data()};
}

std::string getPathFromFd(int fd) {
    char buf[PATH_MAX];
    std::string fdPath = "/proc/self/fd/" + std::to_string(fd);
    ssize_t len = readlink(fdPath.c_str(), buf, sizeof(buf) - 1);
    if (len != -1) {
        buf[len] = '\0';
        return {buf};
    } else {
        return "";
    }
}

class MyModule : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->_api = api;
        _env = env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        if (!args || !args->nice_name) {
            LOGE("Skip unknown process");
            return;
        }

        auto package_name = _env->GetStringUTFChars(args->nice_name, nullptr);
        std::string module_dir = getPathFromFd(_api->getModuleDir());

        int fd = _api->connectCompanion();

        std::string config_file_path_to_read = module_dir + "/config";
        writeString(fd, config_file_path_to_read);
        std::string target_package_name = readString(fd);

        if (strcmp(package_name, target_package_name.c_str()) == 0) {
            std::string do_dlopen = "__dl__Z9do_dlopenPKciPK17android_dlextinfoPKv";
            bool watch;
            read(fd, &watch, sizeof(watch));
            std::string target_so_name = readString(fd);
            std::string regex = readString(fd);
            uint delay;
            read(fd, &delay, sizeof(delay));
            uint delay_section;
            read(fd, &delay_section, sizeof(delay_section));
            bool block_deletion;
            read(fd, &block_deletion, sizeof(block_deletion));
            bool on_load;
            read(fd, &on_load, sizeof(on_load));

            gum_init_embedded();
#ifdef __LP64__
            const char* linker = "linker64";
#else
            const char* linker = "linker";
#endif
            // hook do_dlopen
            hookAddress(gum_module_find_symbol_by_name(linker, do_dlopen.c_str()), target_package_name, watch, target_so_name, regex, delay, delay_section, block_deletion, on_load);

            close(fd);
        } else {
            _api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            close(fd);
        }
        _env->ReleaseStringUTFChars(args->nice_name, package_name);
    }

private:
    Api* _api{};
    JNIEnv* _env{};
};

json get_json(const std::string& path) {
    std::ifstream file(path);
    if (file.is_open()) {
        json j;
        file >> j;
        file.close();
        return j;
    } else {
        LOGD("Failed to open %s", path.c_str());
        return nullptr;
    }
}

static void companion_handler(int i) {
    std::string config_file_path = readString(i);

    json j = get_json(config_file_path);
    if (j == nullptr) {
        return;
    }
    std::string pkg_name = j["package"]["name"];
    bool watch = j["package"]["mode"]["watch"];
    std::string so_name = j["package"]["mode"]["dump"]["dump_target"]["so_name"];
    std::string regex = j["package"]["mode"]["dump"]["dump_target"]["regex"];
    uint delay = j["package"]["mode"]["dump"]["dump_target"]["delay"];
    uint delay_section = j["package"]["mode"]["dump"]["dump_target"]["delay_section"];
    bool block_deletion = j["package"]["mode"]["dump"]["dump_target"]["block_deletion"];
    bool on_load = j["package"]["mode"]["dump"]["dump_target"]["on_load"];

    writeString(i, pkg_name);
    write(i, &watch, sizeof(watch));
    writeString(i, so_name);
    writeString(i, regex);
    write(i, &delay, sizeof(delay));
    write(i, &delay_section, sizeof(delay_section));
    write(i, &block_deletion, sizeof(block_deletion));
    write(i, &on_load, sizeof(on_load));
}

REGISTER_ZYGISK_MODULE(MyModule)
REGISTER_ZYGISK_COMPANION(companion_handler)
