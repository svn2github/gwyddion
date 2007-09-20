/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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

#include "config.h"
#include <stdarg.h>
#include <libgwyddion/gwymacros.h>
#include <app/validate.h>
#include "gwyappinternal.h"

#define FAIL gwy_data_validation_failure_new

static GwyDataValidationFailure*
gwy_data_validation_failure_new(GwyDataError type,
                                GQuark key,
                                const gchar *fmt,
                                ...)
{
    GwyDataValidationFailure *failure;

    failure = g_new0(GwyDataValidationFailure, 1);
    failure->error = type;
    failure->key = key;

    if (fmt) {
        va_list ap;

        va_start(ap, fmt);
        failure->details = g_strdup_vprintf(fmt, ap);
        va_end(ap);
    }

    return failure;
}

static void
gwy_data_validation_failure_free(GwyDataValidationFailure *failure)
{
    g_free(failure->details);
    g_free(failure);
}

void
gwy_data_validation_failure_list_free(GSList *list)
{
    GSList *l;

    for (l = list; l; l = g_slist_next(l))
        gwy_data_validation_failure_free(l->data);
    g_slist_free(list);
}

static gboolean
check_utf8(const gchar *string,
           GQuark key,
           GSList **errors)
{
    const gchar *end;

    if (G_LIKELY(g_utf8_validate(string, -1, &end)))
        return TRUE;

    *errors = g_slist_prepend(*errors,
                              FAIL(GWY_DATA_ERROR_NON_UTF8_STRING, key,
                                   _("byte at position %d"),
                                   end - string));
    return FALSE;
}

static gboolean
check_ascii_key(const guchar *strkey,
                GQuark key,
                GSList **errors)
{
    const guchar *end;

    for (end = strkey; *end; end++) {
        if (G_UNLIKELY(*end >= 127U || !g_ascii_isprint(*end)))
            goto fail;
    }
    return TRUE;

fail:
    *errors = g_slist_prepend(*errors,
                              FAIL(GWY_DATA_ERROR_KEY_CHARACTERS, key,
                                   _("byte at position %d"),
                                   end - strkey));
    return FALSE;
}

static gboolean
check_type(GValue *gvalue,
           GType type,
           GQuark key,
           GSList **errors)
{
    if (G_LIKELY(G_VALUE_HOLDS(gvalue, type))) {
        if (type == G_TYPE_STRING)
            return check_utf8(g_value_get_string(gvalue), key, errors);
        return TRUE;
    }

    *errors = g_slist_prepend(*errors,
                              FAIL(GWY_DATA_ERROR_UNEXPECTED_TYPE, key,
                                   _("%s instead of %s"),
                                   g_type_name(type),
                                   g_type_name(G_VALUE_TYPE(gvalue))));
    return FALSE;
}

static void
validate_item(gpointer hash_key,
              gpointer hash_value,
              gpointer user_data)
{
    GQuark key = GPOINTER_TO_UINT(hash_key);
    GValue *gvalue = (GValue*)hash_value;
    GSList **errors = (GSList**)user_data;
    const gchar *strkey;
    gint id;
    guint len;
    GwyAppKeyType type;

    strkey = g_quark_to_string(key);
    check_ascii_key(strkey, key, errors);
    if (strkey[0] != GWY_CONTAINER_PATHSEP)
        *errors = g_slist_prepend(*errors,
                                  FAIL(GWY_DATA_ERROR_KEY_FORMAT, key, NULL));

    id = _gwy_app_analyse_data_key(strkey, &type, &len);

    /* Non-id items */
    if (type == KEY_IS_FILENAME) {
        check_type(gvalue, G_TYPE_STRING, key, errors);
        return;
    }

    /* Items that must have data id.  While ids above 2^20 are technically
     * valid, they indicate a bug or malice. */
    if (id < 0) {
        *errors = g_slist_prepend(*errors,
                                  FAIL(GWY_DATA_ERROR_KEY_UNKNOWN, key, NULL));
        return;
    }
    else if (id < 0 || id >= (1 << 20))
        *errors = g_slist_prepend(*errors,
                                  FAIL(GWY_DATA_ERROR_KEY_ID, key, NULL));

    /* Types */
    switch (type) {
        case KEY_IS_DATA:
        case KEY_IS_MASK:
        case KEY_IS_SHOW:
        check_type(gvalue, GWY_TYPE_DATA_FIELD, key, errors);
        break;

        case KEY_IS_GRAPH:
        check_type(gvalue, GWY_TYPE_GRAPH_MODEL, key, errors);
        break;

        case KEY_IS_SPECTRA:
        check_type(gvalue, GWY_TYPE_SPECTRA, key, errors);
        break;

        case KEY_IS_META:
        check_type(gvalue, GWY_TYPE_CONTAINER, key, errors);
        break;

        case KEY_IS_TITLE:
        case KEY_IS_PALETTE:
        case KEY_IS_3D_PALETTE:
        case KEY_IS_3D_MATERIAL:
        check_type(gvalue, G_TYPE_STRING, key, errors);
        break;

        case KEY_IS_SELECT:
        check_type(gvalue, GWY_TYPE_SELECTION, key, errors);
        break;

        case KEY_IS_RANGE_TYPE:
        case KEY_IS_SPS_REF:
        check_type(gvalue, G_TYPE_INT, key, errors);
        break;

        case KEY_IS_RANGE:
        case KEY_IS_MASK_COLOR:
        check_type(gvalue, G_TYPE_DOUBLE, key, errors);
        break;

        case KEY_IS_REAL_SQUARE:
        check_type(gvalue, G_TYPE_BOOLEAN, key, errors);
        break;

        case KEY_IS_3D_SETUP:
        check_type(gvalue, GWY_TYPE_3D_SETUP, key, errors);
        break;

        case KEY_IS_3D_LABEL:
        check_type(gvalue, GWY_TYPE_3D_LABEL, key, errors);
        break;

        default:
        g_warning("Key type %u of %s not handled in validate_item()",
                  type, strkey);
        break;
    }
}

GSList*
gwy_data_validate(GwyContainer *data,
                  GwyDataValidateFlags flags)
{
    GSList *errors = NULL;

    gwy_container_foreach(data, NULL, &validate_item, &errors);
    errors = g_slist_reverse(errors);

    return errors;
}

const gchar*
gwy_data_error_desrcibe(GwyDataError error)
{
    static const gchar *errors[] = {
        "",
        N_("Invalid item key format"),
        N_("Item key contains invalid characters"),
        N_("Item key does not belong to any known data"),
        N_("Wrong data item id"),
        N_("Unexpected data item type"),
        N_("String value is not valid UTF-8"),
    };

    if (error < 1 || error >= G_N_ELEMENTS(errors))
        return "";

    return errors[error];
}

/************************** Documentation ****************************/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
