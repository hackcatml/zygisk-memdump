#include <unistd.h>
#include <sstream>
#include <fstream>
#include <thread>

#include "log.h"
#include "frida-gum.h"
#include "SoFixer/ElfReader.h"
#include "SoFixer/ElfRebuilder.h"
#include "SoFixer/ObElfReader.h"

static std::string output_dir;
uintptr_t section_addr;

gboolean section_found(const GumSectionDetails * details, gpointer user_data)
{
    if (strstr(details->name, "il2cpp") != nullptr || strstr(details->name, ".text") != nullptr || strstr(details->name, ".rodata") != nullptr)
    {
        section_addr = details->address;
        LOGD("found %s section: %p", details->name, (void*)section_addr);
        return false;
    }
    return true;
}

int fix_so(const std::string& sofile_path, uintptr_t module_base, size_t module_size)
{
    //SoFixer Code//
    LOGD("Rebuilding %s", sofile_path.substr(sofile_path.find_last_of('/') + 1).c_str());
    std::string outPath = sofile_path + ".fix.so";

    ObElfReader elf_reader;

    elf_reader.setDumpSoBaseAddr(module_base);
    elf_reader.setDumpSoSize(module_size);

    auto file = fopen(sofile_path.c_str(), "rb");
    if (nullptr == file) {
        LOGD("source so file cannot found!!!");
        return -1;
    }

    auto fd = fileno(file);

    if (!elf_reader.setSource(sofile_path.c_str())) {
        LOGD("unable to open source file");
        return -1;
    }

    if (!elf_reader.Load()) {
        LOGD("source so file is invalid");
        return -1;
    }

    ElfRebuilder elf_rebuilder(&elf_reader);
    if (!elf_rebuilder.Rebuild()) {
        LOGD("error occured in rebuilding elf file");
        return -1;
    }
    fclose(file);

    std::ofstream redump(outPath, std::ofstream::out | std::ofstream::binary);
    if (redump.is_open()) {
        redump.write((char*) elf_rebuilder.getRebuildData(), elf_rebuilder.getRebuildSize());
    } else {
        LOGD("Can't Output File");
        return -1;
    }
    redump.close();
    //SoFixer Code//
    return 1;
}

void delay_section_dump_thread(const std::string& dump_so_path, uintptr_t module_base, uintptr_t offset, size_t module_size, uint delay)
{
    usleep(delay);
    uintptr_t remaining_size = module_size - offset;
    LOGD("remaining_size: %d", remaining_size);

    std::string module_name_to_dump = dump_so_path.substr(dump_so_path.find_last_of('/') + 1);

    std::ofstream dump(dump_so_path, std::ofstream::out | std::ofstream::binary | std::ofstream::app);
    if (dump.is_open()) {
        auto* buffer = new guint8[remaining_size];
        std::memmove(buffer, reinterpret_cast<const void*>(module_base + offset), remaining_size);
        dump.write(reinterpret_cast<const char*>(buffer), (int)remaining_size);
        LOGD("mem dump: %p, %d bytes", reinterpret_cast<const void*>(module_base + offset), remaining_size);
        delete[] buffer;
        dump.close();
        LOGD("%s dump done", module_name_to_dump.c_str());
        LOGD("Output: %s", dump_so_path.c_str());
    } else {
        LOGD("Failed to open file: %s", dump_so_path.c_str());
    }

    int res = fix_so(dump_so_path, module_base, module_size);
    if (res == 1) {
        LOGD("Rebuilding %s Complete", module_name_to_dump.c_str());
        LOGD("Output: %s", (dump_so_path + ".fix.so").c_str());
    } else {
        LOGD("SoFix fail");
    }
}

void dump_so(std::string& package_name, const char* so_path, uintptr_t module_base, size_t module_size, uint delay, uint delay_section)
{
    LOGD("module base: %p, size: %u", (void*)module_base, module_size);

    output_dir = "/data/data/" + package_name + "/files/";
    std::stringstream module_base_hexstr;
    module_base_hexstr << "0x" << std::hex << module_base;
    std::string module_name_to_dump = std::string(so_path).substr(std::string(so_path).find_last_of('/') + 1);
    std::string dump_so_path = output_dir + module_name_to_dump + ".dump[" + module_base_hexstr.str() +"].so";

    std::ofstream dump(dump_so_path, std::ofstream::out | std::ofstream::binary);
    if (dump.is_open()) {
        gum_ensure_code_readable((void*)module_base, module_size);

        uintptr_t offset{};
        if (delay_section > 0) {
            // dump before the section first
            gum_module_enumerate_sections(so_path, section_found, NULL);
            if (section_addr == 0) {
                // If cannot find section_addr, then dump elf header first
#if defined(__LP64__)
                offset = sizeof(Elf64_Ehdr);
#else
                offset = sizeof(Elf32_Ehdr);
#endif
            } else {
                offset = section_addr - module_base;
            }
            auto* header_buffer = new u_char[offset];
            std::memmove(header_buffer, reinterpret_cast<const void*>(module_base), offset);
            dump.write(reinterpret_cast<const char*>(header_buffer), (int)offset);
            LOGD("mem dump: %p, %u bytes", reinterpret_cast<const void *>(module_base), offset);
            delete[] header_buffer;
            dump.close();
            // dump remaining part
            std::thread t(delay_section_dump_thread, dump_so_path, module_base, offset, module_size, delay_section);
            t.detach();
        } else {
            auto* buffer = new u_char[module_size];
            std::memmove(buffer, reinterpret_cast<const void*>(module_base), module_size);
            dump.write(reinterpret_cast<const char*>(buffer), (int)module_size);
            LOGD("mem dump: %p, %u bytes", reinterpret_cast<const void *>(module_base), module_size);
            delete[] buffer;
            dump.close();
            LOGD("%s dump done", module_name_to_dump.c_str());
            LOGD("Output: %s", dump_so_path.c_str());

            int res = fix_so(dump_so_path, module_base, module_size);
            if (res == 1) {
                LOGD("Rebuilding %s Complete", module_name_to_dump.c_str());
                LOGD("Output: %s", (dump_so_path + ".fix.so").c_str());
            } else {
                LOGD("SoFix fail");
            }
        }
    } else {
        LOGD("failed to open file");
    }
}