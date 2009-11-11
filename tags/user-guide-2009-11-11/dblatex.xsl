<?xml version='1.0'?>
<!-- $Id$
     vim: set ts=2 sw=2 et :
     customize dblatex XSL stylesheets -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version='1.0'>

<xsl:param name='gwy.latex.language'>
  <xsl:choose>
    <xsl:when test='/book[@xml:lang="en"]'>
      <xsl:text>english</xsl:text>
    </xsl:when>
    <xsl:when test='/book[@xml:lang="fr"]'>
      <xsl:text>french</xsl:text>
    </xsl:when>
    <xsl:when test='/book[@xml:lang="ru"]'>
      <xsl:text>russian</xsl:text>
    </xsl:when>
    <xsl:otherwise>
      <!-- As good as anything else and we don't have to special-case it... -->
      <xsl:text>english</xsl:text>
      <xsl:message>Warning: xml:lang is not set on book, cannot determine the language.</xsl:message>
    </xsl:otherwise>
  </xsl:choose>
</xsl:param>

<xsl:param name="latex.encoding">utf8</xsl:param>
<xsl:param name="latex.class.options">a4paper,10pt,twoside,<xsl:value-of select='$gwy.latex.language'/></xsl:param>

<!-- Note: All XeTeX stuff removed, also UTF-8 branching because we always
     use UTF-8.  -->
<xsl:template name="encode.before.style">
  <xsl:text>\newif\ifxetex&#10;\xetexfalse&#10;</xsl:text>
  <!-- Standard latex font setup -->
  <!--
  <xsl:choose>
    <xsl:when test='$gwy.latex.language="russian"'>
      <xsl:text>\usepackage[T2A,T2D]{fontenc}&#10;</xsl:text>
    </xsl:when>
    <xsl:otherwise>
    -->
      <xsl:text>\usepackage[T2A,T2D,T1]{fontenc}&#10;</xsl:text>
   <!--
    </xsl:otherwise>
  </xsl:choose>
  -->
  <xsl:text>\usepackage{ucs}&#10;</xsl:text>
  <xsl:text>\usepackage[utf8x]{inputenc}&#10;</xsl:text>
  <xsl:text>\def\hyperparamadd{unicode=true}&#10;</xsl:text>
</xsl:template>

<xsl:template name="encode.after.style">
  <xsl:text>\lstset{inputencoding=utf8x, extendedchars=\true}&#10;</xsl:text>
</xsl:template>

<xsl:variable name="latex.begindocument">
  <xsl:text>\usepackage{ltablex}&#10;</xsl:text>
  <xsl:text>\usepackage{mathtext}&#10;</xsl:text>
  <xsl:text>\usepackage{cmap}&#10;</xsl:text>
  <xsl:text>\usepackage[</xsl:text>
  <xsl:value-of select='$gwy.latex.language'/>
  <xsl:text>]{babel}&#10;</xsl:text>
  <xsl:text>\input{user-guide.sty}&#10;</xsl:text>
  <xsl:text>\begin{document}&#10;</xsl:text>
</xsl:variable>

<xsl:template match='informaltable[@role="toolbox"]'/>

<xsl:template match="releaseinfo" mode="docinfo">
  <xsl:text>\def\gwyreleaseinfo{</xsl:text>
  <xsl:apply-templates select="."/>
  <xsl:text>}&#10;</xsl:text>
</xsl:template>

<!-- *********************************************************************

         Captions

     ********************************************************************* -->
<xsl:template match="mediaobject/caption">
  <xsl:text>\gwycaption{</xsl:text>
  <xsl:value-of select="$mediaobject.caption.style"/>
  <xsl:text> </xsl:text>
  <xsl:apply-templates/>
  <!-- dblatex calls "normalize-scape" on "."  This unfortunately kills
       formulas embedded in the caption. -->
  <xsl:text>}</xsl:text>
</xsl:template>

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
  <!-- If there is <?dblatex COLSPEC?> use that as the column specification.
       Processing instructions are ugly but the HTML and LaTeX table models
       are too different. -->
  <xsl:variable name='colspec'>
    <xsl:choose>
      <xsl:when test='processing-instruction("dblatex")'>
        <xsl:value-of select='processing-instruction("dblatex")'/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:variable name='cols'>
          <xsl:value-of select="@cols"/>
        </xsl:variable>
        <xsl:call-template name='repeat'>
          <xsl:with-param name='count' select='$cols'/>
          <xsl:with-param name='text' select='"X"'/>
        </xsl:call-template>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>
  <xsl:choose>
    <xsl:when test='contains($colspec, "X")'>
      <xsl:text>\begin{tabularx}{\linewidth}{</xsl:text>
      <xsl:value-of select='$colspec'/>
      <xsl:text>}&#10;</xsl:text>
      <xsl:apply-templates mode='tabularx' select='thead'/>
      <xsl:apply-templates select='tbody'/>
      <xsl:text>\end{tabularx}&#10;</xsl:text>
    </xsl:when>
    <xsl:otherwise>
      <xsl:text>\begin{tabular}{</xsl:text>
      <xsl:value-of select='$colspec'/>
      <xsl:text>}&#10;</xsl:text>
      <xsl:apply-templates mode='tabular' select='thead'/>
      <xsl:apply-templates select='tbody'/>
      <xsl:text>\noalign{\hrule}&#10;</xsl:text>
      <xsl:text>\end{tabular}&#10;</xsl:text>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template match='thead/row|tbody/row'>
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match='table/title'/>

<xsl:template match='colspec'/>

<xsl:template match='thead' mode='tabularx'>
  <xsl:text>\noalign{\hrule}&#10;</xsl:text>
  <xsl:text>\noalign{\smallskip}&#10;</xsl:text>
  <xsl:apply-templates/>
  <xsl:text>\noalign{\hrule}&#10;</xsl:text>
  <xsl:text>\noalign{\smallskip}&#10;</xsl:text>
  <xsl:text>\endhead&#10;</xsl:text>
  <xsl:text>\noalign{\hrule}&#10;</xsl:text>
  <xsl:text>\endfoot&#10;</xsl:text>
</xsl:template>

<xsl:template match='thead' mode='tabular'>
  <xsl:text>\noalign{\hrule}&#10;</xsl:text>
  <xsl:text>\noalign{\smallskip}&#10;</xsl:text>
  <xsl:apply-templates/>
  <xsl:text>\noalign{\hrule}&#10;</xsl:text>
  <xsl:text>\noalign{\smallskip}&#10;</xsl:text>
</xsl:template>

<xsl:template match='table|informaltable|tbody'>
  <xsl:apply-templates/>
</xsl:template>

</xsl:stylesheet>
