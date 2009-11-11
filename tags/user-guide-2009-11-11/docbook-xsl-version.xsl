<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<xsl:import href="http://docbook.sourceforge.net/release/xsl/current/html/chunk.xsl"/>
<xsl:include href="version-greater-or-equal.xsl"/>
<xsl:template match='book'>
  <xsl:variable name="tooldver">
    <xsl:call-template name="version-greater-or-equal">
      <xsl:with-param name="ver1" select="$VERSION" />
      <xsl:with-param name="ver2">1.70</xsl:with-param>
    </xsl:call-template>
  </xsl:variable>
  <xsl:if test="$tooldver = 0">
    <xsl:message terminate="yes">Too old DocBook XSL Styles</xsl:message>
  </xsl:if>
</xsl:template>
</xsl:stylesheet>
