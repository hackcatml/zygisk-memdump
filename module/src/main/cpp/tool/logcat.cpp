// https://github.com/topjohnwu/Magisk/blob/master/native/src/core/deny/logcat.cpp
#include <unistd.h>
#include <android/log.h>
#include <iostream>
#include <iomanip>

using namespace std;

struct logger_entry {
    uint16_t len;      /* length of the payload */
    uint16_t hdr_size; /* sizeof(struct logger_entry) */
    int32_t pid;       /* generating process's pid */
    uint32_t tid;      /* generating process's tid */
    uint32_t sec;      /* seconds since Epoch */
    uint32_t nsec;     /* nanoseconds */
    uint32_t lid;      /* log id of the payload, bottom 4 bits currently */
    uint32_t uid;      /* generating process's uid */
};

#define LOGGER_ENTRY_MAX_LEN (5 * 1024)
struct log_msg {
    union [[gnu::aligned(4)]] {
        unsigned char buf[LOGGER_ENTRY_MAX_LEN + 1];
        struct logger_entry entry;
    };
};

struct AndroidLogEntry {
    time_t tv_sec;
    long tv_nsec;
    android_LogPriority priority;
    int32_t uid;
    int32_t pid;
    int32_t tid;
    const char *tag;
    size_t tagLen;
    size_t messageLen;
    const char *message;
};

struct [[gnu::packed]] android_event_header_t {
    int32_t tag;    // Little Endian Order
};

struct [[gnu::packed]] android_event_int_t {
    int8_t type;    // EVENT_TYPE_INT
    int32_t data;   // Little Endian Order
};

struct [[gnu::packed]] android_event_string_t {
    int8_t type;    // EVENT_TYPE_STRING;
    int32_t length; // Little Endian Order
    char data[];
};

struct [[gnu::packed]] android_event_list_t {
    int8_t type;    // EVENT_TYPE_LIST
    int8_t element_count;
} ;

// 30014 am_proc_start (User|1|5),(PID|1|5),(UID|1|5),(Process Name|3),(Type|3),(Component|3)
struct [[gnu::packed]] android_event_am_proc_start {
    android_event_header_t tag;
    android_event_list_t list;
    android_event_int_t user;
    android_event_int_t pid;
    android_event_int_t uid;
    android_event_string_t process_name;
//  android_event_string_t type;
//  android_event_string_t component;
};

// 3040 boot_progress_ams_ready (time|2|3)

extern "C" {

[[gnu::weak]] struct logger_list *android_logger_list_alloc(int mode, unsigned int tail, pid_t pid);
[[gnu::weak]] void android_logger_list_free(struct logger_list *list);
[[gnu::weak]] int android_logger_list_read(struct logger_list *list, struct log_msg *log_msg);
[[gnu::weak]] struct logger *android_logger_open(struct logger_list *list, log_id_t id);
[[gnu::weak]] int android_log_processLogBuffer(struct logger_entry *buf, AndroidLogEntry *entry);

}

static void process_main_buffer(struct log_msg *msg) {
    AndroidLogEntry entry{};
    if (android_log_processLogBuffer(&msg->entry, &entry) < 0) return;
    entry.tagLen--;
    auto tag = string_view(entry.tag, entry.tagLen);

    if (tag.find("ZygiskMemDump") != std::string::npos) {
        struct tm *local_time = localtime(&entry.tv_sec);
        long milliseconds = entry.tv_nsec / 1000000;
        cout << std::put_time(local_time, "%H:%M:%S") << "." << std::setw(3) << std::setfill('0') << milliseconds
        << " " << tag << " " << entry.message << endl;
    }
}

static void process_events_buffer(struct log_msg *msg) {
    if (msg->entry.uid != 1000) return;
    auto event_data = &msg->buf[msg->entry.hdr_size];
    auto event_header = reinterpret_cast<const android_event_header_t *>(event_data);
    if (event_header->tag == 30014) {
        auto am_proc_start = reinterpret_cast<const android_event_am_proc_start *>(event_data);
        auto proc = string_view(am_proc_start->process_name.data,
                                am_proc_start->process_name.length);
        return;
    }
    if (event_header->tag == 3040) {
        return;
    }
}

[[noreturn]] void run() {
    while (true) {
        const unique_ptr<logger_list, decltype(&android_logger_list_free)> logger_list{
            android_logger_list_alloc(0, 1, 0), &android_logger_list_free};

        for (log_id id: {LOG_ID_MAIN, LOG_ID_EVENTS}) {
            auto *logger = android_logger_open(logger_list.get(), id);
            if (logger == nullptr) continue;
        }

        struct log_msg msg{};
        while (true) {
            if (android_logger_list_read(logger_list.get(), &msg) <= 0) {
                break;
            }

            switch (msg.entry.lid) {
                case LOG_ID_EVENTS:
                    process_events_buffer(&msg);
                    break;
                case LOG_ID_MAIN:
                    process_main_buffer(&msg);
                default:
                    break;
            }
        }
        sleep(1);
    }

    pthread_exit(nullptr);
}

void logcat() {
    run();
}
