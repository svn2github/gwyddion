<?xml version='1.0'?>
<!-- $Id$
     customize dblatex XSL stylesheets -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version='1.0'>

<xsl:param name="latex.encoding">utf8</xsl:param>
<xsl:param name="latex.class.options">a4paper</xsl:param>

<xsl:variable name="latex.begindocument">
  <xsl:text>\usepackage{ltablex}&#10;</xsl:text>
  <xsl:text>\input{user-guide.sty}&#10;</xsl:text>
  <xsl:text>\begin{document}&#10;</xsl:text>
</xsl:variable>

<xsl:template match='informaltable[@role="toolbox"]'/>

<!-- *********************************************************************

         Formulas

     ********************************************************************* -->

<xsl:template match='informalequation'>
  <xsl:text>$$&#32;</xsl:text>
  <xsl:if test='mediaobject/textobject[@role="tex"]/phrase'>
    <xsl:value-of select='normalize-space(mediaobject/textobject[@role="tex"]/phrase)'/>
  </xsl:if>
  <xsl:if test='alt'>
    <xsl:value-of select='normalize-space(alt)'/>
  </xsl:if>
  <xsl:text>&#32;$$</xsl:text>
</xsl:template>

<xsl:template match='inlineequation'>
  <xsl:text>$&#32;</xsl:text>
  <xsl:if test='inlinemediaobject/textobject[@role="tex"]/phrase'>
    <xsl:value-of select='normalize-space(inlinemediaobject/textobject[@role="tex"]/phrase)'/>
  </xsl:if>
  <xsl:if test='alt'>
    <xsl:value-of select='normalize-space(alt)'/>
  </xsl:if>
  <xsl:text>&#32;$</xsl:text>
</xsl:template>

<!-- *********************************************************************

         Tables

     ********************************************************************* -->

<xsl:template match='thead/row/entry'>
  <xsl:text>\bf </xsl:text>
  <xsl:apply-templates/>
  <xsl:choose>
    <xsl:when test='following-sibling::entry'>
      <xsl:text>&amp;</xsl:text>
    </xsl:when>
    <xsl:otherwise>
      <xsl:text>\\&#10;</xsl:text>
      <xsl:text>\noalign{\smallskip}&#10;</xsl:text>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template match='tbody/row/entry'>
  <xsl:apply-templates/>
  <xsl:choose>
    <xsl:when test='following-sibling::entry'>
      <xsl:text>&amp;</xsl:text>
    </xsl:when>
    <xsl:otherwise>
      <xsl:text>\\&#10;</xsl:text>
      <xsl:text>\noalign{\smallskip}&#10;</xsl:text>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template name='repeat'>
  <xsl:param name='count'/>
  <xsl:param name='text'/>
  <xsl:if test='$count != 0'>
    <xsl:value-of select='$text'/>
    <xsl:call-template name='repeat'>
      <xsl:with-param name='count' select='$count - 1'/>
      <xsl:with-param name='text' select='$text'/>
    </xsl:call-template>
  </xsl:if>
</xsl:template>

<xsl:template match='tgroup'>
  <xsl:variable name='cols'>
   <xsl:value-of select="@cols"/>
 </xsl:variable>
  <xsl:text>\begin{tabularx}{\linewidth}{</xsl:text>
  <xsl:call-template name='repeat'>
    <xsl:with-param name='count' select='$cols'/>
    <xsl:with-param name='text' select='"X"'/>
  </xsl:call-template>
  <xsl:text>}&#10;</xsl:text>
  <xsl:apply-templates/>
  <xsl:text>\end{tabularx}</xsl:text>
</xsl:template>

<xsl:template match='thead/row'>
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match='tbody/row'>
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match='table/title'>
  <!--
  <xsl:text>{\slshape </xsl:text>
  <xsl:apply-templates/>
  <xsl:text>}&#10;</xsl:text>
  -->
</xsl:template>

<xsl:template match='colspec'/>

<xsl:template match='thead'>
  <xsl:text>\noalign{\hrule}&#10;</xsl:text>
  <xsl:text>\noalign{\smallskip}&#10;</xsl:text>
  <xsl:apply-templates/>
  <xsl:text>\noalign{\hrule}&#10;</xsl:text>
  <xsl:text>\noalign{\smallskip}&#10;</xsl:text>
  <xsl:text>\endhead&#10;</xsl:text>
  <xsl:text>\noalign{\hrule}&#10;</xsl:text>
  <xsl:text>\endfoot&#10;</xsl:text>
</xsl:template>

<xsl:template match='table|tbody'>
  <xsl:apply-templates/>
</xsl:template>

</xsl:stylesheet>
