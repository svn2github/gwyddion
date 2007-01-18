<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

  <!-- Remove id123456 anchors -->
  <xsl:template match="a[@name]">
    <xsl:if test="not(starts-with(@name,'id'))">
      <xsl:copy>
        <xsl:apply-templates select="@*|node()"/>
      </xsl:copy>
    </xsl:if>
  </xsl:template>

  <!-- Change name to id -->
  <xsl:template match="@name">
    <xsl:attribute name="id">
      <xsl:value-of select="."/>
    </xsl:attribute>
  </xsl:template>

  <!-- Remove unwanted divs -->
  <xsl:template match="div">
    <xsl:choose>
      <xsl:when test="@id or @class='variablelist' or @class='navigation' or @class='informalequation'">
        <xsl:copy>
          <xsl:apply-templates select="node()|@class|@id"/>
        </xsl:copy>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates select="child::node()"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- Remove unwanted spans -->
  <xsl:template match="span">
    <xsl:choose>
      <xsl:when test="@id or @class='gtkdoc-since' or @class='math'">
        <xsl:copy>
          <xsl:apply-templates select="node()|@class|@id"/>
        </xsl:copy>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates select="child::node()"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- Identity transform -->
  <xsl:template match="@*|node()">
    <xsl:copy>
      <xsl:apply-templates select="@*|node()"/>
    </xsl:copy>
  </xsl:template>

</xsl:stylesheet>

