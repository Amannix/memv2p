#ifndef COMMON_H
#define COMMON_H

#define _GNU_SOURCE
#define __USE_GNU

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <getopt.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/time.h>
#include <signal.h>

#define ARGS_MAX    64

enum args_type {
    MBUF,
    STRING,
    WORD,
    FLAG,
};

struct mbuf {
    char *filename;
    int fd;
    uint8_t *data;
    uint32_t length;
};

typedef struct mbuf MBUF_t;
typedef char* STRING_t;
typedef uint64_t WORD_t;
typedef char FLAG_t;

struct test_argument {
    enum args_type type;
    void *addr;
    char *helper;
};

static struct test_argument user_args[ARGS_MAX];
static struct option long_options[ARGS_MAX];
static uint32_t args_end = 0;

__attribute__ ((unused))  static struct mbuf* mbuf_new(char *file)
{
    struct stat ft;
    int fd;
    MBUF_t *f = NULL;

    if (file == NULL)
        return f;

    fd = open(file, O_RDWR | O_CREAT);
    if (fd == -1)
        return NULL;

    f = (MBUF_t*)malloc(sizeof(MBUF_t));
    if (f == NULL) return NULL;
    memset(f, 0, sizeof(MBUF_t));

    f->fd = fd;
    f->filename = (char*)file;

    fstat(f->fd, &ft);
    f->length = ft.st_size;
    f->data = (uint8_t *)mmap(NULL, f->length,
                              PROT_READ | PROT_WRITE, MAP_SHARED, f->fd, 0);
    if (f->data == MAP_FAILED) {
        f->length = 0;
        f->data = NULL;
    }
    return f;
}

__attribute__ ((unused))  static uint32_t mbuf_set_data(MBUF_t *f, uint8_t *data, uint32_t len)
{
    if (!f->fd || ftruncate(f->fd, len) == -1 || data == NULL) {
        perror ("fstream remap failed");
        return 0;
    }
    f->length = len;

    if (f->data == NULL) {
        f->data = (uint8_t *)mmap(NULL, len,
                            PROT_READ | PROT_WRITE, MAP_SHARED, f->fd, 0);
    } else {
        f->data = (uint8_t*)mremap(f->data, f->length, len, MREMAP_MAYMOVE);
    }

    if (f->data == MAP_FAILED) {
        f->length = 0;
        f->data = NULL;
    } else {
        memcpy (f->data, data, len);
        msync(f->data, len, MS_SYNC);
    }
    return f->length;
}

static void add_test_argument(enum args_type type, void *addr, char *helper, char *cmd)
{
    user_args[args_end].type = type;
    user_args[args_end].addr = addr;
    user_args[args_end].helper = helper;

    long_options[args_end].flag = NULL;
    long_options[args_end].has_arg = type == FLAG ? no_argument : required_argument;
    long_options[args_end].name = cmd;
    long_options[args_end].val = args_end;

    ++args_end;
}

#define INIT_PRIO(func, prio) static void __attribute__((constructor(prio))) func(void)
#define FINAL_PRIO(func, prio) static void __attribute__((destructor(prio))) func(void)

/* Register a test argument */
#define register_argument(type, name, helper)          \
    type##_t name;                                     \
    INIT_PRIO(argument_register_##name, 256) {         \
        add_test_argument(type, &name, helper, #name); \
    }

static void Usage(void)
{
    int i;
    printf("Usage:\n");
    for (i = 1; i < args_end; ++i) {
        printf("    -%-15s %s\n", long_options[i].name, user_args[i].helper);
    }
    exit(0);
}

register_argument(FLAG, help, "Usage");
static int loc_argc;
static char **loc_argv;
INIT_PRIO(parse_parameter, 65535)
{
    char c;
    MBUF_t *fs;
    MBUF_t *new_mbuf;
    while ((c = getopt_long_only(loc_argc, loc_argv, "", long_options, NULL)) != -1) {
        if (c != '?') {
            uint8_t id = c;
            switch (user_args[id].type) {
            case STRING:
                *(STRING_t*)user_args[id].addr = optarg;
                break;
            case WORD:
                if (strlen(optarg) > 1 && optarg[1] == 'x')
                    sscanf (optarg, "%lx", (WORD_t *)user_args[id].addr);
                else
                    *(WORD_t *)user_args[id].addr = atoi(optarg);
                break;
            case MBUF:
                fs = (MBUF_t *)user_args[id].addr;
                new_mbuf = mbuf_new(optarg);
                memcpy(fs, new_mbuf, sizeof(MBUF_t));
                break;
            case FLAG:
                *(char *)user_args[id].addr = 1;
                break;
            default:
                break;
            }
        } else {
            exit(-1);
        }
    }
    if (help)
        Usage();
}

#define hexdump(data, len) {         \
    fprintf(stdout, "%s in %p, size %d:", #data, data, (len));     \
    _hexdump_(data, len);   \
}

__attribute__ ((unused)) static void _hexdump_(uint8_t *data, uint32_t len)
{
    uint32_t i = 0;
    for (i = 0; i < len; ++i) {
        if (i % 16 == 0) fprintf(stdout, "\n");
        fprintf(stdout, "%02X", data[i]);
    }
    fprintf(stdout, "\n\n");
}

int __libc_start_main(
    int (*main)(int, char **, char **),
    int argc,
    char **argv,
    int (*init)(int, char **, char **),
    void (*fini)(void),
    void (*rtld_fini)(void),
    void *stack_end)
{

    /* Find the real __libc_start_main()... */
    typeof(&__libc_start_main) orig = dlsym(RTLD_NEXT, "__libc_start_main");

    loc_argc = argc;
    loc_argv = argv;

    /* ... and call it with our custom main function */
    return orig(main, argc, argv, init, fini, rtld_fini, stack_end);
}

#endif
