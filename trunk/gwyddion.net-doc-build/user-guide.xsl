<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:exsl="http://exslt.org/common"
                version="1.0"
                exclude-result-prefixes="exsl">

  <!-- import the chunked XSL stylesheet -->
  <xsl:import href="http://docbook.sourceforge.net/release/xsl/current/html/chunk.xsl"/>
  <!-- import gtk-doc's version comparison -->
  <xsl:include href="version-greater-or-equal.xsl"/>

  <!-- change some parameters -->
  <xsl:param name="toc.section.depth" select="1"/>
  <xsl:param name="generate.toc">
    book	toc
    chapter	toc
  </xsl:param>

  <xsl:param name="chunker.output.encoding" select="'utf-8'"/>
  <!-- for some reason, this gives less silly DOCTYPE -->
  <xsl:param name="chunker.output.method" select="'xml'"/>
  <!-- MSIE, dillo do not like xml decl -->
  <xsl:param name="chunker.output.omit-xml-declaration" select="'yes'"/>
  <xsl:param name="chunk.fast" select="1"/>
  <xsl:param name="chapter.autolabel" select="0"/>
  <xsl:param name="use.id.as.filename" select="1"/>

  <xsl:param name="chunker.output.indent" select="'no'"/>
  <!-- Note: gtkdoc-fixxref classifies files by extension, must use .html,
       then rename -->
  <xsl:param name="html.ext" select="'.html'"/>

  <!-- use index filtering (if available) -->
  <xsl:param name="index.on.role" select="1"/>

  <xsl:param name="shade.verbatim" select="0"/>

  <!-- ========================================================= -->
  <!-- template to create the index.sgml anchor index -->

  <xsl:template name="book.titlepage.separator"/>

  <xsl:template name="book.titlepage.before.recto">
    <a href="http://gwyddion.net/" title="Gwyddion web site">
      <img style="float:right;border:none;" src="stacked3.png"/>
    </a>
  </xsl:template>

  <xsl:template match="book|article">
    <xsl:variable name="tooldver">
      <xsl:call-template name="version-greater-or-equal">
        <xsl:with-param name="ver1" select="$VERSION" />
        <xsl:with-param name="ver2">1.36</xsl:with-param>
      </xsl:call-template>
    </xsl:variable>
    <xsl:if test="$tooldver = 0">
      <xsl:message terminate="yes">
FATAL-ERROR: You need the DocBook XSL Stylesheets version 1.36 or higher
to build the documentation.
Get a newer version at http://docbook.sourceforge.net/projects/xsl/
      </xsl:message>
    </xsl:if>
    <xsl:apply-imports/>

    <!-- generate the index.sgml href index
         this is necessary to use fixxref -->
    <xsl:call-template name="generate.index"/>
  </xsl:template>

  <xsl:template name="generate.index">
    <xsl:call-template name="write.text.chunk">
      <xsl:with-param name="filename" select="'index.sgml'"/>
      <xsl:with-param name="content">
        <!-- check all anchor and refentry elements -->
        <xsl:apply-templates select="//anchor|//refentry"
                             mode="generate.index.mode"/>
      </xsl:with-param>
      <xsl:with-param name="encoding" select="'utf-8'"/>
    </xsl:call-template>
  </xsl:template>

  <xsl:template match="*" mode="generate.index.mode">
    <xsl:if test="not(@href)">
      <xsl:text>&lt;ANCHOR id=&quot;</xsl:text>
      <xsl:value-of select="@id"/>
      <xsl:text>&quot; href=&quot;</xsl:text>
      <xsl:if test="$gtkdoc.bookname">
        <xsl:value-of select="$gtkdoc.bookname"/>
        <xsl:text>/</xsl:text>
      </xsl:if>
      <xsl:call-template name="href.target"/>
      <xsl:text>&quot;&gt;
</xsl:text>
    </xsl:if>
  </xsl:template>

  <!-- ========================================================= -->
  <!-- template to output gtkdoclink elements for the unknown targets -->

  <xsl:template match="link">
    <xsl:choose>
      <xsl:when test="id(@linkend)">
        <xsl:apply-imports/>
      </xsl:when>
      <xsl:otherwise>
        <GTKDOCLINK HREF="{@linkend}">
          <xsl:apply-templates/>
        </GTKDOCLINK>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- ========================================================= -->
  <!-- The global layout, this is a dummy structure replaced with content
       by another trasnform. -->

  <xsl:template name="chunk-element-content">
    <xsl:param name="prev"/>
    <xsl:param name="next"/>
    <xsl:param name="nav.context"/>
    <xsl:param name="content">
      <xsl:apply-imports/>
    </xsl:param>

    <html>
      <head>
      <title>
        <xsl:variable name="node" select="."/>
          <xsl:variable name="title">
          <xsl:apply-templates select="$node" mode="object.title.markup.textonly"/>
        </xsl:variable>
        <xsl:copy-of select="$title"/>
      </title>
      </head>
      <body>
        <xsl:call-template name="header.navigation">
          <xsl:with-param name="prev" select="$prev"/>
          <xsl:with-param name="next" select="$next"/>
          <xsl:with-param name="nav.context" select="$nav.context"/>
        </xsl:call-template>
        <xsl:copy-of select="$content"/>
      </body>
    </html>
  </xsl:template>

  <!-- ========================================================= -->
  <!-- Titlepage, make it quite similar to other pages -->

  <xsl:template match="title" mode="book.titlepage.recto.mode">
    <h1>
      <xsl:value-of select="."/>
    </h1>
  </xsl:template>

  <xsl:template match="title" mode="chapter.titlepage.recto.mode">
    <h1>
      <xsl:value-of select="."/>
    </h1>
  </xsl:template>

  <xsl:template name="book.titlepage">
    <xsl:variable name="recto.content">
      <xsl:call-template name="book.titlepage.before.recto"/>
      <xsl:call-template name="book.titlepage.recto"/>
    </xsl:variable>
    <xsl:variable name="recto.elements.count">
      <xsl:choose>
        <xsl:when test="function-available('exsl:node-set')"><xsl:value-of select="count(exsl:node-set($recto.content)/*)"/></xsl:when>
        <xsl:otherwise>1</xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <xsl:if test="(normalize-space($recto.content) != '') or ($recto.elements.count &gt; 0)">
      <xsl:copy-of select="$recto.content"/>
    </xsl:if>
  </xsl:template>

  <!-- ========================================================= -->
  <!-- Section title -->

  <xsl:template match="refnamediv">
    <xsl:call-template name="anchor"/>
    <h1>
      <xsl:choose>
        <xsl:when test="../refmeta/refentrytitle">
          <xsl:apply-templates select="../refmeta/refentrytitle"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:apply-templates select="refname[1]"/>
        </xsl:otherwise>
      </xsl:choose>
    </h1>
    <p>
      <xsl:apply-templates/>
    </p>
  </xsl:template>

  <xsl:template name="section.heading">
    <xsl:param name="section" select="."/>
    <xsl:param name="level" select="1"/>
    <xsl:param name="allow-anchors" select="1"/>
    <xsl:param name="title"/>
    <xsl:param name="class" select="'title'"/>

    <xsl:variable name="id">
      <xsl:choose>
        <!-- if title is in an *info wrapper, get the grandparent -->
        <xsl:when test="contains(local-name(..), 'info')">
          <xsl:call-template name="object.id">
            <xsl:with-param name="object" select="../.."/>
          </xsl:call-template>
        </xsl:when>
        <xsl:otherwise>
          <xsl:call-template name="object.id">
            <xsl:with-param name="object" select=".."/>
          </xsl:call-template>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>

    <!-- HTML H level is one higher than section level -->
    <xsl:variable name="hlevel">
      <xsl:choose>
        <!-- highest valid HTML H level is H6; so anything nested deeper
        than 5 levels down just becomes H6 -->
        <xsl:when test="$level &gt; 6">6</xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="$level"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <xsl:element name="h{$hlevel}" namespace="http://www.w3.org/1999/xhtml">
      <xsl:attribute name="class"><xsl:value-of select="$class"/></xsl:attribute>
      <xsl:if test="$allow-anchors != 0">
        <xsl:call-template name="anchor">
          <xsl:with-param name="node" select="$section"/>
          <xsl:with-param name="conditional" select="0"/>
        </xsl:call-template>
      </xsl:if>
      <xsl:copy-of select="$title"/>
    </xsl:element>
  </xsl:template>

  <!-- ========================================================= -->
  <!-- Navigation -->

  <xsl:template name="navigation.separator">
    <xsl:text>&#160;|&#160;</xsl:text>
  </xsl:template>

  <xsl:template name="header.navigation">
    <xsl:param name="prev" select="/foo"/>
    <xsl:param name="next" select="/foo"/>
    <xsl:variable name="home" select="/*[1]"/>
    <xsl:variable name="up" select="parent::*"/>
    <xsl:variable name="sections" select="./refsect1[@id]"/>

    <xsl:if test="$home != .">
      <div class="navigation"><table summary="Navigation"><tr>
        <td class="nav-basic">
          <xsl:if test="count($prev) > 0">
            <xsl:element name="a">
              <xsl:attribute name="accesskey">p</xsl:attribute>
              <xsl:attribute name="href">
                <xsl:call-template name="href.target">
                  <xsl:with-param name="object" select="$prev"/>
                </xsl:call-template>
              </xsl:attribute>
              <xsl:text>Prev</xsl:text>
            </xsl:element>
            <xsl:call-template name="navigation.separator"/>
          </xsl:if>

          <xsl:if test="count($up) > 0 and $up != $home">
            <xsl:element name="a">
              <xsl:attribute name="accesskey">u</xsl:attribute>
              <xsl:attribute name="href">
                <xsl:call-template name="href.target">
                  <xsl:with-param name="object" select="$up"/>
                </xsl:call-template>
              </xsl:attribute>
              <xsl:text>Up</xsl:text>
            </xsl:element>
            <xsl:call-template name="navigation.separator"/>
          </xsl:if>

          <xsl:element name="a">
            <xsl:attribute name="accesskey">h</xsl:attribute>
            <xsl:attribute name="href">
              <xsl:call-template name="href.target">
                <xsl:with-param name="object" select="$home"/>
              </xsl:call-template>
            </xsl:attribute>
            <xsl:text>Home</xsl:text>
          </xsl:element>

          <xsl:if test="count($next) > 0">
            <xsl:call-template name="navigation.separator"/>
            <xsl:element name="a">
              <xsl:attribute name="accesskey">n</xsl:attribute>
              <xsl:attribute name="href">
                <xsl:call-template name="href.target">
                  <xsl:with-param name="object" select="$next"/>
                </xsl:call-template>
              </xsl:attribute>
              <xsl:text>Next</xsl:text>
            </xsl:element>
          </xsl:if>
        </td>
      </tr></table></div>
    </xsl:if>
  </xsl:template>

  <!-- avoid creating multiple identical indices
       if the stylesheets don't support filtered indices
    -->
  <xsl:template match="index">
    <xsl:variable name="has-filtered-index">
      <xsl:call-template name="version-greater-or-equal">
        <xsl:with-param name="ver1" select="$VERSION" />
        <xsl:with-param name="ver2">1.66</xsl:with-param>
      </xsl:call-template>
    </xsl:variable>
    <xsl:if test="($has-filtered-index = 1) or (count(@role) = 0)">
      <xsl:apply-imports/>
    </xsl:if>
  </xsl:template>

  <xsl:template match="index" mode="toc">
    <xsl:variable name="has-filtered-index">
      <xsl:call-template name="version-greater-or-equal">
        <xsl:with-param name="ver1" select="$VERSION" />
        <xsl:with-param name="ver2">1.66</xsl:with-param>
      </xsl:call-template>
    </xsl:variable>
    <xsl:if test="($has-filtered-index = 1) or (count(@role) = 0)">
      <xsl:apply-imports/>
    </xsl:if>
  </xsl:template>

</xsl:stylesheet>
