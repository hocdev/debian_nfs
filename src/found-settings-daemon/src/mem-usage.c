#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    unsigned long total;
    unsigned long free;
    unsigned long available;
    unsigned long buffers;
    unsigned long cached;
} Memory;

static bool
read_memory(Memory *memory)
{
    FILE *file = fopen("/proc/meminfo", "r");
    if (!file)
        return false;

    const struct {
        const char *name;
        unsigned long *value;
    } fields[] = {
        { "MemTotal:", &memory->total },
        { "MemFree:", &memory->free },
        { "MemAvailable:", &memory->available },
        { "Buffers:", &memory->buffers },
        { "Cached:", &memory->cached },
    };

    char line[1024];
    while (fgets(line, sizeof line, file)) {
        char key[32];
        unsigned long value;
        if (sscanf(line, "%31s %lu", key, &value) != 2)
            continue;

        for (size_t i = 0; i < sizeof fields / sizeof fields[0]; i++) {
            if (strcmp(key, fields[i].name) == 0) {
                *fields[i].value = value;
                break;
            }
        }
    }
    bool ok = !ferror(file) && memory->total > 0;
    fclose(file);
    return ok;
}

int
main(void)
{
    bool notified = false;
    while (true) {
        Memory memory = {0};
        if (read_memory(&memory)) {
            float usage = (float) ((100.0 * ((double) memory.total - memory.available)) /
                                  memory.total);
            printf("MemTotal      : %lu\n"
                   "MemFree       : %lu\n"
                   "MemAvailable  : %lu\n"
                   "MemBuffers    : %lu\n"
                   "MemCached     : %lu\n"
                   "MemSwapCached : 0\n"
                   "MemSwapTotal  : 0\n"
                   "MemSwapFree   : 0\n"
                   "Mem Usage(%%): %8.4f\n",
                   memory.total, memory.free, memory.available, memory.buffers,
                   memory.cached, (double) usage);

            if (usage >= 90.0f && !notified) {
                int status = system("/usr/lib/found-settings-daemon/mem-usage-notification");
                (void) status;
                notified = true;
            } else {
                notified = false;
            }
        }
        sleep(10);
        putchar('\n');
    }
}
