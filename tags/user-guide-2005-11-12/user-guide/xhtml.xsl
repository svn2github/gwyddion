<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<xsl:import href="http://docbook.sourceforge.net/release/xsl/current/xhtml/chunk.xsl"/>

<xsl:param name="toc.section.depth" select="1"/>
<xsl:param name="use.id.as.filename" select="1"/>
<xsl:param name="phrase.propagates.style" select="1"/>
<xsl:param name="header.rule" select="0"/>
<xsl:param name="footer.rule" select="0"/>
<xsl:param name="html.stylesheet.type">text/css</xsl:param>
<xsl:param name="html.stylesheet">user-guide.css</xsl:param>
<xsl:param name="base.dir">xhtml/</xsl:param>

</xsl:stylesheet>
