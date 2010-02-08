<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<xsl:output omit-xml-declaration='yes'/>
<xsl:param name='program' select='"unknown"'/>

<xsl:template match='/'>
<tr>
    <td><code><xsl:value-of select='$program'/></code></td>
    <td><span class='success'><xsl:value-of select='count(//status[@result="success"])'/></span></td>
    <td><span class='failed'><xsl:value-of select='count(//status[@result="failed"])'/></span></td>
    <td><xsl:value-of select='sum(//testcase/duration)'/></td>
    <td><a href="test-report.html">full report</a></td>
    <td><a href="coverage.html">coverage</a></td>
</tr>
</xsl:template>

</xsl:stylesheet>
