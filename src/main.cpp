#include <cstdio>
#include <cstring>

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--version") == 0) {
        std::printf("Traveler. v0.1.0-alpha\n");
        return 0;
    }
    std::printf("Traveler. v0.1.0-alpha\n");
    return 0;
}
