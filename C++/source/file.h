//
// Created by Fan on 2025/11/23.
//

#ifndef SOURCE_FILE_H
#define SOURCE_FILE_H

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

namespace CodeGuide {
/**
 * @brief rename 提供文件替换的原子操作
 * @param src
 * @param dest
 */
void atomicRename(const std::string &src, const std::string &dest) {
    if (std::rename(src.c_str(), dest.c_str()) != 0) {
        std::perror("rename error");
    }
}

/**
 * @brief find + xargs 并行处理文件删除
 * @param dir
 */
void deleteDirectory(const std::string &dir) {
    // 并行删除所有文件
    std::string command = "find " + dir + " -type f -print0 | xargs -0 rm -f";
    if (std::system(command.c_str()) != 0) {
        std::cerr << "Failed to remove directory " << dir << std::endl;
    }

    // 删除空目录
    command = "find " + dir + "-depth -type d -exec rmdir {} \\\\;";
    if (std::system(command.c_str()) != 0) {
        std::cerr << "Failed to remove directory " << dir << std::endl;
    }
}

void traverseDirectory(const std::string &path, const std::function<void(const std::string&)>& fileCallback) {
    DIR *dir = opendir(path.c_str());
    if (!dir) {
        std::cout << "Failed to open directory " << dir << std::endl;
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        // 忽略特殊目录 . 和 ..
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        std::string fileName = path + "/" + entry->d_name;
        if (entry->d_type == DT_DIR) {
            traverseDirectory(fileName, fileCallback);
        }
        else {
            fileCallback(fileName);
        }
    }
    closedir(dir);
}

class FileMapping {
public:
    explicit FileMapping(const std::string &name) {
        fd = open(name.c_str(), O_RDWR | O_CREAT, 0x644);
        if (fd == -1) {
            std::perror(name.c_str());
            throw std::runtime_error("Failed to open file mapping file");
        }
    }

    ~FileMapping() {
        if (nullptr != pAddress) {
            munmap(pAddress, file_state.st_size);
        }
        if (fd != -1) {
            close(fd);
        }
    }

    /**
     * @brief 文件映射
     * @return 内存映射
     */
    [[nodiscard]] char *mapping() {
        if (fstat(fd, &file_state) != 0) {
            return nullptr;
        }

        // 将整个文件映射到内存中
        pAddress = mmap(nullptr, file_state.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (pAddress == MAP_FAILED) {
            return nullptr;
        }
        return static_cast<char *>(pAddress);
    }

    /**
     * @brief 空洞文件，在文件中创建未分配数据区域
     * @param size
     */
    void allocate(int size) const {
#ifdef __linux__
        if (fallocate(fd, 0, 0, size) == -1) {
            std::perror("fallocate error");
            return;
        }
#elif __APPLE__ && __MACH__
        struct fstore fst = {};
        fst.fst_flags = F_ALLOCATECONTIG;  // 预分配连续空间（可选，无则分配非连续）
        fst.fst_posmode = F_PEOFPOSMODE;   // 从文件末尾开始预分配（offset=0）
        fst.fst_length = size;             // 预分配的字节数
        if (fcntl(fd, F_PREALLOCATE, &fst) == -1) {
            std::perror("fm_preallocate");
            return;
        }
        ftruncate(fd, fst.fst_length); // 扩展文件逻辑大小到预分配的size
#endif
    }

private:
    void *pAddress = nullptr;
    struct stat file_state{};
    int fd;
};

}

#endif //SOURCE_FILE_H