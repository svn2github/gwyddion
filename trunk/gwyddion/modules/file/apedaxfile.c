/*
 *  $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  APDT and DAX file format Importer Module
 *  Copyright (C) 2015 A.P.E. Research srl
 *  E-mail: infos@aperesearch.com
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-ape-dax-spm">
 *   <comment>A.P.E. Research DAX SPM data</comment>
 *   <glob pattern="*.dax"/>
 *   <glob pattern="*.DAX"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * A.P.E. Research DAX
 * .dax
 * Read
 **/

/*Includes*/

#include "config.h"
#include <libgwyddion/gwyddion.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <unzip.h>

#include "err.h"

/*Macros*/
#define FILE_TYPE "DAX"
#define EXTENSION ".dax"
#define APDT_FILE_TYPE "APDT"
#define APDT_EXTENSION ".apdt"
#define MAGIC "PK\x03\x04"
#define MAGIC_SIZE (sizeof(MAGIC)-1)
#define REGPATTERN "^([0-9]{4})-([0-9]{2})-([0-9]{2})T([0-2][0-9]:[0-6][0-9]:[0-6][0-9])[^+-]*([+-][0-9]{2}:[0-9]{2})$"

/*Enums*/

/*Field Types*/
typedef enum {
    FIELD_TYPE_STRING,
    FIELD_TYPE_DATE,
    FIELD_TYPE_LAST
} APEFieldType;

/*SPM modes*/
typedef enum {
    SPM_MODE_SNOM = 0,
    SPM_MODE_AFM_NONCONTACT = 1,
    SPM_MODE_AFM_CONTACT = 2,
    SPM_MODE_STM = 3,
    SPM_MODE_PHASE_DETECT_AFM = 4,
    SPM_MODE_LAST
} SPMModeType;

/*SPM Modes Labels*/
static const GwyEnum spm_modes[] = {
    { "SNOM",                  SPM_MODE_SNOM },
    { "AFM Non-contact",       SPM_MODE_AFM_NONCONTACT },
    { "AFM Contact",           SPM_MODE_AFM_CONTACT },
    { "STM",                   SPM_MODE_STM },
    { "Phase detection AFM",   SPM_MODE_PHASE_DETECT_AFM },
};

/*APDT Sensor types*/
typedef enum {
    Unknown = -1,
    Cantilever = 0,
    Capacitive = 1
} SensorType;

static const GwyEnum sensor_types[] = {
    { "Unknown",                Unknown    },
    { "Cantilever",             Cantilever },
    { "Capacitive",             Capacitive }
 };

/*Structures*/
typedef struct {
    gint XRes;
    gint YRes;
    gdouble XReal;
    gdouble YReal;
} APEScanSize;

typedef struct {
    APEFieldType type;
    gchar *name;
    gchar *xpath;
    gboolean optional;
} APEXmlField;

/*XML fields arrays*/
static const APEXmlField dax_afm_c[] = {
    {FIELD_TYPE_STRING, "File Version", "/Scan/Header/FileVersion", FALSE},
    {FIELD_TYPE_DATE, "Date", "/Scan/Header/Date", FALSE},
    {FIELD_TYPE_STRING, "Remark", "/Scan/Header/Remark", TRUE},
    {FIELD_TYPE_STRING, "BIAS DC Voltage", "/Scan/Header/VPmt1", FALSE}
};

static const APEXmlField dax_afm_nc[] = {
    {FIELD_TYPE_STRING, "File Version", "/Scan/Header/FileVersion", FALSE},
    {FIELD_TYPE_DATE, "Date", "/Scan/Header/Date", FALSE},
    {FIELD_TYPE_STRING, "Remark", "/Scan/Header/Remark", TRUE},
    {FIELD_TYPE_STRING, "Tip Oscillation Frequency", "/Scan/Header/TipOscFreq", FALSE},
    {FIELD_TYPE_STRING, "BIAS DC Voltage", "/Scan/Header/VPmt1", FALSE}
};

static const APEXmlField dax_snom[] = {
    {FIELD_TYPE_STRING, "File Version", "/Scan/Header/FileVersion", FALSE},
    {FIELD_TYPE_DATE, "Date", "/Scan/Header/Date", FALSE},
    {FIELD_TYPE_STRING, "Remark", "/Scan/Header/Remark", TRUE},
    {FIELD_TYPE_STRING, "Tip Oscillation Frequency", "/Scan/Header/TipOscFreq", FALSE},
    {FIELD_TYPE_STRING, "PMT 1 Voltage", "/Scan/Header/VPmt1", FALSE},
    {FIELD_TYPE_STRING, "PMT 2 Voltage", "/Scan/Header/VPmt2", FALSE}
};

static const APEXmlField dax_stm[] = {
    {FIELD_TYPE_STRING, "File Version", "/Scan/Header/FileVersion", FALSE},
    {FIELD_TYPE_DATE, "Date", "/Scan/Header/Date", FALSE},
    {FIELD_TYPE_STRING, "Remark", "/Scan/Header/Remark", TRUE}
};

static const APEXmlField apdt_std[] = {
    {FIELD_TYPE_STRING, "File Version", "/Scan/Header/FileVersion", FALSE},
    {FIELD_TYPE_STRING, "Project Name", "/Scan/Header/ProjectName", TRUE},
    {FIELD_TYPE_STRING, "Sensor Type", "/Scan/Header/SensorType", FALSE},
    {FIELD_TYPE_STRING, "Exchange Axes", "/Scan/Header/ExchangeAxes", FALSE},
    {FIELD_TYPE_DATE, "Date", "/Scan/Header/Date", FALSE},
    {FIELD_TYPE_STRING, "Remark", "/Scan/Header/Remark", TRUE}
};

/*Prototypes*/

static gboolean      module_register               (void);
static gint          apedax_detect                 (const GwyFileDetectInfo *fileinfo,
                                                    gboolean only_name);
static GwyContainer* apedax_load                   (const gchar *filename,
                                                    GwyRunType mode,
                                                    GError **error);
static GwyContainer* apedax_get_meta               (guchar *scanXmlContent,
                                                    gsize contentSize,
                                                    APEScanSize *scanSize,
                                                    gboolean apdtFile);
static guchar*       apedax_get_file_content       (unzFile uFile,
                                                    unz_file_info *uFileInfo,
                                                    gsize *size,
                                                    GError **error);
static gchar*        apedax_get_xml_field_as_string(xmlDocPtr doc,
                                                    const gchar *fieldXPath);
static gboolean      apedax_set_meta_field         (GwyContainer *meta,
                                                    xmlDocPtr doc,
                                                    APEXmlField data);
static GwyDataField* apedax_get_data_field         (unzFile uFile,
                                                    const gchar *chFileName,
                                                    const APEScanSize *scanSize,
                                                    gchar *zUnit,
                                                    gdouble scale,
                                                    GError **error);
static void          apedax_get_channels_data      (unzFile uFile,
                                                    guchar *scanXmlContent,
                                                    gsize contentSize,
                                                    const gchar *filename,
                                                    GwyContainer *container,
                                                    GwyContainer *meta,
                                                    const APEScanSize *scanSize,
                                                    GError **error);
static gchar*        apedax_format_date             (const gchar* datefield);

/*Informations about the module*/

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports A.P.E. Research DAX data files."),
    "Andrea Cervesato <infos@aperesearch.com>, Gianfranco Gallizia <infos@aperesearch.com>",
    "0.4",
    "A.P.E. Research srl",
    "2015"
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("apedaxfile",
                           N_("A.P.E. Research DAX Files (.dax) and APDT File (.apdt)"),
                           (GwyFileDetectFunc)&apedax_detect,
                           (GwyFileLoadFunc)&apedax_load,
                           NULL,
                           NULL);

    return TRUE;
}

/*Detect function*/

static gint
apedax_detect(const GwyFileDetectInfo *fileinfo,
              gboolean only_name)
{
    gint score = 0;
    unzFile uFile;

    score += (g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0);
    score += (g_str_has_suffix(fileinfo->name_lowercase, APDT_EXTENSION) ? 10 : 0);

    if (only_name)
        return score;

    if (fileinfo->file_size > MAGIC_SIZE
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0)
        score += 30;
    else
        return 0;

    gwy_debug("Opening the file with MiniZIP");
    uFile = unzOpen(fileinfo->name);

    if (uFile == NULL) {
        unzClose(uFile);
        return 0;
    }

    if (unzLocateFile(uFile, "scan.xml", 0) == UNZ_OK) {
        score += 30;
    } else {
        score = 0;
    }

    unzClose(uFile);

    return score;
}

/*Load function*/

static GwyContainer*
apedax_load(const gchar *filename,
            G_GNUC_UNUSED GwyRunType mode,
            GError **error)
{
    GwyContainer *container = NULL;
    GwyContainer *meta = NULL;
    unzFile uFile;
    unz_file_info uFileInfo;
    guchar *buffer;
    gsize size = 0;
    gboolean apdt_flag = FALSE;
    gchar *lowercaseFilename;
    APEScanSize scanSize;

    scanSize.XRes = 0;
    scanSize.YRes = 0;
    scanSize.XReal = 0.0;
    scanSize.YReal = 0.0;

    lowercaseFilename = g_ascii_strdown(filename, -1);

    if (g_str_has_suffix(filename, APDT_EXTENSION)) {
        apdt_flag = TRUE;
    }

    g_free(lowercaseFilename);

    gwy_debug("Opening the file with MiniZIP");
    uFile = unzOpen(filename);

    if (uFile == NULL) {
        if (apdt_flag) {
            err_FILE_TYPE(error, APDT_FILE_TYPE);
        }
        else {
            err_FILE_TYPE(error, FILE_TYPE);
        }
        unzClose(uFile);
        return NULL;
    }

    gwy_debug("Locating the XML file");
    if (unzLocateFile(uFile, "scan.xml", 0) != UNZ_OK) {
        if (apdt_flag) {
            err_FILE_TYPE(error, APDT_FILE_TYPE);
        }
        else {
            err_FILE_TYPE(error, FILE_TYPE);
        }
        unzClose(uFile);
        return NULL;
    }

    gwy_debug("Getting the XML file info");
    if (unzGetCurrentFileInfo(uFile,
                              &uFileInfo,
                              NULL,
                              0L,
                              NULL,
                              0L,
                              NULL,
                              0L) != UNZ_OK) {
        err_OPEN_READ(error);
        unzClose(uFile);
        return NULL;
    }

    buffer = apedax_get_file_content(uFile, &uFileInfo, &size, error);

    container = gwy_container_new();

    meta = apedax_get_meta(buffer, size, &scanSize, apdt_flag);

    if (meta == NULL) {
        gwy_debug("Metadata Container is NULL");
        g_object_unref(container);
        g_free(buffer);
        err_FILE_TYPE(error, FILE_TYPE);
        unzClose(uFile);
        return NULL;
    }

    apedax_get_channels_data(uFile,
                             buffer,
                             size,
                             filename,
                             container,
                             meta,
                             &scanSize,
                             error);

    g_free(buffer);
    g_object_unref(meta);

    unzClose(uFile);

    return container;
}

/*Gets the binary content of a file within a ZIP file*/

static guchar*
apedax_get_file_content(unzFile uFile,
                        unz_file_info *uFileInfo,
                        gsize *size,
                        GError **error)
{
    guchar *buffer = NULL;

    gwy_debug("Reading binary data");
    if (uFile == NULL || uFileInfo == NULL) {
        err_OPEN_READ(error);
        return NULL;
    }

    *size = (gsize)(uFileInfo->uncompressed_size);

    buffer = g_new(guchar, *size + 1);
    buffer[*size] = '\0';

    if (buffer && unzOpenCurrentFile(uFile) == UNZ_OK) {
        gint readsize = 0;
        readsize = unzReadCurrentFile(uFile, buffer, *size);
        *size = readsize;
    }

    return buffer;
}

/*Sets the field into the metadata container*/
static gboolean      apedax_set_meta_field         (GwyContainer *meta,
                                                    xmlDocPtr doc,
                                                    APEXmlField data)
{
    gboolean outcome = FALSE;
    gchar* buffer = NULL;

    buffer = apedax_get_xml_field_as_string(doc, data.xpath);

    if (buffer) {

        switch (data.type) {
            case FIELD_TYPE_STRING:
                gwy_container_set_string_by_name(meta,
                                                 data.name,
                                                 g_strdup(buffer));
                break;
            case FIELD_TYPE_DATE:
                {
                    gchar *datestring = apedax_format_date(buffer);

                    if (datestring != NULL) {
                        gwy_container_set_string_by_name(meta,
                                                         "Date",
                                                         g_strdup(datestring));
                        g_free(datestring);
                    }
                    else {
                        g_free(buffer);
                        return FALSE;
                    }
                }
                break;
            default:
                break;
        }

        g_free(buffer);
        outcome = TRUE;

    }

    return outcome;
}

/*Gets the metadata from the XML file*/
static GwyContainer*
apedax_get_meta(guchar *scanXmlContent,
                gsize contentSize,
                APEScanSize *scanSize,
                gboolean apdtFile)
{
    GwyContainer* meta = NULL;
    xmlDocPtr doc = NULL;
    xmlNodePtr cur = NULL;
    gchar* buffer = NULL;
    gint currentSPMMode = -1;
    const APEXmlField *fields = NULL;
    guint fields_size = 0;
    guint i = 0;

    if (scanXmlContent == NULL || contentSize == 0)
        return NULL;

    meta = gwy_container_new();

    gwy_debug("Parsing the scan XML file");
    doc = xmlReadMemory(scanXmlContent,
                        contentSize,
                        "scan.xml",
                        NULL,
                        0);

    if (doc == NULL)
        goto fail;

    /*Check for the right XML root*/
    cur = xmlDocGetRootElement(doc);

    if (cur == NULL)
        goto fail;

    if (xmlStrcmp(cur->name, (const xmlChar*)"Scan"))
        goto fail;

    gwy_debug("Populating metadata container");

    if (apdtFile) {
        gwy_debug("Selected APDT file fields");
        /*Set fields and fields_size*/
        fields = apdt_std;
        fields_size = G_N_ELEMENTS(apdt_std);
    }
    else {
        gwy_debug("Selected DAX file fields");
        /*Fetch the SPM Mode*/
        buffer = apedax_get_xml_field_as_string(doc,
                                                "/Scan/Header/SpmMode");

        if (buffer) {
            gwy_container_set_string_by_name(meta,
                                             "SPM Mode",
                                             g_strdup(buffer));

            currentSPMMode = gwy_string_to_enum(buffer,
                                                spm_modes,
                                                G_N_ELEMENTS(spm_modes));

            /*Set fields and fields_size according to SPM Mode*/
            switch (currentSPMMode) {
                case SPM_MODE_AFM_CONTACT:
                    fields = dax_afm_c;
                    fields_size = G_N_ELEMENTS(dax_afm_c);
                    break;
                case SPM_MODE_AFM_NONCONTACT:
                    fields = dax_afm_nc;
                    fields_size = G_N_ELEMENTS(dax_afm_nc);
                    break;
                case SPM_MODE_SNOM:
                    fields = dax_snom;
                    fields_size = G_N_ELEMENTS(dax_snom);
                    break;
                case SPM_MODE_STM:
                    fields = dax_stm;
                    fields_size = G_N_ELEMENTS(dax_stm);
                    break;
            }

            g_free(buffer);
        }
        else {
            gwy_debug("Cannot get SpmMode field");
            goto fail;
        }
    }

    for (i = 0; i < fields_size; i++) {
        gboolean result = apedax_set_meta_field(meta, doc, fields[i]);
        if (result == FALSE) {
            gwy_debug("Cannot get %s field", fields[i].xpath);
            if (fields[i].optional == FALSE)
                goto fail;
        }
    }

    /*Number of columns (XRes)*/
    buffer = apedax_get_xml_field_as_string(doc,
                                            "/Scan/Header/ScanSize/XRes");

    if (buffer) {
        scanSize->XRes = (gint)g_ascii_strtod(buffer, NULL);
        g_free(buffer);
    }
    else {
        goto fail;
    }

    /*Number of rows (YRes)*/
    buffer = apedax_get_xml_field_as_string(doc,
                                            "/Scan/Header/ScanSize/YRes");

    if (buffer) {
        scanSize->YRes = (gint)g_ascii_strtod(buffer, NULL);
        g_free(buffer);
    }
    else {
        goto fail;
    }

    /*Width in nanometers*/
    buffer = apedax_get_xml_field_as_string(doc,
                                            "/Scan/Header/ScanSize/X");

    if (buffer) {
        scanSize->XReal = g_ascii_strtod(buffer, NULL);
        scanSize->XReal *= 1e-9; /*nm to m conversion*/
        g_free(buffer);
    }
    else {
        goto fail;
    }

    /*Height in nanometers*/
    buffer = apedax_get_xml_field_as_string(doc,
                                            "/Scan/Header/ScanSize/Y");

    if (buffer) {
        scanSize->YReal = g_ascii_strtod(buffer, NULL);
        scanSize->YReal *= 1e-9; /*nm to m conversion*/
        g_free(buffer);
    }
    else {
        goto fail;
    }

    xmlFreeDoc(doc);

    gwy_debug("Returning metadata container");

    return meta;

fail:
    gwy_debug("Cleaning up after a fail");
    if (doc)
        xmlFreeDoc(doc);
    gwy_object_unref(meta);
    return NULL;
}

static gchar*
apedax_get_xml_field_as_string(xmlDocPtr doc, const gchar *fieldXPath)
{
    gchar *fieldString = NULL;
    xmlChar *xFieldString = NULL;
    xmlXPathContextPtr context;
    xmlXPathObjectPtr pathObj;
    xmlNodeSetPtr nodeset;

    context = xmlXPathNewContext(doc);

    if (context == NULL)
        return NULL;

    pathObj = xmlXPathEvalExpression((const xmlChar*)fieldXPath, context);

    if (pathObj == NULL) {
        xmlXPathFreeContext(context);
        return NULL;
    }

    if (xmlXPathNodeSetIsEmpty(pathObj->nodesetval)) {
        xmlXPathFreeObject(pathObj);
        xmlXPathFreeContext(context);
        return NULL;
    }

    nodeset = pathObj->nodesetval;

    if (nodeset->nodeNr == 1) {
        xFieldString = xmlNodeListGetString(doc,
                                            nodeset->nodeTab[0]->xmlChildrenNode,
                                            1);
        fieldString = g_strdup((gchar*)xFieldString);
        xmlFree(xFieldString);
    }

    xmlXPathFreeObject(pathObj);
    xmlXPathFreeContext(context);

    return fieldString;
}

static GwyDataField*
apedax_get_data_field(unzFile uFile,
                      const gchar *chFileName,
                      const APEScanSize *scanSize,
                      gchar *zUnit,
                      gdouble scale,
                      GError **error)
{
    GwyDataField *dfield = NULL;
    GwySIUnit *xyUnit;
    GwySIUnit *zSIUnit;
    gdouble *data;
    guchar *buffer;
    gsize size, expectedSize;
    unz_file_info uFileInfo;

    /*Checking the dimensions*/
    if (err_DIMENSION(error, scanSize->XRes)) {
        return NULL;
    }

    if (err_DIMENSION(error, scanSize->YRes)) {
        return NULL;
    }

    /*If XReal it's not greater than 0 or XReal is NaN*/
    if (!(fabs(scanSize->XReal) > 0)) {
        err_UNSUPPORTED(error, "X scan size");
        return NULL;
    }

    /*Same for YReal*/
    if (!(fabs(scanSize->YReal) > 0)) {
        err_UNSUPPORTED(error, "Y scan size");
        return NULL;
    }

    expectedSize = scanSize->XRes * scanSize->YRes * sizeof(gdouble);

    unzGoToFirstFile(uFile);

    if (unzLocateFile(uFile, chFileName, 0) != UNZ_OK) {
        gwy_debug("Binary file not found");
        err_NO_DATA(error);
        return NULL;
    }

    if (unzGetCurrentFileInfo(uFile,
                              &uFileInfo,
                              NULL,
                              0L,
                              NULL,
                              0L,
                              NULL,
                              0L) != UNZ_OK) {
        err_NO_DATA(error);
        return NULL;
    }

    buffer = apedax_get_file_content(uFile, &uFileInfo, &size, error);

    if (buffer == NULL) {
        err_NO_DATA(error);
        return NULL;
    }

    if (err_SIZE_MISMATCH(error, expectedSize, size, FALSE)) {
       return NULL;
    }

    dfield = gwy_data_field_new(scanSize->XRes, scanSize->YRes,
                                scanSize->XReal, scanSize->YReal,
                                FALSE);

    data = gwy_data_field_get_data(dfield);

    xyUnit = gwy_data_field_get_si_unit_xy(dfield);
    gwy_si_unit_set_from_string(xyUnit, "m");

    zSIUnit = gwy_data_field_get_si_unit_z(dfield);
    gwy_si_unit_set_from_string(zSIUnit, zUnit);

    gwy_debug("Reading RAW data");

    gwy_convert_raw_data(buffer,
                         scanSize->XRes * scanSize->YRes,
                         1,
                         GWY_RAW_DATA_DOUBLE,
                         GWY_BYTE_ORDER_LITTLE_ENDIAN,
                         data,
                         scale,
                         0.0);

    return dfield;
}

static void
apedax_get_channels_data(unzFile uFile,
                         guchar *scanXmlContent,
                         gsize contentSize,
                         const gchar *filename,
                         GwyContainer *container,
                         GwyContainer *meta,
                         const APEScanSize *scanSize,
                         GError **error)
{
    xmlDocPtr doc = NULL;
    xmlNodePtr cur = NULL;
    xmlXPathContextPtr context;
    xmlXPathObjectPtr pathObj;
    xmlNodeSetPtr nodeset;
    xmlChar *buffer = NULL;
    gchar key[256];
    gint i;
    gint power10 = 0;
    gdouble scaleFactor = 1.0;
    GwySIUnit *zUnit;
    gchar *zUnitString = NULL;
    gchar *binFileName = NULL;
    GwyDataField *dfield;
    GwyContainer *tmp;

    if (scanXmlContent == NULL || contentSize == 0)
        return;

    gwy_clear(key, sizeof(key));

    doc = xmlReadMemory(scanXmlContent,
                        contentSize,
                        "scan.xml",
                        NULL,
                        0);

    if (doc == NULL)
        goto fail;

    context = xmlXPathNewContext(doc);

    if (context == NULL)
        goto fail;

    pathObj = xmlXPathEvalExpression("/Scan/Channels/Channel", context);

    if (pathObj == NULL) {
        xmlXPathFreeContext(context);
        goto fail;
    }

    /*There must be at least one channel*/
    if (xmlXPathNodeSetIsEmpty(pathObj->nodesetval)) {
        xmlXPathFreeObject(pathObj);
        xmlXPathFreeContext(context);
        err_NO_DATA(error);
        return;
    }

    nodeset = pathObj->nodesetval;

    if (nodeset->nodeNr <= 0)
        goto fail;

    for (i = 0; i < nodeset->nodeNr; i++) {

        cur = nodeset->nodeTab[i]->xmlChildrenNode;

        while (cur) {
            /*Label*/
            if (gwy_strequal((gchar*)cur->name, "Label")) {
                buffer = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                g_snprintf(key, sizeof(key), "/%d/data/title", i);
                gwy_container_set_string_by_name(container, key, g_strdup(buffer));
                xmlFree(buffer);
            }
            /*Factor*/
            if (gwy_strequal((gchar*)cur->name, "ConversionFactor")) {
                buffer = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                scaleFactor = g_ascii_strtod((gchar*)buffer, NULL);
                xmlFree(buffer);
            }
            /*Unit*/
            if (gwy_strequal((gchar*)cur->name, "DataUnit")) {
                buffer = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                zUnitString = g_strdup((gchar*)buffer);
                zUnit = gwy_si_unit_new_parse(zUnitString, &power10);
                xmlFree(buffer);
                g_object_unref(zUnit);
            }
            /*Binary file name*/
            if (gwy_strequal((gchar*)cur->name, "BINFile")) {
                buffer = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                binFileName = g_strdup((gchar*)buffer);
                xmlFree(buffer);
            }
            cur = cur->next;
        }

        scaleFactor *= pow(10.0, power10);

        dfield = apedax_get_data_field(uFile,
                                       binFileName,
                                       scanSize,
                                       zUnitString,
                                       scaleFactor,
                                       error);
        if (dfield) {
            g_snprintf(key, sizeof(key), "/%d/data", i);
            gwy_container_set_object_by_name(container, key, dfield);
            g_object_unref(dfield);
            gwy_file_channel_import_log_add(container, i, NULL,
                                            filename);

            tmp = gwy_container_duplicate(meta);
            g_snprintf(key, sizeof(key), "/%d/meta", i);
            gwy_container_set_object_by_name(container, key, tmp);
            g_object_unref(tmp);
        }
    }

    xmlXPathFreeObject(pathObj);
    xmlXPathFreeContext(context);
    xmlFreeDoc(doc);

    return;

fail:
    err_FILE_TYPE(error, FILE_TYPE);
    if (doc)
        xmlFreeDoc(doc);
    return;
}

static gchar*
apedax_format_date(const gchar* datefield)
{
    GRegex *re;
    gchar *result;
    GError *re_err = NULL;

    gwy_debug("Compiling the Regular expression.");
    re = g_regex_new(REGPATTERN, 0, 0, &re_err);
    g_assert(!re_err);
    result = g_regex_replace(re, datefield, -1, 0, "\\3-\\2-\\1 \\4 \\5", 0,
                             &re_err);
    if (re_err) {
        g_warning("Invalid date field (%s)", re_err->message);
        g_clear_error(&re_err);
    }
    g_regex_unref(re);
    return result;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
