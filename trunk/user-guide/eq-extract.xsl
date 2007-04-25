<?xml version="1.0"?>
<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:exsl="http://exslt.org/common"
                extension-element-prefixes="exsl">
<xsl:output method="text"
            encoding="utf-8"
            indent="no"/>

<xsl:template name="write-eq">
  <xsl:param name="filename"/>
  <xsl:param name="formula"/>
  <!-- Only xsltproc is supported, DocBook styles are smarter... -->
  <xsl:choose>
    <xsl:when test="element-available('exsl:document')">
      <xsl:message>
        <xsl:text>Writing </xsl:text>
        <xsl:value-of select="$filename"/>
      </xsl:message>
      <!-- Seems we need omit-xml-declaration explicitly here -->
      <exsl:document href="{$filename}"
                     omit-xml-declaration="yes">
        <xsl:copy-of select="$formula"/>
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

<xsl:template match="book">
  <xsl:for-each select="//informalequation|//inlineequation">
    <xsl:choose>
      <xsl:when test="not(@id)">
        <xsl:message>
          <xsl:text>Missing id for </xsl:text>
          <xsl:value-of select="name(.)"/>
          <xsl:text> </xsl:text>
          <xsl:value-of select="mediaobject/imageobject/imagedata/attribute::fileref"/>
        </xsl:message>
      </xsl:when>
      <xsl:when test="not(mediaobject/textobject[@role='TeX'])">
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
            <xsl:text>formulae/</xsl:text>
            <xsl:value-of select="@id"/>
            <xsl:text>.tex</xsl:text>
          </xsl:with-param>
          <xsl:with-param name="formula">
            <xsl:value-of select="mediaobject/textobject[@role='TeX']"/>
          </xsl:with-param>
        </xsl:call-template>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:for-each>
</xsl:template>

</xsl:stylesheet>
