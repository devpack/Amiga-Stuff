/*
 * rom-swizzle.c
 * 
 * Byte-swap/split/dupe/decrypt Kickstart ROM files for EPROM programming.
 * 
 * Written & released by Keir Fraser <keir.xen@gmail.com>
 * 
 * This is free and unencumbered software released into the public domain.
 * See the file COPYING for more details, or visit <http://unlicense.org>.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>

static uint16_t _swap(uint16_t x)
{
    return (x >> 8) | (x << 8);
}

static void usage(int rc)
{
    printf("Usage: rom-swizzle [options] in_file out_file\n");
    printf("Options:\n");
    printf("  -h, --help      Display this information\n");
    printf("  -k, --key=FILE  Key file for encrypted ROM\n");
    printf("  -s, --swap      Swap endianess (little vs big endian)\n");
    printf("  -S, --split     Split into two 16-bit ROM files\n");
    exit(rc);
}

int main(int argc, char **argv)
{
    int ch, fd, swap = 0, split = 0;
    int insz, i, j, is_encrypted;
    uint8_t *buf, *outbuf[2], header[11];
    char *in, *out, *keyfile = NULL;

    const static char sopts[] = "hk:sS";
    const static struct option lopts[] = {
        { "help", 0, NULL, 'h' },
        { "key", 1, NULL, 'k' },
        { "swap", 0, NULL, 's' },
        { "split", 0, NULL, 'S' },
        { 0, 0, 0, 0 }
    };

    while ((ch = getopt_long(argc, argv, sopts, lopts, NULL)) != -1) {
        switch (ch) {
        case 'h':
            usage(0);
            break;
        case 'k':
            keyfile = optarg;
            break;
        case 's':
            swap = 1;
            break;
        case 'S':
            split = 1;
            break;
        default:
            usage(1);
            break;
        }
    }

    if (argc != (optind + 2))
        usage(1);
    in = argv[optind];
    out = argv[optind+1];

    fd = open(in, O_RDONLY);
    if (fd == -1)
        err(1, "%s", in);
    if ((insz = lseek(fd, 0, SEEK_END)) < 0)
        err(1, NULL);
    lseek(fd, 0, SEEK_SET);
    if (insz < 8192)
        errx(1, "Input file too short");
    if (read(fd, header, sizeof(header)) != sizeof(header))
        err(1, NULL);
    is_encrypted = !strncmp((char *)header, "AMIROMTYPE1", sizeof(header));
    if (is_encrypted) {
        insz -= sizeof(header);
    } else {
        lseek(fd, 0, SEEK_SET);
    }
    if ((buf = malloc(insz)) == NULL)
        err(1, NULL);
    if (read(fd, buf, insz) != insz)
        err(1, NULL);
    close(fd);

    if (is_encrypted && !keyfile)
        errx(1, "Must specify keyfile for encrypted ROM");
    if (!is_encrypted && keyfile)
        errx(1, "Expected encrypted ROM");
    if ((insz & (insz-1)) != 0)
        errx(1, "Kickstart image must be power-of-two sized");

    printf("Input: '%s'", in);
    if (swap)
        printf("; Swap");
    if (split)
        printf("; Split");
    if (keyfile)
        printf("; Decrypt(\"%s\")", keyfile);
    printf("\n");

    if (keyfile) {
        unsigned char *key;
        int keysz;
        fd = open(keyfile, O_RDONLY);
        if (fd == -1)
            err(1, "%s", keyfile);
        if ((keysz = lseek(fd, 0, SEEK_END)) < 0)
            err(1, NULL);
        lseek(fd, 0, SEEK_SET);
        if ((key = malloc(keysz)) == NULL)
            err(1, NULL);
        if (read(fd, key, keysz) != keysz)
            err(1, NULL);
        close(fd);
        for (i = j = 0; i < insz; i++, j = (j + 1) % keysz)
            buf[i] ^= key[j];
        free(key);
    }

    if (swap) {
        uint16_t *p = (uint16_t *)buf;
        for (i = 0; i < insz/2; i++, p++)
            *p = _swap(*p);
    }

    if (split) {
        char outname[256];
        uint16_t *a, *b, *p;
        outbuf[0] = malloc(insz);
        outbuf[1] = malloc(insz);
        for (j = 0; j < 2; j++) {
            p = (uint16_t *)buf;
            a = (uint16_t *)outbuf[0];
            b = (uint16_t *)outbuf[1];
            for (i = 0; i < insz/2; i++) {
                *a++ = *p++;
                *b++ = *p++;
            }
        }

        snprintf(outname, sizeof(outname), "%s_a", out);
        fd = open(outname, O_WRONLY|O_CREAT|O_TRUNC, 0644);
        if (fd == -1)
            err(1, "%s", outname);
        if (write(fd, outbuf[0], insz) != insz)
            err(1, NULL);
        close(fd);

        snprintf(outname, sizeof(outname), "%s_b", out);
        fd = open(outname, O_WRONLY|O_CREAT|O_TRUNC, 0644);
        if (fd == -1)
            err(1, "%s", outname);
        if (write(fd, outbuf[1], insz) != insz)
            err(1, NULL);
        close(fd);
    } else {
        fd = open(out, O_WRONLY|O_CREAT|O_TRUNC, 0644);
        if (fd == -1)
            err(1, "%s", out);
        if (write(fd, buf, insz) != insz)
            err(1, NULL);
        close(fd);
    }

    return 0;
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
