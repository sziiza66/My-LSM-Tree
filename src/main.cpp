#ifndef NDEBUG
#include "tests.h"
#endif

int main() {
#ifndef NDEBUG
    Test_All();
#endif

    return 0;
}
