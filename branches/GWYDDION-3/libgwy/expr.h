/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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

#ifndef __LIBGWY_EXPR_H__
#define __LIBGWY_EXPR_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GWY_EXPR_ERROR gwy_expr_error_quark()

typedef enum {
    GWY_EXPR_ERROR_UTF8 = 1,
    GWY_EXPR_ERROR_CLOSING_PARENTHESIS,
    GWY_EXPR_ERROR_EMPTY,
    GWY_EXPR_ERROR_EMPTY_PARENTHESES,
    GWY_EXPR_ERROR_GARBAGE,
    GWY_EXPR_ERROR_INVALID_ARGUMENT,
    GWY_EXPR_ERROR_INVALID_TOKEN,
    GWY_EXPR_ERROR_MISSING_ARGUMENT,
    GWY_EXPR_ERROR_NOT_EXECUTABLE,
    GWY_EXPR_ERROR_OPENING_PARENTHESIS,
    GWY_EXPR_ERROR_STRAY_COMMA,
    GWY_EXPR_ERROR_UNRESOLVED_IDENTIFIERS,
    GWY_EXPR_ERROR_IDENTIFIER_NAME,
    GWY_EXPR_ERROR_NAME_CLASH,
} GwyExprError;

#define GWY_TYPE_EXPR \
    (gwy_expr_get_type())
#define GWY_EXPR(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_EXPR, GwyExpr))
#define GWY_EXPR_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_EXPR, GwyExprClass))
#define GWY_IS_EXPR(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_EXPR))
#define GWY_IS_EXPR_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_EXPR))
#define GWY_EXPR_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_EXPR, GwyExprClass))

typedef struct _GwyExpr      GwyExpr;
typedef struct _GwyExprClass GwyExprClass;

struct _GwyExpr {
    GObject g_object;
    struct _GwyExprPrivate *priv;
};

struct _GwyExprClass {
    /*<private>*/
    GObjectClass g_object_class;
};

GQuark       gwy_expr_error_quark      (void)                       G_GNUC_CONST;
GType        gwy_expr_get_type         (void)                       G_GNUC_CONST;
GwyExpr*     gwy_expr_new              (void)                       G_GNUC_MALLOC;
gboolean     gwy_expr_evaluate         (GwyExpr *expr,
                                        const gchar *text,
                                        gdouble *result,
                                        GError **err);
gboolean     gwy_expr_compile          (GwyExpr *expr,
                                        const gchar *text,
                                        GError **err);
guint        gwy_expr_resolve_variables(GwyExpr *expr,
                                        guint n,
                                        const gchar* const *names,
                                        guint *indices);
guint        gwy_expr_get_variables    (GwyExpr *expr,
                                        const gchar ***names);
gdouble      gwy_expr_execute          (GwyExpr *expr,
                                        const gdouble *values);
void         gwy_expr_vector_execute   (GwyExpr *expr,
                                        guint n,
                                        const gdouble **data,
                                        gdouble *result);
gboolean     gwy_expr_define_constant  (GwyExpr *expr,
                                        const gchar *name,
                                        gdouble value,
                                        GError **err);
gboolean     gwy_expr_undefine_constant(GwyExpr *expr,
                                        const gchar *name);
const gchar* gwy_expr_get_expression   (GwyExpr *expr)              G_GNUC_PURE;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
