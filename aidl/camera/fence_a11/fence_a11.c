/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 *
 * android::Fence with the layout it had up to Android 12, for camera HAL blobs built back then.
 *
 * Such a HAL allocates `new Fence(fd)` itself with the old size (8 bytes: LightRefBase mCount, unique_fd
 * mFenceFd) and refcounts and deletes it with inlined code. Since Android 13 Fence has virtual methods and is
 * 16 bytes (vptr, mCount, mFenceFd), so the libui constructor writes past the HAL's allocation and corrupts the
 * heap; the camera provider then crashes at random places.
 *
 * Point the HAL's two Fence imports here with same-length renames in extract-files.py:
 *     .binary_regex_replace(b'_ZN7android5FenceC1Ei', b'_ZN7a11shim5FenceC1Ei')
 *     .binary_regex_replace(b'_ZN7android5Fence4waitEi', b'_ZN7a11shim5Fence4waitEi')
 *     .add_needed('libcamera_fence_a11.so')
 */

#include <android/fdsan.h>
#include <errno.h>
#include <stdint.h>
#include <sync/sync.h>

struct fence_a11 {
    int32_t count; /* LightRefBase<Fence>::mCount */
    int fd;        /* base::unique_fd mFenceFd */
};

/* android::Fence::Fence(int) */
void fence_ctor(struct fence_a11* f, int fd) __asm__("_ZN7a11shim5FenceC1Ei");
void fence_ctor(struct fence_a11* f, int fd) {
    f->count = 0;
    f->fd = fd;
    /* As base::unique_fd::reset(): the HAL's inlined ~unique_fd closes the fd with this owner tag. */
    if (fd != -1) {
        android_fdsan_exchange_owner_tag(
                fd, 0,
                android_fdsan_create_owner_tag(ANDROID_FDSAN_OWNER_TYPE_UNIQUE_FD,
                                               (uint64_t)(uintptr_t)&f->fd));
    }
}

/* android::Fence::wait(int) */
int fence_wait(struct fence_a11* f, int timeout) __asm__("_ZN7a11shim5Fence4waitEi");
int fence_wait(struct fence_a11* f, int timeout) {
    if (f->fd == -1) return 0; /* NO_ERROR */
    int err = sync_wait(f->fd, timeout);
    return err < 0 ? -errno : 0;
}
