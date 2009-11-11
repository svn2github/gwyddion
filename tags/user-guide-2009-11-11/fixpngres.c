#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <png.h>

enum {
    OK = 0,
    FIXED = -1,
    FAILED = 1
};

static int
process_file(FILE *fr, const char *rname,
             FILE *fw, const char *wname)
{
    enum { MAGIC_BYTES = 8, RESOLUTION = 5906 };

    png_structp reader, writer;
    png_infop reader_info, writer_info;
    png_byte magic[MAGIC_BYTES];
    png_uint_32 xres, yres;
    png_byte **rows;
    int units;

    /* Check magic */
    if (fread(magic, 1, MAGIC_BYTES, fr) != MAGIC_BYTES
        || png_sig_cmp(magic, 0, MAGIC_BYTES)) {
        fprintf(stderr, "file %s is not PNG\n", rname);
        return FAILED;
    }

    /* Create reader */
    reader = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (!reader) {
        printf("libpng error: png_create_read_struct() failed\n", stderr);
        return FAILED;
    }

    reader_info = png_create_info_struct(reader);
    if (!reader_info) {
        png_destroy_read_struct(&reader, NULL, NULL);
        fputs("libpng error: reader png_create_info_struct() failed\n", stderr);
        return FAILED;
    }

    if (setjmp(png_jmpbuf(reader))) {
        fprintf(stderr,
                "libpng error: reader png_jmpbuf() returned nonzero for `%s'\n",
                rname);
        png_destroy_read_struct(&reader, &reader_info, NULL);
        return FAILED;
    }

    /* Read PNG */
    png_init_io(reader, fr);
    png_set_sig_bytes(reader, MAGIC_BYTES);
    png_set_keep_unknown_chunks(reader, 3, NULL, 0);
    png_read_png(reader, reader_info, PNG_TRANSFORM_EXPAND, NULL);

    /* Check if pHYs is OK to avoid unecessary processing */
    if (png_get_pHYs(reader, reader_info, &xres, &yres, &units)
        && xres == RESOLUTION && yres == RESOLUTION && units == 1) {
        png_destroy_read_struct(&reader, &reader_info, NULL);
        fprintf(stderr, "%s: already OK\n", rname);
        return OK;
    }

    /* Create writer */
    writer = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                     NULL, NULL, NULL);

    if (!writer) {
        png_destroy_read_struct(&reader, &reader_info, NULL);
        fputs("libpng error: png_create_write_struct() failed\n", stderr);
        return FAILED;
    }

    writer_info = png_create_info_struct(writer);
    if (!writer_info) {
        png_destroy_read_struct(&writer, NULL, NULL);
        fputs("libpng error: writer png_create_info_struct() failed\n", stderr);
        return FAILED;
    }

    if (setjmp(png_jmpbuf(writer))) {
        fprintf(stderr,
                "libpng error: writer png_jmpbuf() returned nonzero for `%s'\n",
                wname);
        png_destroy_write_struct(&writer, &writer_info);
        png_destroy_read_struct(&reader, &reader_info, NULL);
        return FAILED;
    }

    png_init_io(writer, fw);

    /* Copy chunks */
    {
        png_uint_32 width, height;
        int bit_depth, color_type, interlace_type, compression_type,
            filter_type;

        png_get_IHDR(reader, reader_info, &width, &height,
                     &bit_depth, &color_type, &interlace_type,
                     &compression_type, &filter_type);
        png_set_IHDR(writer, writer_info, width, height,
                     bit_depth, color_type, interlace_type,
                     compression_type, filter_type);
    }

    /* Set pHYs */
    png_set_pHYs(writer, writer_info, RESOLUTION, RESOLUTION, 1);

    /* Copy data */
    rows = png_get_rows(reader, reader_info);
    png_set_rows(writer, writer_info, rows);

    /* Write PNG */
    png_set_compression_level(writer, Z_BEST_COMPRESSION);
    png_write_png(writer, writer_info, PNG_TRANSFORM_IDENTITY, NULL);

    /* Cleanup */
    png_destroy_write_struct(&writer, &writer_info);
    png_destroy_read_struct(&reader, &reader_info, NULL);

    fprintf(stderr, "%s: pHYs corrected\n", rname);

    return FIXED;
}

int
main(int argc,
     char *argv[])
{
    int failures = 0;
    int pid = getpid();

    if (argc == 1)
        failures += process_file(stdin, "stdin", stdout, "stdout");
    else {
        int i;

        for (i = 1; i < argc; i++) {
            FILE *fr, *fw;
            char *tmpfname;
            int status = 1;

            fr = fopen(argv[i], "rb");
            if (!fr) {
                failures++;
                fprintf(stderr, "Cannot open file `%s': %s\n",
                        argv[i], strerror(errno));
                continue;
            }
            asprintf(&tmpfname, "%s.%d.tmp", argv[i], pid);
            fw = fopen(tmpfname, "wb");
            if (fw) {
                status = process_file(fr, argv[i], fw, tmpfname);
                if (status > 0)
                    failures++;
                fclose(fw);
            }
            else {
                failures++;
                fprintf(stderr, "Cannot open file `%s': %s\n",
                        argv[i], strerror(errno));
            }
            fclose(fr);
            if (status == FIXED) {
                if (rename(tmpfname, argv[i]) != 0) {
                    fprintf(stderr, "Cannot rename `%s' to `%s': %s\n",
                            tmpfname, argv[i], strerror(errno));
                    failures++;
                    unlink(tmpfname);
                }
            }
            else {
                unlink(tmpfname);
            }
            free(tmpfname);
        }
    }

    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}

