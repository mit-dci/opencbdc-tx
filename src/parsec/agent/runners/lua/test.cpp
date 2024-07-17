#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <filesystem>

int main() {
	if (chroot(".")!=0) {
        std::cout << "[TEST] failed to chroot" << std::endl;
    } else {
         std::cout << "[TEST] chrooted proper" << std::endl;
    }

    /* system("./forbid.sh");

    if (chroot(".")!=0) {
        std::cout << "[TEST] failed to chroot" << std::endl;
    } else {
         std::cout << "[TEST] chrooted proper" << std::endl;
    }

    return -1; */
}
