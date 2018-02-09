#include "io61.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>

// io61_file
//    Data structure for io61 file wrappers. Add your own stuff.

struct io61_file {
    FILE *f;
};


// io61_fdopen(fd, mode)
//    Return a new io61_file that reads from and/or writes to the given
//    file descriptor `fd`. `mode` is either O_RDONLY for a read-only file
//    or O_WRONLY for a write-only file. You need not support read/write
//    files.

io61_file *io61_fdopen(int fd, int mode) {
    assert(fd >= 0);
    io61_file *f = (io61_file *) malloc(sizeof(io61_file));
    f->f = fdopen(fd, mode == O_RDONLY ? "r" : "w");
    return f;
}


// io61_close(f)
//    Close the io61_file `f`.

int io61_close(io61_file *f) {
    io61_flush(f);
    int r = fclose(f->f);
    free(f);
    return r;
}


// io61_readc(f)
//    Read a single (unsigned) character from `f` and return it. Returns EOF
//    (which is -1) on error or end-of-file.

int io61_readc(io61_file *f) {
    return fgetc(f->f);
}


// io61_writec(f)
//    Write a single character `ch` to `f`. Returns 0 on success or
//    -1 on error.

int io61_writec(io61_file *f, int ch) {
    return fputc(ch, f->f);
}


// io61_flush(f)
//    Forces a write of any `f` buffers that contain data.

int io61_flush(io61_file *f) {
    return fflush(f->f);
}


// io61_read(f, buf, sz)
//    Read up to `sz` characters from `f` into `buf`. Returns the number of
//    characters read on success; normally this is `sz`. Returns a short
//    count if the file ended before `sz` characters could be read. Returns
//    -1 an error occurred before any characters were read.

ssize_t io61_read(io61_file *f, char *buf, size_t sz) {
    size_t n = fread(buf, 1, sz, f->f);
    return n ? (ssize_t) n : (feof(f->f) ? 0 : ferror(f->f));
}


// io61_write(f, buf, sz)
//    Write `sz` characters from `buf` to `f`. Returns the number of
//    characters written on success; normally this is `sz`. Returns -1 if
//    an error occurred before any characters were written.

ssize_t io61_write(io61_file *f, const char *buf, size_t sz) {
    size_t n = fwrite(buf, 1, sz, f->f);
    return n ? (ssize_t) n : (feof(f->f) ? 0 : ferror(f->f));
}


// io61_seek(f, pos)
//    Change the file pointer for file `f` to `pos` bytes into the file.
//    Returns 0 on success and -1 on failure.

int io61_seek(io61_file *f, size_t pos) {
    return fseek(f->f, pos, SEEK_SET);
}


// You should not need to change either of these functions.

// io61_open_check(filename, mode)
//    Open the file corresponding to `filename` and return its io61_file.
//    If `filename == NULL`, returns either the standard input or the
//    standard output, depending on `mode`. Exits with an error message if
//    `filename != NULL` and the named file cannot be opened.

io61_file *io61_open_check(const char *filename, int mode) {
    int fd;
    if (filename)
        fd = open(filename, mode);
    else if (mode == O_RDONLY)
        fd = STDIN_FILENO;
    else
        fd = STDOUT_FILENO;
    if (fd < 0) {
        fprintf(stderr, "%s: %s\n", filename, strerror(errno));
        exit(1);
    }
    return io61_fdopen(fd, mode);
}


// io61_filesize(f)
//    Return the number of bytes in `f`. Returns -1 if `f` is not a seekable
//    file (for instance, if it is a pipe).

ssize_t io61_filesize(io61_file *f) {
    struct stat s;
    int r = fstat(fileno(f->f), &s);
    if (r >= 0 && S_ISREG(s.st_mode) && s.st_size <= SSIZE_MAX)
        return s.st_size;
    else
        return -1;
}
