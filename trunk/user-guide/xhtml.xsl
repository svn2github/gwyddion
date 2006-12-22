<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns="http://www.w3.org/1999/xhtml"
                version="1.0"
                exclude-result-prefixes="#default">

<xsl:import href="http://docbook.sourceforge.net/release/xsl/current/xhtml/chunk.xsl"/>

  <xsl:param name="chunker.output.method" select="'xml'"/>
  <!-- MSIE, dillo do not like xml decl -->
  <xsl:param name="chunker.output.omit-xml-declaration" select="'yes'"/>
  <xsl:param name="chunker.output.encoding" select="'utf-8'"/>
  <xsl:param name="toc.section.depth" select="1"/>
  <xsl:param name="use.id.as.filename" select="1"/>
  <xsl:param name="phrase.propagates.style" select="1"/>
  <xsl:param name="header.rule" select="0"/>
  <xsl:param name="footer.rule" select="0"/>
  <xsl:param name="html.stylesheet.type">text/css</xsl:param>
  <xsl:param name="html.stylesheet">user-guide.css</xsl:param>
  <xsl:param name="base.dir">xhtml/</xsl:param>
  <!-- Unfortunately it seems I cannot just override book -->
  <xsl:param name="generate.toc">
  appendix  toc,title
  article/appendix  nop
  article   toc,title
  book      toc,title
  chapter   toc,title
  part      toc,title
  preface   toc,title
  qandadiv  toc
  qandaset  toc
  reference toc,title
  sect1     toc
  sect2     toc
  sect3     toc
  sect4     toc
  sect5     toc
  section   toc
  set       toc,title
  </xsl:param>

  <xsl:template name="book.titlepage.before.recto">
    <a href="http://gwyddion.net/" title="Gwyddion web site">
      <img style="float:right;border:none;" src="stacked3.png"/>
    </a>
  </xsl:template>

</xsl:stylesheet>
