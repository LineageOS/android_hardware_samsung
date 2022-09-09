#include "filesystem.h"

#include <dirent.h>
#include <log/log.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <sstream>
#include <string>
#include <vector>

namespace filesystem {

bool exists(const path& p) {
    struct stat s;
    return stat(p.string().c_str(), &s) == 0;
}

bool is_directory(const path& p) {
    struct stat s;
    if (stat(p.string().c_str(), &s))
        return false;

    return S_ISDIR(s.st_mode);
}

bool is_symlink(const path& p) {
    struct stat s;
    if (lstat(p.string().c_str(), &s))
        return false;

    return S_ISLNK(s.st_mode);
}

path read_symlink(const path& p) {
    char* actualPath = realpath(p.string().c_str(), NULL);
    if (!actualPath) {
        return path(p.string());
    }

    path out(actualPath);
    free(actualPath);
    return out;
}

std::vector<directory_entry> directory_iterator(const path& p) {
    if (!exists(p) || !is_directory(p))
        return {};

    std::unique_ptr<DIR, decltype(&closedir)> dir(opendir(p.string().c_str()), &closedir);
    if (!dir) {
        ALOGE("Failed to open %s directory", p.string().c_str());
    }

    std::vector<directory_entry> out;
    struct dirent* dent;
    while ((dent = readdir(dir.get()))) {
        if (!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, ".."))
            continue;

        std::stringstream ss(p.string());
        ss << "/" << dent->d_name;
        out.emplace_back(ss.str());
    }

    return out;
}

} // namespace filesystem
