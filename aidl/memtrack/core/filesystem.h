// TODO(b/147469372): filesystem library in Android's libcxx is not available
// for vendors. It had an unstable ABI and libcxx isn't updated ever since.

// This simply implements some of the required functions in not-so-safe fashion.

#pragma once

#include <dirent.h>
#include <log/log.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string>
#include <vector>

namespace filesystem {
class path {
public:
    path(const std::string _path) : strPath(_path) {}

    path filename() const {
        auto pos = strPath.rfind('/');
        if (pos == std::string::npos)
            return path(strPath);

        pos++;
        auto l = strPath.size();
        return path(strPath.substr(pos, l - pos));
    }

    std::string string() const { return strPath; }

private:
    std::string strPath;
};

class directory_entry {
public:
    directory_entry(const std::string _path) : p(_path) {}

    class path path() {
        return p;
    }

private:
    class path p;
};

bool exists(const path& p);

bool is_directory(const path& p);

bool is_symlink(const path& p);

path read_symlink(const path& p);

// Vector is easier to create than an iterator and serves our purposes well
std::vector<directory_entry> directory_iterator(const path& p);
} // namespace filesystem
