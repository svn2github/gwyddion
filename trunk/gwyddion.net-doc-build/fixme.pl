#!/usr/bin/perl -w

# Define parameters according to HEAD/non-HEAD documentation
my ( $top, $head, $self );
$self = quotemeta shift @ARGV;
if (@ARGV and $ARGV[0] eq 'HEAD') {
    $top = '../../..';
    $head = ' (HEAD)';
}
else {
    $top = '../..';
    $head = '';
}

# Slurp stdin
local $/;
local $_ = <STDIN>;

# Close <col> tags.  XXX: Why the output isn't well-formed?
s#(<col\b[^/>]*)>#$1/>#sg;

# Fix relative paths to .php
s#(<a\s+(?:accesskey="."\s+)?href="(?:\.\./[^/"]+/)?[^/"]+)\.html((?:\#[-A-Za-z0-9_:.]*)?")#$1.php$2#sg;

# Fix absolute paths to WWW links
my $gtkwww = 'http://developer.gnome.org/doc/API/2.0';
my $gtkglextwww = 'http://gtkglext.sourceforge.net/reference';
my $cairowww = 'http://www.cairographics.org/manual';
my $gimpwww = 'http://developer.gimp.org/api/2.0';
my %docmap = (
    'atk'            => $gtkwww      . '/atk',
    'cairo'          => $cairowww,
    'gdk'            => $gtkwww      . '/gdk',
    'gdk-pixbuf'     => $gtkwww      . '/gdk-pixbuf',
    'glib'           => $gtkwww      . '/glib',
    'gobject'        => $gtkwww      . '/gobject',
    'gtk'            => $gtkwww      . '/gtk',
    'gtkglext'       => $gtkglextwww . '/gtkglext',
    'libgimp'        => $gimpwww     . '/libgimp',
    'libgimpbase'    => $gimpwww     . '/libgimpbase',
    'libgimpcolor'   => $gimpwww     . '/libgimpcolor',
    'libgimpmath'    => $gimpwww     . '/libgimpmath',
    'libgimpmodule'  => $gimpwww     . '/libgimpmodule',
    'libgimpthumb'   => $gimpwww     . '/libgimpthumb',
    'libgimpwidgets' => $gimpwww     . '/libgimpwidgets',
    'pango'          => $gtkwww      . '/pango',
);
my %unknowndoc;
s#(<a\s+href=")/.*?/([^"/]+)(/[^"/]+")#
    if (defined $docmap{$2}) { $1.$docmap{$2}.$3 }
    else { $unknowndoc{$2} = 1; $1.$gtkwww.$2.'/'.$3 }
#sge;
if (%unknowndoc) {
    my $x;
    for $x (keys %unknowndoc) { warn "$x documentation location is unknown." }
}

# Remove document name from links to self
s#(<a\s+href=")$self(\#[-A-Za-z0-9_:.]*")#$1$2#sg;
s#(<a\s+href=")$self(")#$1\#$2#sg;

# Remove leading whitespace from <pre class="synopsis">
s#(<pre class="synopsis">)( *\n)+#$1\n#sg;

# Remove leading whitespace from property list lines
sub r { my $x = shift( @_ ); $x =~ s#^\s+##mg; return $x; }
s#(<h2>Properties</h2>\s*<pre class="synopsis">)(.*?)(</pre>)#$1.r($2).$3#se;

# Extract title
m#^<title>(.*?)</title>#m;
my $title = $1 . $head;

# Replace dummy header
s#<head>.*?</head>#<?php
ini_set('include_path', '$top:' . ini_get('include_path'));
\$title = '$title';
include('_head.php');
?>#s;

# Fix main header to match title (for HEAD)
s#>[^>]+</h1>#>$title</h1>#;

# Replace opening
s#<body>#<?php include('_top.php'); ?>#;

# Replace closing
s#</body>#<?php include('_bot.php'); ?>#;

# Remove <html>
s#</?html>\s*##sg;

# Output
print;
