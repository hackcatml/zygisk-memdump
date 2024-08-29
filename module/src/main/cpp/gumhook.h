#ifndef ZYGISK_MEMDUMP_GUMHOOK_H
#define ZYGISK_MEMDUMP_GUMHOOK_H

#include "frida-gum.h"

void hookAddress(GumAddress addr, std::string& target_package_name, bool watch, std::string& target_so_name, std::string& target_so_regex, uint delay, uint delay_section, bool block_delete, bool on_load);

#endif
