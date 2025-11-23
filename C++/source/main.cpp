#include <iostream>
#include "file.h"
#include "thread.h"

int main() {
    CodeGuide::traverseDirectory(".", [](const std::string& filename) {
        std::cout << filename << std::endl;
    });

    CodeGuide::thread_test();
    return 0;
}
