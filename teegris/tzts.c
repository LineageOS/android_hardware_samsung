/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cutils/properties.h>
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <teecl.h>

#define DRIVER_DIR "/vendor/tee/driver"

struct message {
    void* context;
    void* session;
    char* name;
};

void parse(char* name, char* uuid) {
    char inter;
    while (*name != 0) {
        inter = name[2];
        name[2] = 0;
        *uuid = strtol(name, NULL, 16);
        name[2] = inter;
        name += 2;
        uuid += 1;
    }
}

void* load(void* message) {
    void* data;
    int fd;
    size_t size;
    struct stat st;
    char uuid[17];
    char path[56];
    struct message* msg = (struct message*)message;

    TEEC_InitializeContext(NULL, &msg->context);

    snprintf(path, sizeof(path), "%s/%s", DRIVER_DIR, msg->name);
    fd = open(path, O_RDONLY);
    fstat(fd, &st);
    size = st.st_size;
    data = malloc(size);
    read(fd, data, size);
    close(fd);

    parse(msg->name + 24, uuid + 10);
    TEECS_OpenSession(&msg->context, &msg->session, uuid, data, size, 0, NULL, NULL, NULL);
    free(data);
    return NULL;
}

void* unload(void* message) {
    struct message* msg = (struct message*)message;

    TEEC_CloseSession(&msg->session);
    TEEC_FinalizeContext(&msg->context);
    return NULL;
}

int main() {
    DIR* dir;
    struct dirent* entry;
    int thread_count = 0;
    long loc;

    dir = opendir(DRIVER_DIR);

    for (int i = 0; i < 2; i++) {
        readdir(dir);
    }

    loc = telldir(dir);

    while ((entry = readdir(dir)) != NULL) {
        thread_count++;
    }

    seekdir(dir, loc);
    pthread_t threads[thread_count];
    struct message msgs[thread_count];

    for (int i = 0; i < thread_count; i++) {
        entry = readdir(dir);
        msgs[i].name = entry->d_name;
        pthread_create(threads + i, NULL, load, msgs + i);
    }

    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }

    property_set("vendor.tzts_daemon", "Ready");
    closedir(dir);
    pause();

    for (int i = 0; i < thread_count; i++) {
        pthread_create(threads + i, NULL, unload, msgs + i);
    }

    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }

    return -1;
}
