/*
 *  $Id$
 *  Copyright (C) 2009 David Nečas (Yeti).
 *  E-mail: yeti@gwyddion.net.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <string.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include "libgwy/macros.h"
#include "libgwy/strfuncs.h"
#include "libgwy/expr.h"
#include "libgwy/math.h"

#define GWY_EXPR_SCOPE_GLOBAL 0

#define GWY_CSET_UTF8_UPPER \
    "\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f" \
    "\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f" \
    "\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf" \
    "\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf" \
    "\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf" \
    "\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf" \
    "\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef" \
    "\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd"

/* things that can appear on code stack */
typedef enum {
    /* negative values are reserved for variables */
    GWY_EXPR_CODE_CONSTANT = 0,
    GWY_EXPR_CODE_NEGATE = 1,
    GWY_EXPR_CODE_ADD,
    GWY_EXPR_CODE_SUBTRACT,
    GWY_EXPR_CODE_MULTIPLY,
    GWY_EXPR_CODE_DIVIDE,
    GWY_EXPR_CODE_MODULO,
    GWY_EXPR_CODE_POWER,
    GWY_EXPR_CODE_POW,
    GWY_EXPR_CODE_MIN,
    GWY_EXPR_CODE_MAX,
    GWY_EXPR_CODE_FMOD,
    GWY_EXPR_CODE_HYPOT,
    GWY_EXPR_CODE_ATAN2,
    GWY_EXPR_CODE_ABS,
    GWY_EXPR_CODE_FLOOR,
    GWY_EXPR_CODE_CEIL,
    GWY_EXPR_CODE_STEP,
    GWY_EXPR_CODE_SQRT,
    GWY_EXPR_CODE_CBRT,
    GWY_EXPR_CODE_SIN,
    GWY_EXPR_CODE_COS,
    GWY_EXPR_CODE_TAN,
    GWY_EXPR_CODE_ASIN,
    GWY_EXPR_CODE_ACOS,
    GWY_EXPR_CODE_ATAN,
    GWY_EXPR_CODE_EXP,
    GWY_EXPR_CODE_LN,
    GWY_EXPR_CODE_LOG,
    GWY_EXPR_CODE_LOG10,
    GWY_EXPR_CODE_COSH,
    GWY_EXPR_CODE_SINH,
    GWY_EXPR_CODE_TANH,
    GWY_EXPR_CODE_ACOSH,
    GWY_EXPR_CODE_ASINH,
    GWY_EXPR_CODE_ATANH,
    GWY_EXPR_CODE_EXP2,
    GWY_EXPR_CODE_LOG2,
    GWY_EXPR_CODE_ERF,
    GWY_EXPR_CODE_ERFC,
    GWY_EXPR_CODE_LGAMMA,
} GwyExprOpCode;

/* code stack item */
typedef struct {
    GwyExprOpCode type;
    gdouble value;
} GwyExprCode;

typedef struct {
    void (*function)(gdouble**);
    const gchar *name;
    gshort in_values;
    gshort out_values;
    GwyExprOpCode type;  /* consistency check: must be equal to position */
} GwyExprFunction;

/* Transitional tokenizer token:
 * can hold both initial GScanner tokens and final GwyExpr RPN stacks */
typedef struct _GwyExprToken GwyExprToken;
struct _GwyExprToken {
    /* This is either GTokenType or GwyExprOpCode */
    gint token;
    /* GwyExprCode values are stored in value.v_float */
    GTokenValue value;
    struct _GwyExprToken *rpn_block;
    GwyExprToken *prev;
    GwyExprToken *next;
};

struct _GwyExprPrivate {
    GObject g_object;
    /* Global context */
    GString *expr;
    GScanner *scanner;
    GHashTable *constants;
    /* Variables */
    GPtrArray *identifiers;    /* variable names */
    const gdouble *variables;    /* user-filled variable values */
    /* Tokens */
    GwyExprToken *tokens;
    /* Compiled RPN representation */
    GwyExprCode *input;    /* compiled expression */
    guint in;    /* nonzero in is a mark of successful compilation */
    guint ilen;    /* allocated size */
    /* Execution stack */
    gdouble *stack;    /* stack */
    gdouble *sp;    /* stack pointer */
    guint slen;    /* allocated size */
};

typedef struct _GwyExprPrivate Expr;

static void     gwy_expr_finalize      (GObject *object);
static gpointer check_call_table_sanity(gpointer arg);
static void     token_list_delete      (GwyExprToken *tokens);

#define make_function_1_1(name) \
    static void gwy_expr_##name(gdouble **s) { **s = name(**s); }

#define make_function_2_1(name) \
    static void gwy_expr_##name(gdouble **s) { --*s; **s = name(*(*s+1), **s); }

make_function_1_1(sqrt)
make_function_1_1(cbrt)
make_function_1_1(sin)
make_function_1_1(cos)
make_function_1_1(tan)
make_function_1_1(exp)
make_function_1_1(log)
make_function_1_1(log10)
make_function_1_1(exp2)
make_function_1_1(log2)
make_function_1_1(asin)
make_function_1_1(acos)
make_function_1_1(atan)
make_function_1_1(cosh)
make_function_1_1(sinh)
make_function_1_1(tanh)
make_function_1_1(acosh)
make_function_1_1(asinh)
make_function_1_1(atanh)
make_function_1_1(fabs)
make_function_1_1(floor)
make_function_1_1(ceil)
make_function_1_1(erf)
make_function_1_1(erfc)
make_function_1_1(lgamma)
make_function_2_1(pow)
make_function_2_1(hypot)
make_function_2_1(atan2)
make_function_2_1(fmod)

static void gwy_expr_negate(gdouble **s) { **s = -(**s); }
static void gwy_expr_add(gdouble **s) { --*s; **s = *(*s+1) + **s; }
static void gwy_expr_subtract(gdouble **s) { --*s; **s = *(*s+1) - **s; }
static void gwy_expr_multiply(gdouble **s) { --*s; **s = *(*s+1) * **s; }
static void gwy_expr_divide(gdouble **s) { --*s; **s = *(*s+1) / **s; }
static void gwy_expr_max(gdouble **s) { --*s; **s = MAX(*(*s+1), **s); }
static void gwy_expr_min(gdouble **s) { --*s; **s = MIN(*(*s+1), **s); }
static void gwy_expr_step(gdouble **s) { **s = **s > 0.0; }

static const GwyExprFunction call_table[] = {
    { NULL,                NULL,     0,  0,  0                       },
    { gwy_expr_negate,     "~",      1,  1,  GWY_EXPR_CODE_NEGATE,   },
    { gwy_expr_add,        "+",      2,  1,  GWY_EXPR_CODE_ADD,      },
    { gwy_expr_subtract,   "-",      2,  1,  GWY_EXPR_CODE_SUBTRACT, },
    { gwy_expr_multiply,   "*",      2,  1,  GWY_EXPR_CODE_MULTIPLY, },
    { gwy_expr_divide,     "/",      2,  1,  GWY_EXPR_CODE_DIVIDE,   },
    { gwy_expr_fmod,       "%",      2,  1,  GWY_EXPR_CODE_MODULO,   },
    { gwy_expr_pow,        "^",      2,  1,  GWY_EXPR_CODE_POWER,    },
    { gwy_expr_pow,        "pow",    2,  1,  GWY_EXPR_CODE_POW,      },
    { gwy_expr_min,        "min",    2,  1,  GWY_EXPR_CODE_MIN,      },
    { gwy_expr_max,        "max",    2,  1,  GWY_EXPR_CODE_MAX,      },
    { gwy_expr_fmod,       "mod",    2,  1,  GWY_EXPR_CODE_FMOD,     },
    { gwy_expr_hypot,      "hypot",  2,  1,  GWY_EXPR_CODE_HYPOT,    },
    { gwy_expr_atan2,      "atan2",  2,  1,  GWY_EXPR_CODE_ATAN2,    },
    { gwy_expr_fabs,       "abs",    1,  1,  GWY_EXPR_CODE_ABS,      },
    { gwy_expr_floor,      "floor",  1,  1,  GWY_EXPR_CODE_FLOOR,    },
    { gwy_expr_ceil,       "ceil",   1,  1,  GWY_EXPR_CODE_CEIL,     },
    { gwy_expr_step,       "step",   1,  1,  GWY_EXPR_CODE_STEP,     },
    { gwy_expr_sqrt,       "sqrt",   1,  1,  GWY_EXPR_CODE_SQRT,     },
    { gwy_expr_cbrt,       "cbrt",   1,  1,  GWY_EXPR_CODE_CBRT,     },
    { gwy_expr_sin,        "sin",    1,  1,  GWY_EXPR_CODE_SIN,      },
    { gwy_expr_cos,        "cos",    1,  1,  GWY_EXPR_CODE_COS,      },
    { gwy_expr_tan,        "tan",    1,  1,  GWY_EXPR_CODE_TAN,      },
    { gwy_expr_asin,       "asin",   1,  1,  GWY_EXPR_CODE_ASIN,     },
    { gwy_expr_acos,       "acos",   1,  1,  GWY_EXPR_CODE_ACOS,     },
    { gwy_expr_atan,       "atan",   1,  1,  GWY_EXPR_CODE_ATAN,     },
    { gwy_expr_exp,        "exp",    1,  1,  GWY_EXPR_CODE_EXP,      },
    { gwy_expr_log,        "ln",     1,  1,  GWY_EXPR_CODE_LN,       },
    { gwy_expr_log,        "log",    1,  1,  GWY_EXPR_CODE_LOG,      },
    { gwy_expr_log10,      "log10",  1,  1,  GWY_EXPR_CODE_LOG10,    },
    { gwy_expr_cosh,       "cosh",   1,  1,  GWY_EXPR_CODE_COSH,     },
    { gwy_expr_sinh,       "sinh",   1,  1,  GWY_EXPR_CODE_SINH,     },
    { gwy_expr_tanh,       "tanh",   1,  1,  GWY_EXPR_CODE_TANH,     },
    { gwy_expr_acosh,      "acosh",  1,  1,  GWY_EXPR_CODE_ACOSH,    },
    { gwy_expr_asinh,      "asinh",  1,  1,  GWY_EXPR_CODE_ASINH,    },
    { gwy_expr_atanh,      "atanh",  1,  1,  GWY_EXPR_CODE_ATANH,    },
    { gwy_expr_exp2,       "exp2",   1,  1,  GWY_EXPR_CODE_EXP2,     },
    { gwy_expr_log2,       "log2",   1,  1,  GWY_EXPR_CODE_LOG2,     },
    { gwy_expr_erf,        "erf",    1,  1,  GWY_EXPR_CODE_ERF,      },
    { gwy_expr_erfc,       "erfc",   1,  1,  GWY_EXPR_CODE_ERFC,     },
    { gwy_expr_lgamma,     "lgamma", 1,  1,  GWY_EXPR_CODE_LGAMMA,   },
};

/* Maximum number of function arguments */
#define GWY_EXPR_FUNC_MAX_ARGS 2

static const GScannerConfig scanner_config = {
    /* character sets */
    " \t\n\r",
    G_CSET_a_2_z G_CSET_A_2_Z GWY_CSET_UTF8_UPPER,
    G_CSET_a_2_z G_CSET_A_2_Z "0123456789_." GWY_CSET_UTF8_UPPER,
    NULL,
    /* case sensitive */
    TRUE,
    /* comments */
    FALSE, FALSE, FALSE,
    /* identifiers */
    TRUE, TRUE, FALSE, TRUE,
    /* number formats */
    FALSE, FALSE, TRUE, TRUE, FALSE,
    /* strings */
    FALSE, FALSE,
    /* conversions */
    TRUE, TRUE, FALSE, TRUE, FALSE,
    /* options */
    FALSE, FALSE, 0,
};

G_DEFINE_TYPE(GwyExpr, gwy_expr, G_TYPE_OBJECT)

static void
gwy_expr_class_init(GwyExprClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Expr));

    gobject_class->finalize = gwy_expr_finalize;
}

static void
gwy_expr_init(GwyExpr *expr)
{
    static GOnce table_sanity_checked = G_ONCE_INIT;
    g_once(&table_sanity_checked, check_call_table_sanity, NULL);

    expr->priv = G_TYPE_INSTANCE_GET_PRIVATE(expr, GWY_TYPE_EXPR, Expr);
    expr->priv->expr = g_string_new(NULL);
}

static void
gwy_expr_finalize(GObject *object)
{
    Expr *expr = GWY_EXPR(object)->priv;

    token_list_delete(expr->tokens);
    if (expr->identifiers)
        g_ptr_array_free(expr->identifiers, TRUE);
    if (expr->scanner)
        g_scanner_destroy(expr->scanner);
    if (expr->constants)
        g_hash_table_destroy(expr->constants);
    g_string_free(expr->expr, TRUE);
    g_free(expr->input);
    g_free(expr->stack);

    G_OBJECT_CLASS(gwy_expr_parent_class)->finalize(object);
}

static gpointer
check_call_table_sanity(G_GNUC_UNUSED gpointer arg)
{
    gboolean ok = TRUE;
    guint i;

    for (i = 1; i < G_N_ELEMENTS(call_table); i++) {
        if (call_table[i].type != i) {
            g_critical("Inconsistent call table: %u at pos %u\n",
                       call_table[i].type, i);
            ok = FALSE;
        }
    }

    return GINT_TO_POINTER(ok);
}

/**
 * gwy_expr_error_quark:
 *
 * Returns error domain for expression parsing and evaluation.
 *
 * See and use %GWY_EXPR_ERROR.
 *
 * Returns: The error domain.
 **/
GQuark
gwy_expr_error_quark(void)
{
    static GQuark error_domain = 0;

    if (!error_domain)
        error_domain = g_quark_from_static_string("gwy-expr-error-quark");

    return error_domain;
}

/****************************************************************************
 *
 *  Execution
 *
 ****************************************************************************/

/**
 * interpret_stack:
 * @expr: An expression.
 *
 * Runs code in @expr->input, at the end, result is in @expr->stack[0].
 *
 * No checking is done, use stack_is_executable() beforehand.
 **/
static inline void
interpret_stack(Expr *expr)
{
    guint i;

    expr->sp = expr->stack - 1;
    for (i = 0; i < expr->in; i++) {
        const GwyExprCode *code = expr->input + i;
        const gint type = (gint)code->type;

        if (type == GWY_EXPR_CODE_CONSTANT)
            *(++expr->sp) = code->value;
        else if (type > 0) {
            call_table[type].function(&expr->sp);
        }
        else
            *(++expr->sp) = expr->variables[-type];
    }
}

/**
 * interpret_stack_vectors:
 * @expr: An expression.
 * @n: Lenght of @result and of @data member arrays, that is vector length.
 * @data: Array of arrays, each of length @n.  The arrays correspond to
 *        individual expression variables in gwy_expr_execute().  The zeroth
 *        array is not accessed and it can be %NULL.
 * @result: Array of length @n to store computation results to.  It may be
 *          one of those in @data.
 *
 * Performs actual vectorized stack interpretation.
 *
 * No checking is done, use stack_is_executable() beforehand.
 **/
static inline void
interpret_stack_vectors(Expr *expr,
                        guint n,
                        const gdouble **data,
                        gdouble *result)
{
    guint i, j;

    for (j = 0; j < n; j++) {
        expr->sp = expr->stack - 1;
        for (i = 0; i < expr->in; i++) {
            const GwyExprCode *code = expr->input + i;
            const gint type = (gint)code->type;

            if (type == GWY_EXPR_CODE_CONSTANT)
                *(++expr->sp) = code->value;
            else if (type > 0)
                call_table[type].function(&expr->sp);
            else
                *(++expr->sp) = data[-type][j];
        }
        result[j] = expr->stack[0];
    }
}

/**
 * stack_is_executable:
 * @expr: An expression.
 *
 * Checks whether a stack is executable and assures it's large enough.
 *
 * Returns: %TRUE if stack is executable, %FALSE if it isn't.
 **/
static gboolean
stack_is_executable(Expr *expr)
{
    guint i;
    gint nval, max;

    nval = max = 0;
    for (i = 0; i < expr->in; i++) {
        if (expr->input[i].type == GWY_EXPR_CODE_CONSTANT
            || (gint)expr->input[i].type < 0) {
            nval++;
        }
        else if (expr->input[i].type > 0) {
            nval -= call_table[expr->input[i].type].in_values;
            nval += call_table[expr->input[i].type].out_values;
            if (nval <= 0)
                return FALSE;
        }
        if (nval > max)
            max = nval;
    }
    if (nval != 1)
        return FALSE;

    if ((gint)expr->slen < max) {
        expr->slen = max;
        expr->stack = g_renew(gdouble, expr->stack, expr->slen);
    }

    return TRUE;
}

G_GNUC_UNUSED static void
print_stack(Expr *expr)
{
    guint i;
    GwyExprCode *code;

    g_printerr("expr->in = %d\n", expr->in);
    for (i = 0; i < expr->in; i++) {
        code = expr->input + i;
        if ((gint)code->type > 0) {
            g_print("#%u Function %s\n",
                    i, call_table[code->type].name);
        }
        else if ((gint)code->type < 0) {
            g_print("#%u Argument %s\n",
                    i, (gchar*)g_ptr_array_index(expr->identifiers,
                                                 -(gint)code->type));
        }
        else
            g_print("#%u Constant %g\n",
                    i, code->value);
    }
}

/**
 * fold_constants:
 * @expr: An expression.
 *
 * Folds foldable constants in compiled op code representation.
 *
 * This is done by selective execution, that is we go through the stack like
 * in interpret_stack(), but only execute function calls on constant
 * while copying operands and operations we cannot carry out right away.
 *
 * Only directly foldable constants can folded this way:
 *   1+2+3+4+x is folded to 10+x, but
 *   x+1+2+3+4 is kept intact.
 * Better than nothing.
 **/
static void
fold_constants(Expr *expr)
{
    guint from, to;
    gint last_constants = 0;

    for (from = to = 0; from < expr->in; from++, to++) {
        GwyExprCode *code = expr->input + from;

        expr->input[to] = expr->input[from];
        if (code->type == GWY_EXPR_CODE_CONSTANT)
            last_constants++;
        else if ((gint)code->type < 0)
            last_constants = 0;
        else {
            const GwyExprFunction *func = call_table + code->type;

            if (last_constants >= func->in_values) {
                gdouble tmp[GWY_EXPR_FUNC_MAX_ARGS];

                for (guint i = 0; i < (guint)func->in_values; i++)
                    tmp[i] = expr->input[to + i - func->in_values].value;

                gdouble *sp = tmp + func->in_values - 1;
                func->function(&sp);

                last_constants += func->out_values;
                last_constants -= func->in_values;
                to += func->out_values;
                to -= func->in_values;

                for (guint i = 0; i < (guint)func->out_values; i++) {
                    code = expr->input + to - func->out_values + i;
                    code->value = tmp[0];
                    code->type = GWY_EXPR_CODE_CONSTANT;
                }
                to--;
            }
            else
                last_constants = 0;
        }
    }
    expr->in = to;
}

/****************************************************************************
 *
 *  Reimplementation of interesting parts of GList
 *
 ****************************************************************************/

/**
 * token_list_last:
 * @tokens: A list of tokens.
 *
 * Returns last token in list.
 *
 * Returns: The last token, %NULL if list is empty.
 **/
static inline GwyExprToken*
token_list_last(GwyExprToken *tokens)
{
    if (G_UNLIKELY(!tokens))
        return NULL;
    while (tokens->next)
        tokens = tokens->next;

    return tokens;
}

/* XXX: used only once */
/**
 * token_list_length:
 * @tokens: A list of tokens.
 *
 * Counts tokens in a list.
 *
 * Returns: Token list length.
 **/
static inline guint
token_list_length(GwyExprToken *tokens)
{
    guint i = 0;

    while (tokens) {
        i++;
        tokens = tokens->next;
    }

    return i;
}

/**
 * token_list_prepend:
 * @tokens: A list of tokens.
 * @token: A token.
 *
 * Prepends a token before a list.
 *
 * Returns: The new list head, that is @token.
 **/
static inline GwyExprToken*
token_list_prepend(GwyExprToken *tokens,
                   GwyExprToken *token)
{
    token->next = tokens;
    if (tokens)
        tokens->prev = token;

    return token;
}

/* XXX: used only once */
/**
 * token_list_reverse:
 * @tokens: A list of tokens.
 *
 * Reverts the order in a token list.
 *
 * Returns: The new list head.
 **/
static inline GwyExprToken*
token_list_reverse(GwyExprToken *tokens)
{
    GwyExprToken *end;

    end = NULL;
    while (tokens) {
        end = tokens;
        tokens = end->next;
        end->next = end->prev;
        end->prev = tokens;
    }

    return end;
}

/**
 * token_list_concat:
 * @head: First token list.
 * @tail: Second token list.
 *
 * Concatenates two token lists.
 *
 * Returns: The new list head.
 **/
static inline GwyExprToken*
token_list_concat(GwyExprToken *head,
                  GwyExprToken *tail)
{
    GwyExprToken *end;

    if (!head)
        return tail;

    end = token_list_last(head);
    end->next = tail;
    if (tail)
        tail->prev = end;

    return head;
}

/**
 * token_list_delete_token:
 * @tokens: A list of tokens.
 * @token: A token.
 *
 * Unlinks a token from list and deletes it.
 *
 * Note if token holds some data (including RPN block), it must be freed first.
 *
 * Returns: The new list head.
 **/
static inline GwyExprToken*
token_list_delete_token(GwyExprToken *tokens,
                        GwyExprToken *token)
{
    if (token->prev)
        token->prev->next = token->next;
    else
        tokens = token->next;

    if (token->next)
        token->next->prev = token->prev;

    token->prev = NULL;
    g_slice_free(GwyExprToken, token);

    return tokens;
}

/**
 * token_list_insert:
 * @tokens: A list of tokens.
 * @before: A token to insert @token before.
 * @token: Token to insert.
 *
 * Inserts a token into a token list before given token.
 *
 * Returns: The new list head.
 **/
static inline GwyExprToken*
token_list_insert(GwyExprToken *tokens,
                  GwyExprToken *before,
                  GwyExprToken *token)
{
    if (!before->prev)
        return token_list_prepend(tokens, token);

    token->prev = before->prev;
    token->next = before;
    before->prev->next = token;
    before->prev = token;

    return tokens;
}

#define gwy_expr_token_new0() g_slice_new0(GwyExprToken)

/**
 * token_list_delete:
 * @tokens: A token list.
 *
 * Actually frees token list.
 *
 * Assumes each token is either unconverted scanner token, or it has non-empty
 * RPN block.  In either case it frees the RPN block, or strings of uncoverted
 * scanner tokens.
 **/
static void
token_list_delete(GwyExprToken *tokens)
{
    GwyExprToken *t;

    for (t = tokens; t; t = t->next) {
        if (t->rpn_block) {
            g_slice_free_chain(GwyExprToken, t->rpn_block, next);
        }
        else {
            switch (t->token) {
                case G_TOKEN_STRING:
                case G_TOKEN_IDENTIFIER:
                case G_TOKEN_IDENTIFIER_NULL:
                case G_TOKEN_COMMENT_SINGLE:
                case G_TOKEN_COMMENT_MULTI:
                g_free(t->value.v_string);
                break;

                default:
                break;
            }
        }
    }
    g_slice_free_chain(GwyExprToken, tokens, next);
}

/****************************************************************************
 *
 *  Infix -> RPN convertor, tokenizer
 *
 ****************************************************************************/

/**
 * scan_tokens:
 * @expr: An expression.
 * @err: Location to store scanning error to.
 *
 * Scans input to tokens, filling @expr->tokens.
 *
 * Returns: %TRUE on success, %FALSE if parsing failed.
 **/
static gboolean
scan_tokens(Expr *expr,
            GError **err)
{
    GScanner *scanner;
    GwyExprToken *tokens = NULL, *t;
    GTokenType token;

    if (expr->tokens) {
        g_warning("Token list residua from last run");
        token_list_delete(expr->tokens);
        expr->tokens = NULL;
    }

    scanner = expr->scanner;
    while ((token = g_scanner_get_next_token(scanner))) {
        switch ((gint)token) {
            case G_TOKEN_LEFT_PAREN:
            case G_TOKEN_RIGHT_PAREN:
            case G_TOKEN_COMMA:
            case '*':
            case '/':
            case '-':
            case '+':
            case '%':
            case '^':
            case '~':
            case G_TOKEN_FLOAT:
            case G_TOKEN_SYMBOL:
            t = gwy_expr_token_new0();
            t->token = token;
            t->value = expr->scanner->value;
            tokens = token_list_prepend(tokens, t);
            break;

            case G_TOKEN_IDENTIFIER:
            t = gwy_expr_token_new0();
            t->token = token;
            t->value.v_string = expr->scanner->value.v_string;
            tokens = token_list_prepend(tokens, t);
            /* Steal the string from GScanner */
            scanner->value.v_string = NULL;
            scanner->token = G_TOKEN_NONE;
            break;

            default:
            g_set_error(err, GWY_EXPR_ERROR, GWY_EXPR_ERROR_INVALID_TOKEN,
                        _("Invalid token 0x%02x encountered."), token);
            token_list_delete(tokens);
            return FALSE;
            break;
        }
    }

    if (!tokens) {
        g_set_error(err, GWY_EXPR_ERROR, GWY_EXPR_ERROR_EMPTY,
                   _("Expression is empty."));
        return FALSE;
    }

    expr->tokens = token_list_reverse(tokens);
    return TRUE;
}

/**
 * rectify_token_list:
 * @expr: An expression.
 *
 * Converts some human-style notations in @expr->tokens to stricter ones.
 *
 * Namely it: Removes unary +, changes unary - to ~ operator, adds
 * multiplication operators between adjacent values, converts ~ to operator.
 **/
static void
rectify_token_list(Expr *expr)
{
    GwyExprToken *t, *prev;

    for (t = expr->tokens; t; ) {
        prev = t->prev;

        switch (t->token) {
            /* convert unary - to negate operation */
            case '-':
            if (!prev || (prev->token != G_TOKEN_FLOAT
                          && prev->token != G_TOKEN_IDENTIFIER
                          && prev->token != G_TOKEN_RIGHT_PAREN)) {
                t->token = G_TOKEN_SYMBOL;
                t->value.v_symbol = GUINT_TO_POINTER(GWY_EXPR_CODE_NEGATE);
            }
            t = t->next;
            break;

            /* remove unary + */
            case '+':
            if (!prev || (prev->token != G_TOKEN_FLOAT
                          && prev->token != G_TOKEN_IDENTIFIER
                          && prev->token != G_TOKEN_RIGHT_PAREN)) {
                prev = t;
                t = t->next;
                expr->tokens = token_list_delete_token(expr->tokens, prev);
            }
            else
                t = t->next;
            break;

            /* add proper multiplications */
            case G_TOKEN_LEFT_PAREN:
            case G_TOKEN_FLOAT:
            case G_TOKEN_IDENTIFIER:
            case G_TOKEN_SYMBOL:
            if (prev && (prev->token == G_TOKEN_FLOAT
                         || prev->token == G_TOKEN_RIGHT_PAREN
                         || prev->token == G_TOKEN_IDENTIFIER)) {
                prev = gwy_expr_token_new0();
                prev->token = '*';
                expr->tokens = token_list_insert(expr->tokens, t, prev);
            }
            t = t->next;
            break;

            /* convert ~ to function */
            case '~':
            t->token = G_TOKEN_SYMBOL;
            t->value.v_symbol = GUINT_TO_POINTER(GWY_EXPR_CODE_NEGATE);
            break;

            default:
            t = t->next;
            break;
        }
    }
}

/**
 * initialize_scanner:
 * @expr: An expression evaluator.
 *
 * Initialises scanner, configuring it and setting up function symbol table.
 **/
static void
initialize_scanner(Expr *expr)
{
    guint i;

    if (expr->scanner)
        return;

    expr->scanner = g_scanner_new(&scanner_config);

    for (i = 1; i < G_N_ELEMENTS(call_table); i++) {
        if (!call_table[i].name || !g_ascii_isalpha(call_table[i].name[0]))
            continue;
        g_scanner_scope_add_symbol(expr->scanner, GWY_EXPR_SCOPE_GLOBAL,
                                   call_table[i].name, GUINT_TO_POINTER(i));
    }
    g_scanner_set_scope(expr->scanner, GWY_EXPR_SCOPE_GLOBAL);
    expr->scanner->input_name = "expression";
}

/**
 * parse_expr:
 * @expr: An expression.
 * @err: Location to store parsing error to
 *
 * Parses an expression to list of tokens, filling @expr->tokens.
 *
 * Returns: A newly allocated token list, %NULL on failure.
 **/
static gboolean
parse_expr(Expr *expr,
           GError **err)
{
    initialize_scanner(expr);
    g_scanner_input_text(expr->scanner, expr->expr->str, expr->expr->len);

    if (!scan_tokens(expr, err)) {
        g_assert(!err || *err);
        return FALSE;
    }
    rectify_token_list(expr);

    return TRUE;
}

/**
 * identifier_name_is_valid:
 * @name: Constant identifier.
 *
 * Checks whether constant name is a valid identifier.
 *
 * Valid identifier must start with a letter, continue with alphanumeric
 * characters (or underscores).
 *
 * Returns: %TRUE if @name is a possible constant name, %FALSE otherwise.
 **/
static gboolean
identifier_name_is_valid(const gchar *name)
{
    static const gunichar more[] = { '_', 0 };
    return gwy_utf8_strisident(name, more, NULL);
}

/**
 * transform_values:
 * @expr: An expression.
 * @err: Location to store error to, or %NULL.
 *
 * Converts constants to single-items RPN lists and indexes identifiers.
 *
 * %G_TOKEN_IDENTIFIER strings are freed, they are converted to single-item
 * RPN lists too, with type as minus index in returned array of name.
 *
 * Modifies @expr->tokens and fills @expr->identifiers.
 *
 * Returns: %TRUE on success, %FALSE if transformation failed.
 **/
static gboolean
transform_values(Expr *expr,
                 GError **err)
{
    GwyExprToken *code, *t;
    GQuark quark;
    gdouble *cval;
    guint i;

    if (!expr->identifiers) {
        expr->identifiers = g_ptr_array_new_with_free_func(g_free);
        /* pos 0 is always unused */
        g_ptr_array_add(expr->identifiers, NULL);
    }
    else
        g_ptr_array_set_size(expr->identifiers, 1);

    for (t = expr->tokens; t; t = t->next) {
        if (t->token == G_TOKEN_FLOAT) {
            code = gwy_expr_token_new0();
            code->token = GWY_EXPR_CODE_CONSTANT;
            code->value.v_float = t->value.v_float;
            t->rpn_block = token_list_prepend(t->rpn_block, code);
            continue;
        }
        else if (t->token != G_TOKEN_IDENTIFIER)
            continue;

        if (!identifier_name_is_valid(t->value.v_identifier)) {
            g_set_error(err, GWY_EXPR_ERROR, GWY_EXPR_ERROR_IDENTIFIER_NAME,
                        _("Invalid identifier name %s."),
                        t->value.v_identifier);
            token_list_delete(expr->tokens);
            expr->tokens = NULL;
            return FALSE;
        }

        if (expr->constants) {
            if ((quark = g_quark_try_string(t->value.v_identifier))
                && (cval = g_hash_table_lookup(expr->constants,
                                               GUINT_TO_POINTER(quark)))) {
                g_free(t->value.v_identifier);
                code = gwy_expr_token_new0();
                code->token = GWY_EXPR_CODE_CONSTANT;
                code->value.v_float = *cval;
                t->rpn_block = token_list_prepend(t->rpn_block, code);
                continue;
            }
        }
        for (i = 1; i < expr->identifiers->len; i++) {
            if (gwy_strequal(t->value.v_identifier,
                             g_ptr_array_index(expr->identifiers, i))) {
                g_free(t->value.v_identifier);
                break;
            }
        }
        if (i == expr->identifiers->len)
            g_ptr_array_add(expr->identifiers, t->value.v_identifier);
        code = gwy_expr_token_new0();
        code->token = -i;
        t->rpn_block = token_list_prepend(t->rpn_block, code);
    }

    return TRUE;
}

/**
 * transform_infix_ops:
 * @tokens: A token list.
 * @right_to_left: %TRUE to process operators right to left, %FALSE to
 *                 left to right.
 * @operators: String containing one-letter operators to convert.
 * @codes: Opcodes corresponding to @operators.
 * @err: Location to store conversion error to.
 *
 * Transforms a set of infix operators of equal priority to RPN blocks.
 *
 * Returns: Converted @tokens (it's changed in place), %NULL on failure.
 **/
static GwyExprToken*
transform_infix_ops(GwyExprToken *tokens,
                    gboolean right_to_left,
                    const gchar *operators,
                    const GwyExprOpCode *codes,
                    GError **err)
{
    GwyExprToken *prev, *next, *code, *t;
    guint i;

    for (t = right_to_left ? token_list_last(tokens) : tokens;
         t;
         t = right_to_left ? t->prev : t->next) {
        if (t->rpn_block)
            continue;
        for (i = 0; operators[i]; i++) {
            if (t->token == operators[i])
                break;
        }
        if (!operators[i])
            continue;

        /* Check arguments */
        prev = t->prev;
        next = t->next;
        if (!next || !prev) {
            g_set_error(err, GWY_EXPR_ERROR, GWY_EXPR_ERROR_MISSING_ARGUMENT,
                        _("Operator %c argument is missing"), operators[i]);
            token_list_delete(tokens);
            return NULL;
        }
        if (!prev->rpn_block || !next->rpn_block) {
            g_set_error(err, GWY_EXPR_ERROR, GWY_EXPR_ERROR_INVALID_ARGUMENT,
                        _("Operator %c argument is invalid"), operators[i]);
            token_list_delete(tokens);
            return NULL;
        }

        /* Convert */
        code = gwy_expr_token_new0();
        code->token = codes[i];
        prev->rpn_block = token_list_concat(prev->rpn_block, code);
        prev->rpn_block = token_list_concat(next->rpn_block,
                                                     prev->rpn_block);
        prev->token = G_TOKEN_NONE;
        t = prev;
        tokens = token_list_delete_token(tokens, t->next);
        tokens = token_list_delete_token(tokens, t->next);
    }

    return tokens;
}

/**
 * transform_functions:
 * @tokens: A token list.
 * @err: Location to store conversion error to.
 *
 * Transforms function calls and unary operators to RPN blocks.
 *
 * Returns: Converted @tokens (it's changed in place), %NULL on failure.
 **/
static GwyExprToken*
transform_functions(GwyExprToken *tokens,
                    GError **err)
{
    GwyExprToken *arg, *code, *t;
    guint func, nargs, i;

    for (t = token_list_last(tokens); t; t = t->prev) {
        if (t->token != G_TOKEN_SYMBOL)
            continue;

        func = GPOINTER_TO_UINT(t->value.v_symbol);
        nargs = call_table[func].in_values;
        /* Check arguments */
        for (i = 0, arg = t->next; i < nargs; i++, arg = arg->next) {
            if (!arg) {
                g_set_error(err, GWY_EXPR_ERROR,
                            GWY_EXPR_ERROR_MISSING_ARGUMENT,
                            _("Function %s expects %u arguments "
                              "but only %d were given."),
                            call_table[func].name, nargs, i);
                token_list_delete(tokens);
                return NULL;
            }
            if (!arg->rpn_block) {
                g_set_error(err, GWY_EXPR_ERROR,
                            GWY_EXPR_ERROR_INVALID_ARGUMENT,
                            _("Function %s argument is invalid"),
                            call_table[func].name);
                token_list_delete(tokens);
                return NULL;
            }
        }

        /* Convert */
        code = gwy_expr_token_new0();
        code->token = func;
        t->rpn_block = token_list_prepend(t->rpn_block, code);

        for (i = 0; i < nargs; i++) {
            arg = t->next;
            t->token = G_TOKEN_NONE;
            t->rpn_block = token_list_concat(arg->rpn_block,
                                                      t->rpn_block);
            tokens = token_list_delete_token(tokens, arg);
        }
    }

    return tokens;
}

/**
 * transform_to_rpn_real:
 * @expr: An expression.
 * @tokens: A parenthesized list of tokens.
 * @err: Location to store conversion error to
 *
 * Recursively converts list of tokens in human (infix) notation to RPN.
 *
 * Returns: Converted @tokens (it's changed in place), %NULL on failure.
 **/
static GwyExprToken*
transform_to_rpn_real(Expr *expr,
                      GwyExprToken *tokens,
                      GError **err)
{
    GwyExprOpCode pow_operators[] = {
        GWY_EXPR_CODE_POWER,
    };
    GwyExprOpCode mult_operators[] = {
        GWY_EXPR_CODE_MULTIPLY, GWY_EXPR_CODE_DIVIDE, GWY_EXPR_CODE_MODULO,
    };
    GwyExprOpCode add_operators[] = {
        GWY_EXPR_CODE_ADD, GWY_EXPR_CODE_SUBTRACT,
    };
    GwyExprToken *t, *subblock, *remainder_ = NULL;
    static guint level = 0;

    if (!tokens) {
        g_warning("Empty token list");
        return NULL;
    }
    level++;

    if (tokens->token != G_TOKEN_LEFT_PAREN) {
        g_set_error(err, GWY_EXPR_ERROR, GWY_EXPR_ERROR_OPENING_PARENTHESIS,
                    _("Opening parenthesis is missing."));
        goto FAIL;
    }
    tokens = token_list_delete_token(tokens, tokens);

    /* isolate the list (parenthesization level) we are responsible for */
    for (t = tokens; t; t = t->next) {
        /* split rest of list to remainder */
        if (t->token == G_TOKEN_RIGHT_PAREN) {
            if (t->next) {
                remainder_ = t->next;
                remainder_->prev = NULL;
                t->next = NULL;
            }
            tokens = token_list_delete_token(tokens, t);
            break;
        }
        else if (t->token == G_TOKEN_LEFT_PAREN) {
            subblock = t;
            if (t->prev) {
                t = t->prev;
                t->next = NULL;
            }
            else
                t = tokens = NULL;
            subblock->prev = NULL;
            subblock = transform_to_rpn_real(expr, subblock, err);
            if (!subblock) {
                g_assert(!err || *err);
                goto FAIL;
            }
            if (t) {
                t->next = subblock;
                subblock->prev = t;
            }
            else
                t = tokens = subblock;
        }
    }
    /* missing right parenthesis or empty parentheses */
    if (!t) {
        g_set_error(err, GWY_EXPR_ERROR, GWY_EXPR_ERROR_CLOSING_PARENTHESIS,
                    _("Closing parenthesis is missing."));
        goto FAIL;
    }
    if (!tokens) {
        g_set_error(err, GWY_EXPR_ERROR, GWY_EXPR_ERROR_EMPTY_PARENTHESES,
                    _("Empty parentheses."));
        goto FAIL;
    }

    /* 0. Remove commas */
    for (t = tokens; t; t = t->next) {
        if (t->token == G_TOKEN_COMMA) {
            if (!t->next || !t->prev) {
                g_set_error(err, GWY_EXPR_ERROR, GWY_EXPR_ERROR_STRAY_COMMA,
                            _("Unexpected comma."));
                goto FAIL;
            }
            t = t->prev;
            tokens = token_list_delete_token(tokens, t->next);
        }
    }

    /* 1. Functions */
    if (!(tokens = transform_functions(tokens, err))) {
        g_assert(!err || *err);
        goto FAIL;
    }

    /* 2. Power operator */
    if (!(tokens = transform_infix_ops(tokens, TRUE, "^",
                                                pow_operators, err))) {
        g_assert(!err || *err);
        goto FAIL;
    }

    /* 3. Multiplicative operators */
    if (!(tokens = transform_infix_ops(tokens, FALSE, "*/%",
                                                mult_operators, err))) {
        g_assert(!err || *err);
        goto FAIL;
    }

    /* 4. Additive operators */
    if (!(tokens = transform_infix_ops(tokens, FALSE, "+-",
                                                add_operators, err))) {
        g_assert(!err || *err);
        goto FAIL;
    }

    /* Check */
    for (t = tokens; t; t = t->next) {
        if (!t->rpn_block) {
            g_set_error(err, GWY_EXPR_ERROR, GWY_EXPR_ERROR_GARBAGE,
                        _("Unexpected symbol %d."), t->token);
            goto FAIL;
        }
    }

    tokens = token_list_concat(tokens, remainder_);
    level--;
    return tokens;

FAIL:
    token_list_delete(tokens);
    token_list_delete(remainder_);
    level--;
    return NULL;
}

/**
 * transform_to_rpn:
 * @expr: An expression.
 * @err: Location to store conversion error to.
 *
 * Converts list of tokens from parser to RPN stack.
 *
 * @expr->tokens is destroyed by the conversion and set to %NULL,
 * @expr->input is filled with opcodes, stack is checked for executability
 * and eventually resized.
 *
 * Returns: A newly created RPN stack, %NULL on failure.
 **/
static gboolean
transform_to_rpn(Expr *expr,
                 GError **err)
{
    GwyExprToken *t;
    guint i;

    /* parenthesize token list */
    t = gwy_expr_token_new0();
    t->token = G_TOKEN_RIGHT_PAREN;
    expr->tokens = token_list_concat(expr->tokens, t);
    t = gwy_expr_token_new0();
    t->token = G_TOKEN_LEFT_PAREN;
    expr->tokens = token_list_prepend(expr->tokens, t);

    expr->tokens = transform_to_rpn_real(expr, expr->tokens, err);
    if (!expr->tokens) {
        g_assert(!err || *err);
        return FALSE;
    }

    if (expr->tokens->next) {
        g_set_error(err, GWY_EXPR_ERROR, GWY_EXPR_ERROR_GARBAGE,
                    _("Expression contains trailing garbage."));
        token_list_delete(expr->tokens);
        expr->tokens = NULL;
        return FALSE;
    }

    expr->in = token_list_length(expr->tokens->rpn_block);
    if (expr->in > expr->ilen) {
        expr->ilen = expr->in;
        expr->input = g_renew(GwyExprCode, expr->input, expr->ilen);
    }
    for (t = expr->tokens->rpn_block, i = 0; t; t = t->next, i++) {
        expr->input[i].type = t->token;
        expr->input[i].value = t->value.v_float;
    }
    token_list_delete(expr->tokens);
    expr->tokens = NULL;

    if (!stack_is_executable(expr)) {
        g_set_error(err, GWY_EXPR_ERROR, GWY_EXPR_ERROR_NOT_EXECUTABLE,
                   _("Expression is not executable."));
        return FALSE;
    }

    return TRUE;
}

static void
free_double(void *pointer)
{
    g_slice_free(gdouble, pointer);
}

static void
ensure_constants(Expr *expr)
{
    if (expr->constants)
        return;

    /* We could also put constants into scanner's symbol table, but then
     * we have to tell them apart when we get some symbol from scanner. */
    expr->constants = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                            NULL, free_double);
}

/****************************************************************************
 *
 *  High level, public API
 *
 ****************************************************************************/

/**
 * gwy_expr_new:
 *
 * Creates a new expression evaluator.
 *
 * Returns: A new expression evaluator.
 **/
GwyExpr*
gwy_expr_new(void)
{
    return g_object_newv(GWY_TYPE_EXPR, 0, NULL);
}

/**
 * gwy_expr_get_expression:
 * @expr: An expression evaluator.
 *
 * Gets the expression string.
 *
 * Returns: The last string passed to gwy_expr_evaluate() or
 *          gwy_expr_compile().  It is owned by @expr and must not be modified
 *          or freed.
 **/
const gchar*
gwy_expr_get_expression(GwyExpr *expr)
{
    g_return_val_if_fail(GWY_IS_EXPR(expr), NULL);
    return expr->priv->expr->str;
}

/**
 * gwy_expr_evaluate:
 * @expr: An expression evaluator.
 * @text: String containing the expression to evaluate.
 * @result: Location to store result to.
 * @err: Location to store compilation or evaluation error to, or %NULL.
 *       Errors from #GwyExprError domain can occur.
 *
 * Evaulates an arithmetic expression.
 *
 * Returns: %TRUE on success, %FALSE if evaluation failed.
 **/
gboolean
gwy_expr_evaluate(GwyExpr *expr,
                  const gchar *text,
                  gdouble *result,
                  GError **err)
{
    g_return_val_if_fail(GWY_IS_EXPR(expr), FALSE);
    if (!gwy_expr_compile(expr, text, err))
        return FALSE;

    Expr *priv = expr->priv;
    if (priv->identifiers->len > 1) {
        g_set_error(err, GWY_EXPR_ERROR, GWY_EXPR_ERROR_UNRESOLVED_IDENTIFIERS,
                    _("Not all identifiers were resolved."));
        return FALSE;
    }

    interpret_stack(priv);
    *result = *priv->stack;

    return TRUE;
}

/**
 * gwy_expr_compile:
 * @expr: An expression evaluator.
 * @text: String containing the expression to compile.
 * @err: Location to store compilation or evaluation error to, or %NULL.
 *       Errors from #GwyExprError domain can occur.
 *
 * Compiles an expression for later execution.
 *
 * This function is useful for expressions with variables.  For normal
 * arithmetic expressions it's easier to use gwy_expr_evaluate().
 *
 * Returns: %TRUE on success, %FALSE if compilation failed.
 **/
gboolean
gwy_expr_compile(GwyExpr *expr,
                 const gchar *text,
                 GError **err)
{
    g_return_val_if_fail(GWY_IS_EXPR(expr), FALSE);
    g_return_val_if_fail(text, FALSE);
    Expr *priv = expr->priv;

    if (!g_utf8_validate(text, -1, NULL)) {
        g_set_error(err, GWY_EXPR_ERROR, GWY_EXPR_ERROR_UTF8,
                    _("Expression is not valid UTF-8."));
        priv->in = 0;
        return FALSE;
    }

    g_string_assign(priv->expr, text);
    if (!parse_expr(priv, err)
        || !transform_values(priv, err)
        || !transform_to_rpn(priv, err)) {
        g_assert(!err || *err);
        priv->in = 0;
        return FALSE;
    }
    fold_constants(priv);

    // Ensure NULL-terminated identifiers[]
    g_ptr_array_add(priv->identifiers, NULL);
    g_ptr_array_set_size(priv->identifiers, priv->identifiers->len-1);

    return TRUE;
}

/**
 * gwy_expr_get_variables:
 * @expr: An expression evaluator.
 * @names: Location to store pointer to array of variable names to (may be
 *         %NULL to get just number of variables).  The string array returned
 *         in this argument in owned by @expr and is valid only until next
 *         gwy_expr_compile() or gwy_expr_evaluate() call or until you release
 *         your reference to @expr.  The array is %NULL-terminated.
 *
 * Get the number, names, and indices of unresolved identifiers in @expr.
 *
 * It is an error to call this function after an unsuccessful compilation.
 *
 * If you only care about variables from a prefedined set, that is if any
 * unknown variable is an error, it's easier to use
 * gwy_expr_resolve_variables().
 *
 * The position of each variable in @names corresponds to the position of its
 * value in @values array in gwy_expr_execute() call.  Namely, the first item
 * in the array is always reserved and do not correspond to any variable.
 *
 * Returns: The length of array stored to @names.  This is the number of
 *          variables plus one (for the first reserved item).
 *          On failure, 0 is returned.
 **/
guint
gwy_expr_get_variables(GwyExpr *expr,
                       const gchar ***names)
{
    g_return_val_if_fail(GWY_IS_EXPR(expr), 0);
    g_return_val_if_fail(expr->priv->in, 0);

    if (names)
        *names = (const gchar**)expr->priv->identifiers->pdata;

    return expr->priv->identifiers->len;
}

/**
 * gwy_expr_resolve_variables:
 * @expr: An expression evaluator.
 * @n: The length of @names and @indices arrays.
 * @names: List of variable names to get positions of.
 * @indices: Array to store variable positions to.  The positions are the same
 *           as in gwy_expr_execute().  Variables not present in the expression
 *           are assigned (reserved) position 0.  This allows to safely
 *           substitute values of all variables before execution wthout caring
 *           which variables are actually present.
 *
 * Finds positions of variables in an expression.
 *
 * Returns: The number of remaining, unresolved variables in @expr.
 **/
guint
gwy_expr_resolve_variables(GwyExpr *expr,
                           guint n,
                           const gchar* const *names,
                           guint *indices)
{
    g_return_val_if_fail(GWY_IS_EXPR(expr), G_MAXUINT);
    g_return_val_if_fail(!n || (names && indices), G_MAXUINT);
    Expr *priv = expr->priv;
    g_return_val_if_fail(priv->in, G_MAXUINT);

    GPtrArray *identifiers = priv->identifiers;
    gboolean requested[identifiers->len];
    gwy_clear(requested, identifiers->len);
    gwy_clear(indices, n);
    guint i;

    for (i = 0; i < n; i++) {
        for (guint j = 1; j < identifiers->len; j++) {
            if (gwy_strequal(names[i],
                             (gchar*)g_ptr_array_index(identifiers, j))) {
                indices[i] = j;
                requested[j] = TRUE;
                break;
            }
        }
    }

    i = 0;
    for (guint j = 1; j < identifiers->len; j++)
        i += !requested[j];

    return i;
}

/**
 * gwy_expr_execute:
 * @expr: An expression evaluator.
 * @values: Array with variable values.  Its zeroth item is always unused.
 *          Variable list can be obtained by gwy_expr_get_variables().
 *
 * Executes a compiled expression with variables, substituting given values.
 *
 * Returns: The result.
 **/
gdouble
gwy_expr_execute(GwyExpr *expr,
                 const gdouble *values)
{
    g_return_val_if_fail(GWY_IS_EXPR(expr), NAN);
    Expr *priv = expr->priv;
    g_return_val_if_fail(priv->in, NAN);
    g_return_val_if_fail(values || priv->identifiers->len <= 1, NAN);

    priv->variables = values;
    interpret_stack(priv);
    return priv->stack[0];
}

/**
 * gwy_expr_vector_execute:
 * @expr: An expression evaluator.
 * @n: The lenght of @result and of @data member arrays, that is vector length.
 * @data: An array of arrays of length @n.  The arrays correspond to expression
 *        variables in gwy_expr_execute().  Zeroth array can be %NULL.
 * @result: An array of length @n to store computation results to.  It may be
 *          one of those in @data.
 *
 * Executes a compiled expression on each item of data arrays.
 **/
void
gwy_expr_vector_execute(GwyExpr *expr,
                        guint n,
                        const gdouble **data,
                        gdouble *result)
{
    g_return_if_fail(GWY_IS_EXPR(expr));
    g_return_if_fail(result);
    Expr *priv = expr->priv;
    g_return_if_fail(priv->in);
    g_return_if_fail(data || priv->identifiers->len <= 1);

    interpret_stack_vectors(priv, n, data, result);
}

/**
 * gwy_expr_define_constant:
 * @expr: An expression evaluator.
 * @name: Name of constant to define.
 * @value: Constant numeric value.
 * @err: Location to store error to, or %NULL.
 *       Only %GWY_EXPR_ERROR_IDENTIFIER_NAME error from #GwyExprError domain
 *       can occur.
 *
 * Defines a symbolic constant.
 *
 * Note the definition does not affect already compiled expression, you have
 * to recompile it (and eventually re-resolve variables).
 *
 * Returns: %TRUE on success, %FALSE if definition failed.
 **/
gboolean
gwy_expr_define_constant(GwyExpr *expr,
                         const gchar *name,
                         gdouble value,
                         GError **err)
{
    g_return_val_if_fail(GWY_IS_EXPR(expr), FALSE);
    g_return_val_if_fail(name, FALSE);

    if (!identifier_name_is_valid(name)) {
        g_set_error(err, GWY_EXPR_ERROR, GWY_EXPR_ERROR_IDENTIFIER_NAME,
                    _("Identifier name is invalid"));
        return FALSE;
    }

    Expr *priv = expr->priv;
    initialize_scanner(priv);
    if (g_scanner_lookup_symbol(priv->scanner, name)) {
        g_set_error(err, GWY_EXPR_ERROR, GWY_EXPR_ERROR_NAME_CLASH,
                    _("Constant name clashes with function"));
        return FALSE;
    }

    GQuark quark = g_quark_from_string(name);
    ensure_constants(priv);
    g_hash_table_insert(priv->constants, GUINT_TO_POINTER(quark),
                        g_slice_dup(gdouble, &value));

    return TRUE;
}

/**
 * gwy_expr_undefine_constant:
 * @expr: An expression evaluator.
 * @name: Name of constant to undefine.
 *
 * Undefines a symbolic constant.
 *
 * Note the definition removal does not affect already compiled expression,
 * you have to recompile it (and eventually re-resolve variables).
 *
 * Returns: %TRUE if there was such a constant and was removed, %FALSE
 *          otherwise.
 **/
gboolean
gwy_expr_undefine_constant(GwyExpr *expr,
                           const gchar *name)
{
    g_return_val_if_fail(GWY_IS_EXPR(expr), FALSE);
    g_return_val_if_fail(name, FALSE);

    Expr *priv = expr->priv;
    if (!priv->constants)
        return FALSE;

    GQuark quark = g_quark_from_string(name);
    return g_hash_table_remove(priv->constants, GUINT_TO_POINTER(quark));

}


/**
 * SECTION: expr
 * @title: GwyExpr
 * @short_description: Arithmetic expression parser and evaluator
 *
 * #GwyExpr is an expression evaluator, more precisely parser, compiler, and
 * evaluator. A new #GwyExpr can be created with gwy_expr_new() and can be
 * used to evaluate any number of expressions then.
 *
 * Expression syntax is described in
 * <ulink url="http://gwyddion.net/documentation/user-guide-en/expression-syntax.html">Gwyddion user guide</ulink>.
 *
 * Simple arithmetic expressions without variables can be directly evaluated
 * with gwy_expr_evaluate().
 *
 * Expression with variables have to be compiled first with gwy_expr_compile().
 * Then either gwy_expr_resolve_variables() or gwy_expr_get_variables() can be
 * used to obtain information what variables are present in the expression and
 * at which positions (variables are referenced by position, not name during
 * final evaluation because of effieiency reasons). Subsequent evaluation with
 * variable substitution is performed by gwy_expr_execute(). It is also
 * possible to evaluate expression on each item of value arrays with
 * gwy_expr_vector_execute().
 *
 * One-shot evaluation of expressions without variables is easy:
 * <informalexample><programlisting>
 * GwyExpr *expr;
 * GError *err = NULL;
 * gdouble result;
 *
 * expr = gwy_expr_new();
 * if (!gwy_expr_evaluate(expr, "1+2", &result, &err)) {
 *     /&ast; Handle compilation error &ast;/
 * }
 * g_print("The result: %g\n", result);
 * g_object_unref(expr);
 * </programlisting></informalexample>
 *
 * One-shot evaluation of expressions with known variables can be performed by
 * defining them as constants beforehand:
 * <informalexample><programlisting>
 * GwyExpr *expr;
 * GError *err = NULL;
 * gdouble result;
 *
 * /&ast; Create expression and define variables as constants &ast;/
 * expr = gwy_expr_new();
 * gwy_expr_define_constant(expr, "x", 3.0, NULL);
 * gwy_expr_define_constant(expr, "y", 4.0, NULL);
 *
 * /&ast; Evaluate expression &ast;/
 * if (!gwy_expr_evaluate(expr, "hypot x,y", &result, &err)) {
 *     /&ast; Handle compilation error &ast;/
 * }
 * g_print("The result: %g\n", result);
 *
 * g_object_unref(expr);
 * </programlisting></informalexample>
 *
 * When the same expression is evaluated multiple times with different
 * variable values and the variables are again from some known set, the
 * gwy_expr_resolve_variables() should be used (if the repeated evaluation
 * happens over items of some arrays, gwy_expr_vector_execute() is more
 * efficient and usually simplier to use too):
 * <informalexample><programlisting>
 * GwyExpr *expr;
 * GError *err = NULL;
 * const gchar *const var_names[] = { "lambda", "theta", "psi", "d", "k" };
 * const gdouble var_values1[] = { 300.0, 0.09, 2.0, 52.3, 1.43 };
 * const gdouble var_values2[] = { 400.0, 0.12, 2.0, 54.1, 1.44 };
 * guint var_positions[G_N_ELEMENTS(var_names)];
 * gdouble vars[G_N_ELEMENTS(var_names) + 1];
 * guint i;
 *
 * /&ast; Create expression and define constant pi &ast;/
 * expr = gwy_expr_new();
 * gwy_expr_define_constant(expr, "pi", G_PI, NULL);
 *
 * /&ast; Compile expression &ast;/
 * if (!gwy_expr_compile(expr, "2*pi/lambda*d*sin theta", &err)) {
 *     /&ast; Handle compilation error &ast;/
 * }
 *
 * /&ast; Resolve variables &ast;/
 * if (gwy_expr_resolve_variables(expr, G_N_ELEMENTS(var_names),
 *                                var_names, var_positions)) {
 *     /&ast; Expression contains unknown variables &ast;/
 * }
 *
 * /&ast; Evaluate first set &ast;/
 * for (i = 0; i < G_N_ELEMENTS(var_names); i++) {
 *     vars[var_positions[i]] = var_values1[i];
 * }
 * g_print("First result: %g\n", gwy_expr_execute(expr, vars));
 *
 * /&ast; Evaluate second set &ast;/
 * for (i = 0; i < G_N_ELEMENTS(var_names); i++)
 *     vars[var_positions[i]] = var_values2[i];
 * g_print("Second result: %g\n", gwy_expr_execute(expr, vars));
 *
 * g_object_unref(expr);
 * </programlisting></informalexample>
 *
 * The most general case is when the variables are from a large set (or
 * completely arbitrary).  Then it is best to get the list of variables with
 * gwy_expr_get_variables() and supply only values of variables that
 * are actually present in the expression.
 **/

/**
 * GwyExprError:
 * @GWY_EXPR_ERROR_CLOSING_PARENTHESIS: A closing parenthesis is missing.
 * @GWY_EXPR_ERROR_EMPTY: Expression is empty.
 * @GWY_EXPR_ERROR_EMPTY_PARENTHESES: A parentheses pair contain nothing
 *                                    inside.
 * @GWY_EXPR_ERROR_GARBAGE: An symbol unexpectedly managed to survive.
 * @GWY_EXPR_ERROR_INVALID_ARGUMENT: Function or operator argument is not a
 *                                   value.
 * @GWY_EXPR_ERROR_INVALID_TOKEN: Expression contains an invalid token.
 * @GWY_EXPR_ERROR_MISSING_ARGUMENT: Function or operator arguments is missing.
 * @GWY_EXPR_ERROR_NOT_EXECUTABLE: Compiled stack is not executable.
 * @GWY_EXPR_ERROR_OPENING_PARENTHESIS: An opening parenthesis is missing.
 * @GWY_EXPR_ERROR_STRAY_COMMA: A comma at the start or end of list.
 * @GWY_EXPR_ERROR_UNRESOLVED_IDENTIFIERS: Expression contains unresolved
 *                                         identifiers.
 * @GWY_EXPR_ERROR_IDENTIFIER_NAME: Identifier name is invalid.
 * @GWY_EXPR_ERROR_NAME_CLASH: Name clashes with built-in symbol.
 * @GWY_EXPR_ERROR_UTF8: Expression is not valid UTF-8.
 *
 * Error codes returned by expression parsing and execution.
 **/

/**
 * GWY_EXPR_ERROR:
 *
 * Error domain for expression parsing and evaluation. Errors in this domain
 * will be from the #GwyExprError enumeration. See #GError for information on
 * error domains.
 **/

/**
 * GwyExpr:
 *
 * Object representing expression evaluators.
 *
 * The #GwyExpr struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyExprClass:
 *
 * Class of item inventories.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
