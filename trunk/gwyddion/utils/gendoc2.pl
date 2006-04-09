#!/usr/bin/perl
# @(#) $Id$
use warnings;
use strict;
use HTML::Entities;
use POSIX qw(getcwd);

# This script transforms gtk-doc documentation to a form directly usable on
# gwyddion.net with left bar, etc.  I should rather learn DSSSL...
my $tidy = 'tidy -asxhtml -q';
my $unsafe_chars = "<>\"&";
my $base = $ENV{'HOME'} . '/Projects/Gwyddion/Web/documentation';
my $suffix;
if ( not defined $ARGV[0] ) { $suffix = ''; }
elsif ( $ARGV[0] eq 'CVS' ) { shift @ARGV; $suffix = "&nbsp;(CVS HEAD)"; }
elsif ( $ARGV[0] =~ /^\d+\.\d/ ) { $suffix = "&nbsp;(" . (shift @ARGV) . ")"; }
if ( defined $ARGV[0] ) { $base = $ARGV[0]; }
my $APIDOCS = "$base";
my $pkgname = "Gwyddion";

undef $/;

my $footer =
"
</div>
<?php include('../../../_leftmenu.php'); ?>
";

chdir "devel-docs" or die "Must be run from top-level $pkgname directory.";
foreach my $dir (glob "*") {
    next if !-d $dir;
    next if $dir eq "CVS";
    my $oldcwd = getcwd;
    chdir $dir;
    mkdir "$APIDOCS/$dir" unless -d "$APIDOCS/$dir";
    foreach my $f (glob "html/*.html") {
        print "$dir/$f\n";
        $_ = qx(sed -e 's:</*gtkdoc[^>]*>::gi' $f | $tidy 2>/dev/null);
        if ( not $_ ) { die "Output is empty, do you have tidy installed?"; }
        # Lowercase attributes
        s#((?:class|rel)=".*?")#\L$1\E#sg;
        # Remove <body> attributes
        s#<body[^>]*>#<body>#s;
        # Move id= attributes directly to elements
        s#(<[^/].*?)(>[^<]*)<a( id=".*?").*?>(.*?)</a>#$1$3$2$4#sg;
        # Remove spaces before colons
        s#>(&nbsp;| |�):<#>:<#sg;
        # Remove silly EOLs
        s#&\#13;\n+##sg;
        # Remove <div> attributes and whitespace before
        s#\s*<div\b[^>]*>##sg;
        # Remove whitespace before </div>
        s#\s*</div>##sg;
        # Remove whitespace before </a>
        s#\s*</a>#</a>#sg;
        # Piece together broken likes
        s#(<\w+)\s*\n\s*#$1 #sg;
        s#(<\w+)\s*\n\s*#$1 #sg;
        s#(<\w+)\s*\n\s*#$1 #sg;
        # Remove function name patenthesis spacing
        s#\s+\(\)#()#g;
        # Add "api-since" class to Since: version
        s#(Since:? [\d.]+)#<span class="api-since">$1</span>#sg;
        # Remove leading empty lines from preforematted text
        s#(<pre\b[^>]*>)\n+#$1\n#sg;
        # Remove <p> inside <td> (XXX: what about multi-para?)
        s#(<td .*?>\s*)<p>(.*?)</p>(\s*</td>)#$1$2$3#sg;
        # Replace images that have alt= (that is navigation) with alt text
        s#<img\s+src=".*? alt="(.*?)"\s+/>#$1#sg;
        s#\s+cellpadding=".*?"##sg;
        s#\s+cellspacing=".*?"##sg;
        s#<meta name="generator".*?>##sgi;
        s#<hr\s*/>\n##sgi;
        # Change .html links to .php, unless they are to local gtk-doc docs or
        # to external URIs
        s#href="((?!/|http:)[^"]*)\.html\b#href="$1.php#sg;
        # Add navigation, also convert various constructs that should be
        # really document titles to <h1>
        my $add_topnote = s#<table class="navigation" width="100%"\s*>\s*<tr>\s*<th valign="middle">\s*<p class="title">(.*?)</p>\s*</th>\s*</tr>\s*</table>\s*<hr\s*/>#<h1>$1</h1>#sg;
        s#<h2><span class="refentrytitle">(.*?)</span></h2>#<h1>$1</h1>#s;
        s#<h2 class="title"(.*?)</h2>#<h1$1</h1>#s;
        # Remove the who-knows-what-it's-good-for table from titles
        s#<table[^>]*>\s*<tr>\s*<td[^>]*>\s*(<h1>.*?</h1>)\s*(.*?)</td>.*?</table>#$1\n<p>$2</p>#si;
        # Add suffix to h1 tags
        if ( $suffix ) {
            s#</h1>#$suffix</h1>#;
            s#(<p class="title">.*?)</p>#$1$suffix</p>#;
        }
        # Change warnings from titles to normal bold paragraphs
        s#<h3 class="title">Warning</h3>\n#<p><b class="warning">Warning:</b></p>#sg;
        # Change navigation table to gwyddion.net style
        if ( !$add_topnote ) { s#(<table class="navigation".*?</table>)#<div class="topnote">$1</div>#s; }
        s#(.*)(<table class="navigation".*?</table>)#$1<div class="botnote">$2</div>#s;
        # FIXME: what this does?
        s#</td>\s*<td><a#&nbsp;<a#sg;
        # Left-center cells
        s#(<tr valign="middle">\s*<td)>#$1 align="left">#s;
        # Change <th> to <td>
        s#(</td>\s*)<th#$1<td#s;
        s#(</th>\s*<td)#$1 align="right"#s;
        s#(<td[^>]*) align="([^"]*)"#$1 style="text-align:$2"#sg;
        s#<th\b#<th#g;
        s#\bth>#td>#g;
        # Replace <head> completely, but grather and keep useful <link rel=...>
        my $links = '';
        foreach my $lnk ( 'home', 'next', 'previous', 'up' ) {
            if (m#<link rel="$lnk"\s+href="(.*?)"\s+title="(.*?)" />#) {
                $links .= "<link rel=\"$lnk\" href=\"$1\" title=\"$2\"/>\n";
            }
        }
        s#href="/usr/share/gtk-doc/html/#href="http://developer.gnome.org/doc/API/2.0/#g;
        m#<title>(.*?)</title>#;
        my $title = $1;
        s#<head>.*?</head>\n#<head><meta http-equiv="Content-Type" content="text/html; charset=UTF-8"/>\n<title>$title</title>\n<link rel="stylesheet" type="text/css" href="/main.css"/>\n<!--[if IE]>\n<style> \#LeftMenu { position: absolute; } </style>\n<![endif]-->\n<link rel="shortcut icon" type="image/x-icon" href="/favicon.ico"/>\n$links</head>#sg;
        # Replace <body> with main part of gwyddion.net page body
        s#(<body>\n)#$1<div id="Main">\n#sg;
        s#(</body>)#$footer$1#;
        if ( $add_topnote ) { s#(<div id="Main">\n)#$1<?php include('../../_topnote.php'); ?>\n#s; }
        # Change links extensions to .php and fix and paths
        $f =~ s/.*\///;
        $f =~ s/\.html$/.php/;
        open FH, ">$APIDOCS/$dir/$f" or die; print FH $_; close FH;
    }
    chdir $oldcwd;
}

# vim: set ts=4 sw=4 et :
