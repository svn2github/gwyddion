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

# Fix absolute paths to WWW links
my $gtkwww = 'http://library.gnome.org/devel';
my $gtkglextwww = 'http://gtkglext.sourceforge.net/reference';
my $cairowww = 'http://www.cairographics.org/manual';
my $gimpwww = 'http://developer.gimp.org/api/2.0';
my %docmap = (
    'atk'            => $gtkwww      . '/atk'             . '/stable',
    'cairo'          => $cairowww,
    'gdk'            => $gtkwww      . '/gdk'             . '/stable',
    'gdk-pixbuf'     => $gtkwww      . '/gdk-pixbuf'      . '/stable',
    'glib'           => $gtkwww      . '/glib'            . '/stable',
    'gobject'        => $gtkwww      . '/gobject'         . '/stable',
    'gtk'            => $gtkwww      . '/gtk'             . '/stable',
    'gtkglext'       => $gtkglextwww . '/gtkglext',
    'libgimp'        => $gimpwww     . '/libgimp',
    'libgimpbase'    => $gimpwww     . '/libgimpbase',
    'libgimpcolor'   => $gimpwww     . '/libgimpcolor',
    'libgimpmath'    => $gimpwww     . '/libgimpmath',
    'libgimpmodule'  => $gimpwww     . '/libgimpmodule',
    'libgimpthumb'   => $gimpwww     . '/libgimpthumb',
    'libgimpwidgets' => $gimpwww     . '/libgimpwidgets',
    'pango'          => $gtkwww      . '/pango'           . '/stable',
);
# Safe relative references
my %localdocmap = (
    'libgwyddion'   => 1,
    'libgwyprocess' => 1,
    'libgwydraw'    => 1,
    'libgwydgets'   => 1,
    'libgwymodule'  => 1,
    'libgwyapp'     => 1,
);
my %unknowndoc;

s#(<a\s+href=")(/.*?/|\.\./)([^"/]+)(/[^"/]+")#
    if (defined $docmap{$3}) { $1.$docmap{$3}.$4 }
    elsif ($2 eq '../' and defined $localdocmap{$3}) { $1.$2.$3.$4 }
    else { $unknowndoc{$3} = 1; $1.$gtkwww.$3.'/stable/'.$4 }
#sge;

if (%unknowndoc) {
    my $x;
    for $x (keys %unknowndoc) { warn "$x documentation location is unknown." }
}

# Fix relative paths to .php
s#(<a\s+(?:class="\w+"\s+)?(?:accesskey="."\s+)?href="(?:\.\./[^/"]+/)?[^/"]+)\.html((?:\#[-A-Za-z0-9_:.]*)?")#$1.php$2#sg;

# Remove document name from links to self
s#(<a\s+(?:class="\w+"\s+)?href=")$self(\#[-A-Za-z0-9_:.]*")#$1$2#sg;
s#(<a\s+(?:class="\w+"\s+)?href=")$self(")#$1\#$2#sg;

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
