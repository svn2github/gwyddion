#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define ntohlb(b) \
    (((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) \
      | ((uint32_t)b[2] << 8) | ((uint32_t)b[3]))

static const uint8_t SIGNATURE[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a
};

enum { SIGNATURE_LEN = sizeof(SIGNATURE), IHDR_LEN = 13 };

static int
print_file(FILE *fh,
           const char *name,
           bool printname)
{
    uint8_t buffer[SIGNATURE_LEN + IHDR_LEN + 8];
    uint8_t *p = buffer;
    uint32_t size, width, height;

    if (fread(buffer, sizeof(buffer), 1, fh) != 1) {
        fprintf(stderr, "Cannot read file `%s' or it's too short: %s\n",
                name, strerror(errno));
        return 1;
    }
    p = buffer;

    if (memcmp(p, SIGNATURE, sizeof(SIGNATURE)) != 0) {
        fprintf(stderr, "File `%s' PNG signature does not match\n", name);
        return 1;
    }
    p += sizeof(SIGNATURE);

    /* According to specs, IHDR must come first */
    size = ntohlb(p);
    if (size != IHDR_LEN) {
        fprintf(stderr, "First chunk len of `%s' is not %u\n", name, IHDR_LEN);
        return 1;
    }
    p += 4;

    if (memcmp(p, "IHDR", 4) != 0) {
        fprintf(stderr, "First chunk of `%s' is `%.4s' instead of IHDR\n",
                name, p);
        return 1;
    }
    p += 4;

    width = ntohlb(p);
    p += 4;
    height = ntohlb(p);
    printf("%s%sw=%u h=%u\n",
           printname ? name : "",
           printname ? ": " : "",
           width, height);

    return 0;
}

int
main(int argc,
     char *argv[])
{
    int failures = 0;

    if (argc == 1)
        failures += print_file(stdin, "stdin", false);
    else {
        int i;

        for (i = 1; i < argc; i++) {
            FILE *fh;

            fh = fopen(argv[i], "rb");
            if (fh) {
                failures += print_file(fh, argv[i], true);
                fclose(fh);
            }
            else {
                failures++;
                fprintf(stderr, "Cannot open file `%s': %s\n",
                        argv[i], strerror(errno));
            }
        }
    }

    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
