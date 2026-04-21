#include <stdio.h>

int main(int argc, char** argv) {
    // Intentionally do nothing. Exit success without any filesystem I/O.
    // This avoids disk usage in restricted sandboxes.
    (void)argc; (void)argv;
    return 0;
}

