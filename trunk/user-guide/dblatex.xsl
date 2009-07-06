<?xml version='1.0'?>
<!-- $Id$
     customize dblatex XSL stylesheets -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version='1.0'>

<xsl:param name="latex.encoding">utf8</xsl:param>

<xsl:template match='informalequation'>$$<xsl:value-of select='normalize-space(mediaobject/textobject/phrase)'/>$$</xsl:template>

<xsl:template match='inlineequation'>$<xsl:value-of select='normalize-space(alt)'/>$</xsl:template>

</xsl:stylesheet>
