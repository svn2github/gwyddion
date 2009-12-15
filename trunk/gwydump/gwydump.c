/*
 *  @(#) $Id$
 *  Copyright (C) 2005,2006 David Necas (Yeti)
 *  E-mail: yeti@gwyddion.net
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <glib.h>

/* Borrow types from GObject to convert old 1.x GwyContainers */
#define G_TYPE_FUNDAMENTAL_SHIFT (2)
#define G_TYPE_MAKE_FUNDAMENTAL(x) (((x) << G_TYPE_FUNDAMENTAL_SHIFT))

#define G_TYPE_UCHAR   G_TYPE_MAKE_FUNDAMENTAL (4)
#define G_TYPE_BOOLEAN G_TYPE_MAKE_FUNDAMENTAL (5)
#define G_TYPE_INT     G_TYPE_MAKE_FUNDAMENTAL (6)
#define G_TYPE_INT64   G_TYPE_MAKE_FUNDAMENTAL (10)
#define G_TYPE_DOUBLE  G_TYPE_MAKE_FUNDAMENTAL (15)
#define G_TYPE_STRING  G_TYPE_MAKE_FUNDAMENTAL (16)
#define G_TYPE_OBJECT  G_TYPE_MAKE_FUNDAMENTAL (20)

#define U64F "%" G_GUINT64_FORMAT

typedef struct {
    guint version;
    gboolean extract_succeeded;
    gchar *buffer0;
    gchar *buffer;
    GString *path;
    GArray *stack;
} DumpState;

static void print_help      (void);
static void process_options (int *argc, char ***argv);
static void dump_object     (DumpState *state, gsize *size);
static void dump_boxed      (DumpState *state, gsize *size);
static void print           (DumpState *state,
                             const gchar *format,
                             ...);

/* Options */
static guint opt_indent = 4;
static gint opt_array_length = 0;
static gint opt_depth = -1;
static gboolean opt_offset = FALSE;
static gboolean opt_object_size = FALSE;
static gboolean opt_type = FALSE;
static gboolean opt_value = FALSE;
static gboolean opt_raw = FALSE;
static gboolean opt_path = FALSE;
static gboolean opt_quad = FALSE;
static gchar *opt_extract = NULL;

int
main(int argc, char *argv[])
{
    DumpState state;
    gsize size;
    GError *err = NULL;

    process_options(&argc, &argv);
    if (argc < 2) {
        print_help();
        return 0;
    }

    if (!g_file_get_contents(argv[1], &state.buffer0, &size, &err)) {
        fprintf(stderr, "Cannot open `%s': %s\n", argv[1], g_strerror(errno));
        return EXIT_FAILURE;
    }

    state.buffer = state.buffer0;
    state.version = opt_quad ? 3 : 2;
    state.extract_succeeded = FALSE;
    state.path = g_string_new(NULL);
    state.stack = g_array_new(FALSE, FALSE, sizeof(gint));
    /* Detect file type, if not raw */
    if (!opt_raw) {
        if (size < 10
            || (strncmp(state.buffer0, "GWYP", 4)
                && strncmp(state.buffer0, "GWYO", 4)
                && strncmp(state.buffer0, "GWYQ", 4))) {
            fprintf(stderr, "File `%s' is not a Gwyddion file\n", argv[1]);
            return EXIT_FAILURE;
        }
        if (strncmp(state.buffer0, "GWYO", 4) == 0)
            state.version = 1;
        else if (strncmp(state.buffer0, "GWYP", 4) == 0)
            state.version = 2;
        else if (strncmp(state.buffer0, "GWYQ", 4) == 0)
            state.version = 3;

        if (opt_offset)
            print(&state, "%08lx: ", (gulong)(state.buffer - state.buffer0));
        print(&state, "Header %.4s\n", state.buffer0);
        state.buffer += 4;
        size -= 4;
    }

    /* Special case this, object that has no component name breaks all the
     * normal printing assumptions. */
    if (opt_offset)
        print(&state, "%08lx: ", (gulong)(state.buffer - state.buffer0));
    print(&state, "\"\"");
    dump_object(&state, &size);

    return (opt_extract && !state.extract_succeeded) ? 2 : EXIT_SUCCESS;
}

static void G_GNUC_NORETURN
fail(DumpState *state,
     const gchar *format,
     ...)
{
    va_list ap;

    fprintf(stderr, "\nError at position %08lx: ",
            (gulong)(state->buffer - state->buffer0));
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fprintf(stderr, "\n");

    exit(EXIT_FAILURE);
}

/* Print to stdout, except when disabled by options */
static void
print(DumpState *state,
      const gchar *format,
      ...)
{
    va_list ap;

    if (opt_extract
        || state->stack->len > opt_depth)
        return;

    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
}

/* Print indentation corresponding to the current state->stack nesting level */
static void
print_indent(DumpState *state)
{
    static gchar *spacer = NULL;
    guint i;

    if (!opt_indent
        || opt_extract
        || state->stack->len >= opt_depth)
        return;

    if (!spacer) {
        spacer = g_new(gchar, opt_indent + 1);
        memset(spacer, ' ', opt_indent);
        spacer[opt_indent] = '\0';
    }

    for (i = 0; i <= state->stack->len; i++)
        fputs(spacer, stdout);
}

/* Push one component/item to component stack */
static void
push(DumpState *state,
     const gchar *format,
     ...)
{
    va_list ap;
    gchar *s;
    guint len;

    va_start(ap, format);
    s = g_strdup_vprintf(format, ap);
    va_end(ap);

    len = state->path->len;
    g_array_append_val(state->stack, len);
    g_string_append_c(state->path, '/');
    g_string_append(state->path, s);

    print(state, "%s", opt_path ? state->path->str : s);
    g_free(s);
}

/* Pop one component/item from component stack */
static void
pop(DumpState *state)
{
    guint len;

    if (!state->stack->len)
        fail(state, "Internal error, no more levels to pop");
    len = g_array_index(state->stack, guint, state->stack->len-1);
    if (len > state->path->len)
        fail(state, "Internal error, deeper path is shorter");
    g_array_set_size(state->stack, state->stack->len-1);
    g_string_truncate(state->path, len);
}

static void
print_offset(DumpState *state)
{
    if (opt_extract
        || state->stack->len >= opt_depth)
        return;

    if (opt_offset)
        printf("%08lx: ", (gulong)(state->buffer - state->buffer0));
    print_indent(state);
}

static const gchar*
format_char(guchar c)
{
    static gchar buf[6];

    if (g_ascii_isprint(c)) {
        buf[0] = c;
        buf[1] = '\0';
    }
    else
        g_snprintf(buf, sizeof(buf), "\\x%02x", c);
    return buf;
}

static void
print_value(DumpState *state,
            const gchar *type,
            const gchar *format,
            ...)
{
    va_list ap;

    if (opt_extract
        || state->stack->len > opt_depth)
        return;

    if (opt_type || opt_value)
        putchar(' ');

    if (opt_type)
        fputs(type, stdout);
    if (!opt_value) {
        putchar('\n');
        return;
    }

    if (opt_type)
        putchar('=');
    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);

    putchar('\n');
}

static inline void
get_value_real(DumpState *state,
               gsize *size,
               guint typesize,
               const gchar *typename,
               gchar *value)
{
    if (*size < typesize)
        fail(state, "Truncated %s", typename);
    memcpy(value, state->buffer, typesize);
#if (G_BYTE_ORDER == G_BIG_ENDIAN)
    {
        guint i;
        gchar x;

        for (i = 0; i < typesize/2; i++) {
            x = value[i];
            value[i] = value[typesize-1 - i];
            value[typesize-1 - i] = x;
        }
    }
#endif
    state->buffer += typesize;
    *size -= typesize;
}

#define get_value(state, size, type, name, value) \
    get_value_real(state, size, sizeof(type), name, (void*)&value)

#define dump_value(state, size, type, name, format, toprint) \
    do { \
        type value; \
        if (opt_extract && strcmp(state->path->str, opt_extract) == 0) { \
            fwrite(state->buffer, 1, sizeof(type), stdout); \
            state->extract_succeeded = TRUE; \
        } \
        get_value_real(state, size, sizeof(type), name, (void*)&value); \
        print_value(state, name, format, toprint); \
    } while (0)

static guint64
get_size(DumpState *state,
         gsize *size)
{
    guint64 value;
    if (state->version == 3)
        get_value(state, size, guint64, "int64", value);
    else {
        guint32 value32;
        get_value(state, size, guint32, "int32", value32);
        value = value32;
    }
    return value;
};

static void
dump_parent(DumpState *state,
            gsize *size)
{
    if (opt_extract
        || state->stack->len > opt_depth)
        return;

    if (opt_type)
        fputs(" parent", stdout);

    putchar('\n');
}

static void
dump_boolean(DumpState *state,
             gsize *size)
{
    dump_value(state, size, gchar, "boolean", "%s", value ? "TRUE" : "FALSE");
}

static void
dump_char(DumpState *state,
          gsize *size)
{
    dump_value(state, size, gchar, "char", format_char(value), value);
}

static void
dump_int16(DumpState *state,
           gsize *size)
{
    dump_value(state, size, gint16, "int16", "%d", value);
}

static void
dump_int32(DumpState *state,
           gsize *size)
{
    dump_value(state, size, gint32, "int32", "%d", value);
}

static void
dump_int64(DumpState *state,
           gsize *size)
{
    dump_value(state, size, gint64, "int64", "%" G_GINT64_FORMAT, value);
}

static void
dump_double(DumpState *state,
            gsize *size)
{
    dump_value(state, size, gdouble, "double", "%g", value);
}

static void
dump_string(DumpState *state,
            gsize *size)
{
    gchar *p, *q;

    if (!(p = memchr(state->buffer, 0, *size)))
        fail(state, "Runaway string");
    if (opt_extract && strcmp(state->path->str, opt_extract) == 0) {
        printf("%s", state->buffer);
        putchar('\0');
        state->extract_succeeded = TRUE;
    }
    q = g_strescape(state->buffer, NULL);
    print_value(state, "string", "\"%s\"", q);
    g_free(q);
    *size -= (p - state->buffer) + 1;
    state->buffer = p + 1;
}

static gboolean
dump_array(DumpState *state,
           gsize *size,
           gchar ctype)
{
    static struct {
        gchar ctype;
        const gchar *name;
        const gsize size;
        void (*func)(DumpState*, gsize*);
    } array[] = {
        { 'B', "boolean", sizeof(gchar),   dump_boolean, },
        { 'C', "char",    sizeof(gchar),   dump_char,    },
        { 'D', "double",  sizeof(gdouble), dump_double,  },
        { 'H', "int16",   sizeof(gint16),  dump_int16,   },
        { 'I', "int32",   sizeof(gint32),  dump_int32,   },
        { 'O', "object",  0,               dump_object,  },
        { 'Q', "int64",   sizeof(gint64),  dump_int64,   },
        { 'S', "string",  0,               dump_string,  },
    };

    guint64 mysize, i, limit, type;
    gchar *p, *start;

    /* Find the type */
    for (type = 0; type < G_N_ELEMENTS(array); type++) {
        if (array[type].ctype == ctype)
            break;
    }
    if (type == G_N_ELEMENTS(array))
        return FALSE;

    /* Print the header line */
    mysize = get_size(state, size);
    if (opt_value || opt_type) {
        if (opt_type)
            print(state, " %s", array[type].name);
        print(state, " array");
        if (opt_value)
            print(state, " of length " U64F, mysize);
    }
    print(state, "\n");

    /* Check sizes */
    start = state->buffer;
    limit = (ctype == 'O') ? mysize : MIN(mysize, opt_array_length);
    if (*size < mysize*array[type].size)
        fail(state, "Truncated %s array", array[type].name);

    /* Extract individual array items; this is a bit crude, we simply force
     * individual processing of the items to extract and let the particular
     * dump function take care about the actual extraction */
    if (opt_extract && g_str_has_prefix(opt_extract, state->path->str)) {
        p = opt_extract + state->path->len;
        if (sscanf(p, "/[" U64F "]", &i) == 1
            && (p = strchr(p, ']'))
            && p[1] == '\0'
            && i < mysize)
            limit = MAX(limit, i + 1);
    }

    /* Print at most limit array items */
    for (i = 0; i < limit; i++) {
        print_offset(state);
        push(state, "[" U64F "]", i);
        array[type].func(state, size);
        pop(state);
    }
    /* Print ellipsis for remaining items */
    if (opt_array_length && i < mysize) {
        print_offset(state);
        push(state, "[" U64F ".." U64F "]", i, mysize-1);
        print_value(state, array[type].name, "...");
        pop(state);
    }

    /* Skip remaining items */
    switch (ctype) {
        case 'O':
        /* Objects are always printed all */
        if (i < mysize)
            fail(state, "Internal error, not all objects processed");
        break;

        case 'S':
        for ( ; i < mysize; i++) {
            if (!(p = memchr(state->buffer, 0, *size)))
                fail(state, "Runaway string");
            *size -= (p - state->buffer) + 1;
            state->buffer = p + 1;
        }
        break;

        default:
        state->buffer += (mysize - i)*array[type].size;
        *size -= (mysize - i)*array[type].size;
        break;
    }

    /* Extract */
    if (opt_extract && strcmp(state->path->str, opt_extract) == 0) {
        fwrite(start, 1, state->buffer - start, stdout);
        state->extract_succeeded = TRUE;
    }

    return TRUE;
}

static gchar
gtype_to_ctype(guint32 gtype)
{
    switch (gtype) {
        case G_TYPE_UCHAR: return 'c'; break;
        case G_TYPE_BOOLEAN: return 'b'; break;
        case G_TYPE_INT: return 'i'; break;
        case G_TYPE_INT64: return 'q'; break;
        case G_TYPE_DOUBLE: return 'd'; break;
        case G_TYPE_STRING: return 's'; break;
        case G_TYPE_OBJECT: return 'o'; break;
    }
    return 0;
}

static void
dump_hash(DumpState *state,
          gsize size,
          gboolean oldfile,
          gboolean forbid_objects)
{
    static struct {
        gchar ctype;
        void (*func)(DumpState*, gsize*);
    } atomic[] = {
        { '^', dump_parent,  },
        { 'b', dump_boolean, },
        { 'c', dump_char,    },
        { 'd', dump_double,  },
        { 'h', dump_int16,   },
        { 'i', dump_int32,   },
        { 'o', dump_object,  },
        { 'q', dump_int64,   },
        { 's', dump_string,  },
        { 'x', dump_boxed,   },
    };
    gboolean handled;
    guint i;
    guint32 gtype;
    gchar ctype = 0;
    gchar *p, *q;

    while (size) {
        print_offset(state);

        /* Type for old files */
        if (oldfile) {
            gtype = get_size(state, &size);
            if (!(ctype = gtype_to_ctype(gtype)))
                fail(state, "Unknown GLib type %u", gtype);
        }

        /* Name */
        if (!(p = memchr(state->buffer, 0, size)))
            fail(state, "Runaway component name");
        q = g_strescape(state->buffer, NULL);
        push(state, "\"%s\"", q);
        g_free(q);
        size -= (p - state->buffer) + 1;
        state->buffer = p + 1;

        /* Type for new files */
        if (!oldfile) {
            if (!size)
                fail(state, "Truncated component type");
            ctype = *state->buffer;
            state->buffer += sizeof(gchar);
            size -= sizeof(gchar);
            if (forbid_objects && g_ascii_tolower(ctype) == 'o')
                fprintf(stderr, "Warning: object inside boxed type");
        }

        /* Data */
        handled = FALSE;
        for (i = 0; i < G_N_ELEMENTS(atomic); i++) {
            if (atomic[i].ctype == ctype) {
                atomic[i].func(state, &size);
                handled = TRUE;
                break;
            }
        }
        if (!handled)
            handled = dump_array(state, &size, ctype);
        if (!handled)
            fail(state, "Unknown type ‘%s’", format_char(ctype));
        pop(state);
    }
}

static void
dump_object_or_boxed(DumpState *state,
                     gsize *size,
                     gboolean is_boxed)
{
    gboolean container;
    guint64 mysize;
    gchar *p, *start;

    /* Name */
    start = state->buffer;
    if (!(p = memchr(state->buffer, 0, *size)))
        fail(state, "Runaway %s name", is_boxed ? "boxed" : "object");
    if (opt_value)
        print(state, " %s%s",
              opt_type ? (is_boxed ? "boxed=" : "object=") : "", state->buffer);
    else if (opt_type)
        print(state, is_boxed ? " boxed" : " object");
    container = (strcmp(state->buffer, "GwyContainer") == 0);
    *size -= (p - state->buffer) + 1;
    state->buffer = p + 1;

    /* Size */
    mysize = get_size(state, size);
    if (mysize > *size)
        fail(state, "Truncated %s data", is_boxed ? "boxed" : "object");
    if (opt_object_size)
        print(state, " size=%" G_GSIZE_FORMAT, mysize);
    print(state, "\n");

    /* Extract */
    if (opt_extract && strcmp(state->path->str, opt_extract) == 0) {
        fwrite(start, 1, (state->buffer + mysize) - start, stdout);
        state->extract_succeeded = TRUE;
    }

    /* Hash */
    *size -= mysize;
    p = state->buffer + mysize;
    dump_hash(state, mysize, state->version == 1 && container, is_boxed);
    if (state->buffer > p)
        fail(state, "Internal error, object/boxed managed to escape jail");
    if (state->buffer < p) {
        fprintf(stderr, "Warning: padding of length " U64F " found",
                (guint64)(p - state->buffer));
        state->buffer = p;
    }
}

static void
dump_object(DumpState *state,
            gsize *size)
{
    dump_object_or_boxed(state, size, FALSE);
}

static void
dump_boxed(DumpState *state,
           gsize *size)
{
    dump_object_or_boxed(state, size, TRUE);
}

static void
process_options(int *argc,
                char ***argv)
{
    static gboolean opt_version = FALSE;
    static gboolean opt_help = FALSE;
    static gboolean opt_all = FALSE;
    static GOptionEntry entries[] = {
        { "all", 'a', 0, G_OPTION_ARG_NONE, &opt_all, NULL, NULL },
        { "value", 'v', 0, G_OPTION_ARG_NONE, &opt_value, NULL, NULL },
        { "offsets", 'o', 0, G_OPTION_ARG_NONE, &opt_offset, NULL, NULL },
        { "types", 't', 0, G_OPTION_ARG_NONE, &opt_type, NULL, NULL },
        { "sizes", 's', 0, G_OPTION_ARG_NONE, &opt_object_size, NULL, NULL },
        { "length", 'l', 0, G_OPTION_ARG_INT, &opt_array_length, NULL, NULL },
        { "depth", 'd', 0, G_OPTION_ARG_INT, &opt_depth, NULL, NULL },
        { "indent", 'i', 0, G_OPTION_ARG_INT, &opt_indent, NULL, NULL },
        { "paths", 'p', 0, G_OPTION_ARG_NONE, &opt_path, NULL, NULL },
        { "extract", 'x', 0, G_OPTION_ARG_STRING, &opt_extract, NULL, NULL },
        { "raw", 'r', 0, G_OPTION_ARG_NONE, &opt_raw, NULL, NULL },
        { "quad", 'q', 0, G_OPTION_ARG_NONE, &opt_quad, NULL, NULL },
        { "version", 'V', 0, G_OPTION_ARG_NONE, &opt_version, NULL, NULL },
        { "help", 'h', 0, G_OPTION_ARG_NONE, &opt_help, NULL, NULL },
        { NULL }
    };
    GOptionContext *context;
    GError *err = NULL;

    context = g_option_context_new(NULL);
    g_option_context_add_main_entries(context, entries, NULL);
    g_option_context_set_help_enabled(context, FALSE);
    if (!g_option_context_parse(context, argc, argv, &err)) {
        fprintf(stderr, "Cannot parse options: %s\n", err->message);
        g_clear_error(&err);
        exit(EXIT_FAILURE);
    }
    g_option_context_free(context);

    if (opt_version)
        printf("%s %s\n", PACKAGE, VERSION);

    if (opt_help)
        print_help();

    if (opt_help || opt_version)
        exit(EXIT_SUCCESS);

    opt_indent = CLAMP(opt_indent, 0, 65536);
    if (opt_depth == -1)
        opt_depth = G_MAXINT;
    if (opt_array_length == -1)
        opt_array_length = G_MAXINT;
    if (opt_all)
        opt_value = opt_offset = opt_type = opt_object_size = TRUE;
    if (opt_extract) {
        /* Improve scanning speed */
        opt_array_length = 0;
        opt_value = opt_offset = opt_type = opt_object_size = FALSE;
    }
}

static void
print_help(void)
{
    printf(
"Usage: gwdump [OPTIONS...] FILENAME\n"
"Dump Gwyddion .gwy file in a human readable text format.\n\n"
        );
    printf(
"Options:\n"
" -o, --offsets        Print offsets in file.\n"
" -t, --types          Print component types.\n"
" -v, --values         Print values of atomic objects.\n"
" -s, --sizes          Print serialized object sizes.\n"
" -a, --all            Print all above.\n"
" -l, --length=N       Print N initial items of atomic arrays, -1 for all.\n"
" -d, --depth=N        Print only up to nesting depth N, -1 for all.\n"
" -i, --indent=N       Indent each nesting level by N spaces.\n"
" -p, --paths          Print full paths as component names.\n\n"
" -r, --raw            File is raw serialized data, no header.\n"
" -q, --quad           File uses 64bit sizes (automatic for non-raw files).\n"
" -x, --extract=PATH   Extract component PATH to standard output (as raw binary\n"
"                      data) in stead of printing.\n\n"
" -h, --help           Print this help and terminate.\n"
" -V, --version        Print version info and terminate.\n\n"
        );
    printf("Please report bugs to Yeti <yeti@gwyddion.net>\n");
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
