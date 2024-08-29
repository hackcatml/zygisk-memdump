#ifndef ZYGISK_MEMDUMP_DUMPSO_H
#define ZYGISK_MEMDUMP_DUMPSO_H

void dump_so(std::string& package_name, const char* so_path, uintptr_t module_base, size_t module_size, uint delay, uint delay_section);

#endif
