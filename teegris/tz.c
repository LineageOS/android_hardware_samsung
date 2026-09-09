/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define TZ_IOC_MAGIC 'c'
#define TZIO_MEM_REGISTER _IOW(TZ_IOC_MAGIC, 120, struct tzio_mem_register)
#define TZIO_UIWSOCK_CONNECT _IOW(TZ_IOC_MAGIC, 130, int)
#define TZIO_UIWSOCK_WAIT_CONNECTION _IOW(TZ_IOC_MAGIC, 131, int)
#define TZIO_UIWSOCK_SEND _IOW(TZ_IOC_MAGIC, 132, int)
#define TZIO_UIWSOCK_RECV_MSG _IOWR(TZ_IOC_MAGIC, 133, int)
#define TZ_UIWSOCK_MAX_NAME_LENGTH 256

struct tzio_mem_register {
    uint64_t size;  /* Memory region size (in) */
    uint32_t write; /* 1 - rw, 0 - ro */
} __attribute__((__packed__));

struct tz_uiwsock_connection {
    char name[TZ_UIWSOCK_MAX_NAME_LENGTH];
};

struct tz_uiwsock_data {
    uint64_t buffer;
    uint64_t size;
    uint32_t flags;
} __attribute__((__packed__));

void iwd_connect(int fd, char* name) {
    struct tz_uiwsock_connection conn;

    strncpy(conn.name, name, sizeof(conn.name));
    ioctl(fd, TZIO_UIWSOCK_CONNECT, conn);
    ioctl(fd, TZIO_UIWSOCK_WAIT_CONNECTION);
}

void iwd_send(int fd, int* buffer, uint64_t size) {
    struct tz_uiwsock_data data;

    data.buffer = (uint64_t)buffer;
    data.size = size;
    ioctl(fd, TZIO_UIWSOCK_SEND, &data);
}

void iwd_recv(int fd, char* buffer, uint64_t size) {
    struct tz_uiwsock_data data;

    data.buffer = (uint64_t)buffer;
    data.size = size;
    ioctl(fd, TZIO_UIWSOCK_RECV_MSG, &data);
}

int iwshmem_create_region(uint64_t size, int* id) {
    int fd;
    struct tzio_mem_register mem;

    fd = open("/dev/tziwshmem", O_RDWR);
    mem.size = size;
    *id = ioctl(fd, TZIO_MEM_REGISTER, &mem);
    return fd;
}

void load(int* info, char* name) {
    int fd, mfd;
    static char path[49] = "/vendor/tee/";
    struct stat st;
    uint64_t size;
    void* mem;

    memcpy(path + 12, name, 36);
    fd = open(path, O_RDONLY);
    fstat(fd, &st);
    size = st.st_size;

    mfd = iwshmem_create_region(size, info + 1);
    mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, mfd, 0);
    close(mfd);
    read(fd, mem, size);
    close(fd);
    info[0] = size;
}

void* loader(void* efd) {
    char message[264];
    int fd, epfd;
    int reply[40];
    struct epoll_event event;

    fd = open("/dev/tziwsock", O_RDWR);
    iwd_connect(fd, "root_task");

    reply[0] = 10;
    iwd_send(fd, reply, 8);
    iwd_recv(fd, message, 16);

    epfd = epoll_create1(0);
    event.events = EPOLLIN;
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);
    event.data.ptr = efd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, *(int*)efd, &event);

    while (true) {
        epoll_wait(epfd, &event, 1, -1);
        if (event.data.ptr == efd) break;
        iwd_recv(fd, message, sizeof(message));
        load(reply + 5, message + 8);
        reply[0] = 9;
        iwd_send(fd, reply, sizeof(reply));
    }

    return NULL;
}

int main() {
    long efd;
    pthread_t thread;

    efd = eventfd(0, 0);
    pthread_create(&thread, NULL, loader, &efd);
    pause();
    write(efd, &efd, 8);
    return -1;
}
