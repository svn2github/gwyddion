<?xml version='1.0'?>
<!-- $Id$
     customize dblatex XSL stylesheets -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version='1.0'>

<xsl:param name="latex.encoding">utf8</xsl:param>
<xsl:param name="latex.class.options">a4paper</xsl:param>

<xsl:variable name="latex.begindocument">
  <xsl:text>\input{user-guide.sty}&#10;</xsl:text>
  <xsl:text>\begin{document}&#10;</xsl:text>
</xsl:variable>

<xsl:template match='informalequation'>$$<xsl:text>&#32;</xsl:text><xsl:if test='mediaobject/textobject[@role="tex"]/phrase'><xsl:value-of select='normalize-space(mediaobject/textobject[@role="tex"]/phrase)'/></xsl:if><xsl:if test='alt'><xsl:value-of select='normalize-space(alt)'/></xsl:if><xsl:text>&#32;</xsl:text>$$</xsl:template>

<xsl:template match='inlineequation'>$<xsl:text>&#32;</xsl:text><xsl:if test='inlinemediaobject/textobject[@role="tex"]/phrase'><xsl:value-of select='normalize-space(inlinemediaobject/textobject[@role="tex"]/phrase)'/></xsl:if><xsl:if test='alt'><xsl:value-of select='normalize-space(alt)'/></xsl:if><xsl:text>&#32;</xsl:text>$</xsl:template>

</xsl:stylesheet>
