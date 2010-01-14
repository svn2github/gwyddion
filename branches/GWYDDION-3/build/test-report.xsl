<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<xsl:output omit-xml-declaration='yes'/>
<xsl:param name='program' select='"unknown"'/>
<xsl:param name='prefix' select='""'/>
<xsl:template match='/'>
    <p>
    Test program: <code><xsl:value-of select='$program'/></code>,
    Successes: <span class='success'><xsl:value-of select='count(//status[@result="success"])'/></span>,
    Failures: <span class='failed'><xsl:value-of select='count(//status[@result="failed"])'/></span>,
    <!-- FIXME: Must sum the times over runs.
    Time: <xsl:value-of select='duration'/> s.
    -->
    </p>
    <xsl:text>&#10;</xsl:text>
    <table>
    <xsl:text>&#10;</xsl:text>
    <thead><tr><th>Test Path</th><th>Time [s]</th><th>Result</th></tr></thead>
    <xsl:text>&#10;</xsl:text>
    <tbody>
    <xsl:text>&#10;</xsl:text>
    <xsl:for-each select='//testcase'>
        <xsl:if test='status'>
            <tr>
                <td><code><xsl:value-of select='substring-after(@path,$prefix)'/></code></td>
                <td><xsl:value-of select='duration'/></td>
                <xsl:apply-templates select='status'/>
            </tr>
            <xsl:text>&#10;</xsl:text>
        </xsl:if>
        <xsl:if test='error'>
            <tr>
                <td class='error' colspan='3'><pre><xsl:value-of select='error'/></pre></td>
            </tr>
            <xsl:text>&#10;</xsl:text>
        </xsl:if>
    </xsl:for-each>
    </tbody>
    <xsl:text>&#10;</xsl:text>
    </table>
    <xsl:text>&#10;</xsl:text>
</xsl:template>

<xsl:template match='status'>
    <xsl:element name='td'>
        <xsl:attribute name='class'>
            <xsl:value-of select='@result'/>
        </xsl:attribute>
        <xsl:value-of select='@result'/>
    </xsl:element>
</xsl:template>

</xsl:stylesheet>
