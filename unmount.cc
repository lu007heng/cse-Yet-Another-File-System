#include <fuse_lowlevel.h>

int main (int argc, char **argv)
{
    fuse_unmount(argv[1]);
    return 0;
}
