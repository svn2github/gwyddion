<?xml version="1.0"?>
<!-- $Id$
     extract equations in mediaobject/textobject/phrase of equations to TeX
     files for further processing -->
<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:exsl="http://exslt.org/common"
                extension-element-prefixes="exsl">

<xsl:template name="write-eq">
  <xsl:param name="filename"/>
  <xsl:param name="formula"/>
  <!-- Only xsltproc is supported, DocBook styles are smarter... -->
  <xsl:choose>
    <xsl:when test="element-available('exsl:document')">
      <!--
      <xsl:message>
        <xsl:text>Writing </xsl:text>
        <xsl:value-of select="$filename"/>
      </xsl:message>
      -->
      <!-- Seems we need omit-xml-declaration explicitly here -->
      <exsl:document href="{$filename}"
                     method="text"
                     indent="no"
                     encoding="utf-8"
                     omit-xml-declaration="yes">
        <xsl:copy-of select="normalize-space($formula)"/>
        <xsl:text>&#10;</xsl:text>
      </exsl:document>
    </xsl:when>
    <xsl:otherwise>
      <xsl:message terminate="yes">
        <xsl:text>Can't write files with </xsl:text>
        <xsl:value-of select="system-property('xsl:vendor')"/>
        <xsl:text>'s processor.</xsl:text>
      </xsl:message>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template match="/">
  <xsl:for-each select="//informalequation">
    <xsl:choose>
      <xsl:when test="not(@id)">
        <xsl:message>
          <xsl:text>Missing id for </xsl:text>
          <xsl:value-of select="name(.)"/>
          <xsl:text> </xsl:text>
          <xsl:value-of select="mediaobject/imageobject/imagedata/attribute::fileref"/>
        </xsl:message>
      </xsl:when>
      <xsl:when test="not(mediaobject/textobject[@role='tex'])">
        <xsl:message>
          <xsl:text>Missing TeX textobject for </xsl:text>
          <xsl:value-of select="name(.)"/>
          <xsl:text> </xsl:text>
          <xsl:value-of select="@id"/>
        </xsl:message>
      </xsl:when>
      <xsl:otherwise>
        <xsl:call-template name="write-eq">
          <xsl:with-param name="filename">
            <xsl:text>formulas/</xsl:text>
            <xsl:value-of select="@id"/>
            <xsl:text>.tex.in</xsl:text>
          </xsl:with-param>
          <xsl:with-param name="formula">
            <xsl:value-of select="mediaobject/textobject[@role='tex']"/>
          </xsl:with-param>
        </xsl:call-template>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:for-each>

  <xsl:for-each select="//inlineequation">
    <xsl:choose>
      <xsl:when test="not(@id)">
        <xsl:if test="not(mathphrase)">
          <xsl:message>
            <xsl:text>Missing id or mathphrase for </xsl:text>
            <xsl:value-of select="name(.)"/>
            <xsl:text> </xsl:text>
            <xsl:value-of select="inlinemediaobject/imageobject/imagedata/attribute::fileref"/>
          </xsl:message>
        </xsl:if>
        <!-- If there is a mathphrase, do not warn, it's rendered in HTML. -->
      </xsl:when>
      <xsl:when test="not(inlinemediaobject/textobject[@role='tex'])">
        <xsl:message>
          <xsl:text>Missing TeX textobject for </xsl:text>
          <xsl:value-of select="name(.)"/>
          <xsl:text> </xsl:text>
          <xsl:value-of select="@id"/>
        </xsl:message>
      </xsl:when>
      <xsl:otherwise>
        <xsl:call-template name="write-eq">
          <xsl:with-param name="filename">
            <xsl:text>formulas/</xsl:text>
            <xsl:value-of select="@id"/>
            <xsl:text>.tex.in</xsl:text>
          </xsl:with-param>
          <xsl:with-param name="formula">
            <xsl:value-of select="inlinemediaobject/textobject[@role='tex']"/>
          </xsl:with-param>
        </xsl:call-template>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:for-each>
</xsl:template>

</xsl:stylesheet>
