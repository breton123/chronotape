// Minimal test: writes to file and stderr. Compile: g++ -o test_output test_output.cpp
#include <cstdio>
#include <fstream>

int main() {
    std::ofstream f("test_output.txt");
    f << "hello\n";
    f.close();

    std::fprintf(stderr, "DONE\n");
    return 0;
}
