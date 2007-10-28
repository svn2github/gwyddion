<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                output="text"
                omit-xml-declaration="yes"
                indent="no"
                encoding="utf-8"
                version="1.0">

<xsl:strip-space elements="term indexterm"/>

<xsl:template match="para">
  <xsl:apply-templates/>
  <xsl:text>\par</xsl:text>
</xsl:template>

<xsl:template match="chapter/title">
  <xsl:text>\chapter{</xsl:text><xsl:value-of select="."/><xsl:text>}</xsl:text>
</xsl:template>

<xsl:template match="sect1/title">
  <xsl:text>\section{</xsl:text><xsl:value-of select="."/><xsl:text>}</xsl:text>
</xsl:template>

<xsl:template match="sect2/title">
  <xsl:text>\subsection{</xsl:text><xsl:value-of select="."/><xsl:text>}</xsl:text>
</xsl:template>

<xsl:template match="orderedlist">
  <xsl:text>\begin{enumerate}</xsl:text>
  <xsl:apply-templates/>
  <xsl:text>\end{enumerate}</xsl:text>
</xsl:template>

<xsl:template match="orderedlist/listitem">
  <xsl:text>\item</xsl:text>
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="variablelist">
  <xsl:text>\begin{description}</xsl:text>
  <xsl:apply-templates/>
  <xsl:text>\end{description}</xsl:text>
</xsl:template>

<xsl:template match="variablelist/varlistentry">
  <xsl:text>\item</xsl:text>
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="varlistentry/term">
  <xsl:text>[</xsl:text><xsl:apply-templates/><xsl:text>]</xsl:text>
</xsl:template>

<xsl:template match="varlistentry/listitem">
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="literallayout|programlisting">
  <xsl:text>\begin{verbatim}</xsl:text>
  <xsl:apply-templates/>
  <xsl:text>\end{verbatim}</xsl:text>
</xsl:template>

<xsl:template match="informalexample/userinput">
  <xsl:text>\begin{verbatim}</xsl:text>
  <xsl:apply-templates/>
  <xsl:text>\end{verbatim}</xsl:text>
</xsl:template>

<xsl:template match="literal|userinput|filename">
  <xsl:text>\verb`</xsl:text><xsl:apply-templates/><xsl:text>`</xsl:text>
</xsl:template>

<xsl:template match="indexterm">
  <xsl:text>\index{</xsl:text><xsl:apply-templates/><xsl:text>}</xsl:text>
</xsl:template>

<xsl:template match="textobject"/>

<xsl:template match="textobject[@role='tex']">
  <xsl:text>\[</xsl:text><xsl:apply-templates/><xsl:text>\]</xsl:text>
</xsl:template>

<xsl:template match="variablelist/title">
  <!-- FIXME -->
</xsl:template>

<xsl:template match="orderedlist/title">
  <!-- FIXME -->
</xsl:template>

<xsl:template match="figure">
  <xsl:text>\begin{figure}[h]</xsl:text>
  <xsl:apply-templates/>
  <xsl:text>\label{</xsl:text><xsl:value-of select="@id"/><xsl:text>}</xsl:text>
  <xsl:text>\end{figure}</xsl:text>
</xsl:template>

<xsl:template match="figure/title">
  <xsl:text>\caption{</xsl:text><xsl:apply-templates/><xsl:text>}</xsl:text>
</xsl:template>

<xsl:template match="figure/mediaobject/imageobject/imagedata">
  <xsl:text>\bgroup\catcode`\_=11\includegraphics[scale=0.5]{</xsl:text><xsl:value-of select="@fileref"/><xsl:text>}\egroup</xsl:text>
</xsl:template>

</xsl:stylesheet>
