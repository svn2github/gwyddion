# Process this file with automake to produce Makefile.in
# $Id$

EXTRA_DIST = $(m4_files)

m4_files = \
	codeset.m4 \
	gettext.m4 \
	glibc21.m4 \
	glibc2.m4 \
	gtk-doc.m4 \
	gwy-math.m4 \
	iconv.m4 \
	intdiv0.m4 \
	intldir.m4 \
	intl.m4 \
	intlmacosx.m4 \
	intltool.m4 \
	intmax.m4 \
	inttypes_h.m4 \
	inttypes.m4 \
	inttypes-pri.m4 \
	isc-posix.m4 \
	lcmessage.m4 \
	lib-ld.m4 \
	lib-link.m4 \
	lib-prefix.m4 \
	libtool.m4 \
	lock.m4 \
	longdouble.m4 \
	longlong.m4 \
	nls.m4 \
	pkg.m4 \
	po.m4 \
	printf-posix.m4 \
	progtest.m4 \
	signed.m4 \
	size_max.m4 \
	stdint_h.m4 \
	uintmax_t.m4 \
	ulonglong.m4 \
	visibility.m4 \
	wchar_t.m4 \
	wint_t.m4 \
	xsize.m4

dist-hook: dist-check-m4

dist-check-m4:
	@f=`for x in *.m4 $(m4_files); do echo $$x; done | sort | uniq -u`; \
	if test "x$$f" != x; then \
		echo "Directory contents does not match distributed m4 file list." 1>&2; \
		echo "Mismatched files:" $$f 1>&2; \
		false; \
	fi
