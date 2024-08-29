#include <regex>
#include <thread>
#include <android/dlext.h>
#include <unistd.h>

#include "log.h"
#include "gumhook.h"
#include "dumpso.h"

#define HOOK_DEF(ret, func, ...)         \
    ret (*old_##func)(__VA_ARGS__) = nullptr; \
    ret new_##func(__VA_ARGS__)

struct dumpTargetData {
    std::string package;
    bool watch;
    std::string name;
    std::string regex;
    uint delay;
    uint delay_section;
    bool onload;
};

dumpTargetData data = {};

std::string module_name_to_dump;
const char* so_name = nullptr;
void* handle = nullptr;

GumInterceptor* unlink_interceptor;

bool did_dump = false;

void delay_dump_thread(std::string package, const char* so_path, uintptr_t module_base, size_t module_size, uint delay, uint delay_section) {
    usleep(delay);
    dump_so(package, so_path, module_base, module_size, delay, delay_section);
}

HOOK_DEF(void*, do_dlopen, const char* name, int flags, const android_dlextinfo* extinfo, const void* caller_addr)
{
    so_name = name;
    bool should_dump = false;

    auto dump = [&](bool onload) {
        if (data.watch) {
            LOGD("%s library: %s", onload ? "onload" : "loaded", so_name);
        }

        if (!data.name.empty() && strstr(so_name, data.name.c_str()) != nullptr) {
            LOGD("%s library: %s", onload ? "onload" : "loaded", so_name);
            module_name_to_dump = data.name;
            should_dump = true;
        } else if (data.name.empty() && !data.regex.empty()) {
            std::regex pattern(data.regex);
            if (std::regex_match(so_name, pattern)) {
                LOGD("%s library: %s", onload ? "onload" : "loaded", so_name);
                module_name_to_dump = std::string(so_name).substr(std::string(so_name).find_last_of('/') + 1);
                should_dump = true;
            }
        }

        if (should_dump && !did_dump) {
            GumAddress base = gum_module_find_base_address(so_name);
            if (base != 0) {
                const GumModuleDetails* m = gum_module_map_find(gum_module_map_new(), base);
                if (m != nullptr) {
                    if (data.delay) {
                        std::thread(delay_dump_thread, data.package, m->path, m->range->base_address, m->range->size, data.delay, data.delay_section).detach();
                    } else {
                        dump_so(data.package, m->path, m->range->base_address, m->range->size, data.delay, data.delay_section);
                    }
                    did_dump = true;
                }
            }
        }
    };

    if (data.onload) {
        dump(true);
        return old_do_dlopen(name, flags, extinfo, caller_addr);
    } else {
        handle = old_do_dlopen(name, flags, extinfo, caller_addr);
        if (handle != nullptr) {
            dump(false);
        }
        return handle;
    }
}

HOOK_DEF(int, unlink, const char* pathname) {
    if (!module_name_to_dump.empty() && strstr(pathname, module_name_to_dump.c_str()) != nullptr) {
        // block the deletion
        std::string fake_pathname = std::string(pathname) + ".hackcatml.so";
        int ret = old_unlink(fake_pathname.c_str());
        if (ret < 0) {
            LOGD("block unlink: %s", pathname);
            gum_interceptor_revert(unlink_interceptor, (gpointer) unlink);
        }
        return ret;
    }
    return old_unlink(pathname);
}

void doReplace(GumAddress target_addr, const std::string& sym) {
    GumInterceptor *interceptor = gum_interceptor_obtain();
    gum_interceptor_begin_transaction(interceptor);

    auto replace_func = [&](auto new_func, auto& old_func) {
        return gum_interceptor_replace_fast(interceptor,
                                            GSIZE_TO_POINTER(target_addr),
                                            GSIZE_TO_POINTER(new_func),
                                            (void**)&old_func);
    };

    GumReplaceReturn ret{};

    if (sym == "do_dlopen") {
        ret = replace_func(new_do_dlopen, old_do_dlopen);
    } else if (sym == "unlink") {
        unlink_interceptor = interceptor;
        ret = replace_func(new_unlink, old_unlink);
    }

    LOGD("%s %s", sym.c_str(), ret == GUM_REPLACE_OK ? "replaced" : "replace went wrong");

    gum_interceptor_end_transaction(interceptor);
}

void hookAddress(GumAddress addr, std::string& package, bool watch, std::string& target, std::string& regex, uint delay, uint delay_section, bool block_deletion, bool on_load)
{
    data.package = package;
    data.watch = watch;
    data.name = target;
    // if regex is provided, ignore target module name
    data.regex = regex;
    if (!data.regex.empty())
        data.name.clear();
    data.delay = delay;
    data.delay_section = delay_section;
    if (data.delay_section != 0)
        data.delay = 0;
    // no dump, just watch lib loading
    if (data.watch) {
        data.name.clear();
        data.regex.clear();
        block_deletion = false;
    }
    data.onload = on_load;

    doReplace(addr, "do_dlopen");

    if (block_deletion) {
        doReplace((GumAddress) ((uint64_t)unlink), "unlink");
    }
}