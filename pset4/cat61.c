#include "io61.h"

int main(int argc, char **argv) {
    const char *in_filename = argc >= 2 ? argv[1] : NULL;
    io61_file *inf = io61_open_check(in_filename, O_RDONLY);
    io61_file *outf = io61_fdopen(STDOUT_FILENO, O_WRONLY);

    while (1) {
        int ch = io61_readc(inf);
        if (ch == EOF)
            break;
        io61_writec(outf, ch);
    }

    io61_close(inf);
    io61_close(outf);
}
