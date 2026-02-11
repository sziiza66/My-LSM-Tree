#ifndef NDEBUG
#include "tests.h"
#endif

int main() {
#ifndef NDEBUG
    Test_Last();
#endif

    return 0;
}
