/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
/* Verify that toku_os_full_pwrite does the right thing when writing beyond 4GB.  */
#include <test.h>
#include <fcntl.h>
#include <toku_assert.h>
#include <string.h>
#include <stdio.h>

static int iszero(char *cp, size_t n) {
    size_t i;
    for (i=0; i<n; i++)
        if (cp[i] != 0) 
	    return 0;
    return 1;
}

int test_main(int argc, char *const argv[]) {
    assert(argc==2); // first arg is the directory to put the data file into.
    char short_fname[] = "pwrite4g.data";
    int fname_len = strlen(short_fname) + strlen(argv[1]) + 5;
    char fname[fname_len];
    snprintf(fname, fname_len, "%s/%s", argv[1], short_fname);
    int r;
    unlink(fname);
    int fd = open(fname, O_RDWR | O_CREAT | O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO);
    assert(fd>=0);
    char buf[] = "hello";
    int64_t offset = (1LL<<32) + 100;
    toku_os_full_pwrite(fd, buf, sizeof buf, offset);
    char newbuf[sizeof buf];
    r = pread(fd, newbuf, sizeof newbuf, 100);
    assert(r==sizeof newbuf);
    assert(iszero(newbuf, sizeof newbuf));
    r = pread(fd, newbuf, sizeof newbuf, offset);
    assert(r==sizeof newbuf);
    assert(memcmp(newbuf, buf, sizeof newbuf) == 0);
    int64_t fsize;
    r = toku_os_get_file_size(fd, &fsize);
    assert(r == 0);
    assert(fsize > 100 + (signed)sizeof(buf));
    r = close(fd);
    assert(r==0);
    return 0;
}