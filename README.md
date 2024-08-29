# zygisk-memdump
A zygisk module that dumps so file from process memory.

# Features
- Monitor the loading of libraries in the target package
- Delay the dumping of the library after it's loaded
- Delay the dumping of specific sections of the library
- Fix the dumped .so file
- Block attempts to delete the library
- Regex support for searching the target library

# Usage
- **Install the release file and reboot**<br> 
`zygisk-memdump` tool will be placed in `/data/local/tmp/`<br>
- **General Usage**<br>
  In the terminal, use the zygisk-memdump tool to set up the config for the lib dump and start the target app:<br> 
`/data/local/tmp/zygisk-memdump -p <packageName> <options>`
```
/data/local/tmp/zygisk-memdump -h

Usage: ./zygisk-memdump -p <packageName> <option(s)>
 Options:
  -l --lib <library_name>               Library name to dump
  -r --regex "<expression>"               Regex expression matching the library name to dump
  -d --delay <microseconds>             Delay in microseconds before dumping the library (cannot be used with the --delay-section option)
  --delay-section <microseconds>        Delay in microseconds before dumping the library's section (.text, il2cpp, .rodata)
  --onload                              Watch or dump the library when it's on loading
  -b --block                            Block the deletion of the library
  -w --watch                            Watch for the library loading of the app
  -c --config                           Print the current config
  -h --help                             Show help
```

- **Watch for the loaded library**<br>
```
/data/local/tmp/zygisk-memdump -p com.hackcatml.test -w
Watch lib loading for com.hackcatml.test
16:43:27.838 [ZygiskMemDump] loaded library: libnativebridge.so
16:43:27.846 [ZygiskMemDump] loaded library: libperfetto_hprof.so
16:43:27.876 [ZygiskMemDump] loaded library: libframework-connectivity-tiramisu-jni.so
16:43:27.893 [ZygiskMemDump] loaded library: /data/app/~~101d8PV2ttpn7lFn6MHlIg==/com.hackcatml.test-cjlRvYK8TV-ze1JDPDhrcw==/oat/arm64/base.odex
16:43:27.918 [ZygiskMemDump] loaded library: libframework-connectivity-jni.so
...  
```

- **Dump library**<br>

Dump libil2cpp.so 3 seconds after it has been loaded.
```
/data/local/tmp/zygisk-memdump -p com.hackcatml.test -l libil2cpp.so -d 3000000
08:00:42.691 [ZygiskMemDump] do_dlopen replaced
08:00:42.997 [ZygiskMemDump] loaded library: /data/app/~~101d8PV2ttpn7lFn6MHlIg==/com.hackcatml.test-cjlRvYK8TV-ze1JDPDhrcw==/lib/arm64/libil2cpp.so
08:00:43.140 [ZygiskMemDump] loaded library: /data/app/~~101d8PV2ttpn7lFn6MHlIg==/com.hackcatml.test-cjlRvYK8TV-ze1JDPDhrcw==/lib/arm64/libil2cpp.so
08:00:45.999 [ZygiskMemDump] module base: 0x6ef08d0000, size: 103014400
08:00:46.318 [ZygiskMemDump] mem dump: 0x6ef08d0000, 103014400 bytes
08:00:46.326 [ZygiskMemDump] libil2cpp.so dump done
08:00:46.326 [ZygiskMemDump] Output: /data/data/com.hackcatml.test/files/libil2cpp.so.dump[0x6ef08d0000].so
08:00:46.326 [ZygiskMemDump] Rebuilding libil2cpp.so.dump[0x6ef08d0000].so
08:00:46.613 [ZygiskMemDump] Rebuilding libil2cpp.so Complete
08:00:46.613 [ZygiskMemDump] Output: /data/data/com.hackcatml.test/files/libil2cpp.so.dump[0x6ef08d0000].so.fix.so
```

Some apps load the library and mess up the header section.<br>
`--delay-section` option allows you to first dump everything before the matched section, such as .text, il2cpp, or .rodata, and then dump the rest. If the .text, il2cpp, or .rodata sections are not found, it will dump the ELF header first.<br>
For example, use a regular expression to find a matching library, dump everything before the section first, and then dump the remaining part after a 3-second delay. If the app attempts to delete the library, block it.
```
/data/local/tmp/zygisk-memdump -p com.hackcatml.test2 -r ".*\\.[A-Z0-9]{5}/[A-Z0-9]{5}$" --delay-section 3000000 -b
08:21:18.930 [ZygiskMemDump] do_dlopen replaced
08:21:18.931 [ZygiskMemDump] unlink replaced
08:21:19.238 [ZygiskMemDump] loaded library: /data/data/com.hackcatml.test2/.8HKOW/UVAYW
08:21:19.238 [ZygiskMemDump] module base: 0x6efb003000, size: 4218880
08:21:19.245 [ZygiskMemDump] found .text section: 0x6efb064850
08:21:19.245 [ZygiskMemDump] mem dump: 0x6efb003000, 399440 bytes
08:21:19.338 [ZygiskMemDump] block unlink: /data/data/com.hackcatml.test2/.8HKOW/UVAYW
08:21:22.246 [ZygiskMemDump] remaining_size: 3819440
08:21:22.262 [ZygiskMemDump] mem dump: 0x6efb064850, 3819440 bytes
08:21:22.264 [ZygiskMemDump] UVAYW.dump[0x6efb003000].so dump done
08:21:22.264 [ZygiskMemDump] Output: /data/data/com.hackcatml.test2/files/UVAYW.dump[0x6efb003000].so
08:21:22.264 [ZygiskMemDump] Rebuilding UVAYW.dump[0x6efb003000].so
08:21:22.284 [ZygiskMemDump] Rebuilding UVAYW.dump[0x6efb003000].so Complete
08:21:22.284 [ZygiskMemDump] Output: /data/data/com.hackcatml.test2/files/UVAYW.dump[0x6efb003000].so.fix.so
```

This tool generally works at the point when the library is just loaded (after the original do_dlopen is called).<br>
However, some apps enter an infinite loop due to this timing.<br>
By using the `--onload` option, you can operate at the point when the library is being loaded (before the original do_dlopen is called).
For example, dump the library at the moment when libil2cpp.so is being loaded.
```
/data/local/tmp/zygisk-memdump -p com.hackcatml.test3 -r ".*il2cpp\\.so$" --onload                  
08:44:42.747 [ZygiskMemDump] do_dlopen replaced
08:44:43.808 [ZygiskMemDump] onload library: /data/app/~~j94vhrlNIW199fjbrIW_wg==/com.hackcatml.test3-48Df26FAQDsR72b4icGlWg==/lib/arm64/libil2cpp.so
08:44:43.809 [ZygiskMemDump] onload library: libil2cpp.so
08:44:44.814 [ZygiskMemDump] onload library: /data/app/~~j94vhrlNIW199fjbrIW_wg==/com.hackcatml.test3-48Df26FAQDsR72b4icGlWg==/split_config.arm64_v8a.apk!/lib/arm64-v8a/libil2cpp.so
08:44:44.815 [ZygiskMemDump] module base: 0x6eb9cb3000, size: 68055040
08:44:44.991 [ZygiskMemDump] mem dump: 0x6eb9cb3000, 68055040 bytes
08:44:44.996 [ZygiskMemDump] libil2cpp.so dump done
08:44:44.996 [ZygiskMemDump] Output: /data/data/com.hackcatml.test3/files/libil2cpp.so.dump[0x6eb9cb3000].so
08:44:44.996 [ZygiskMemDump] Rebuilding libil2cpp.so.dump[0x6eb9cb3000].so
08:44:45.179 [ZygiskMemDump] Rebuilding libil2cpp.so Complete
08:44:45.179 [ZygiskMemDump] Output: /data/data/com.hackcatml.test3/files/libil2cpp.so.dump[0x6eb9cb3000].so.fix.so
```

# Credits
[frida-gum](https://github.com/frida/frida-gum)<br>
[SoFixer](https://github.com/F8LEFT/SoFixer)<br>
[json](https://github.com/nlohmann/json)<br>
[MemDumper](https://github.com/kp7742/MemDumper)
