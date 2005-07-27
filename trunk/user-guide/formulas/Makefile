PLAINTEX = tex
CONVERT = convert
IDENTIFY = identify
DVIPS = dvips
GS = gs

FORMULAS = $(wildcard eq-*.tex)
PNGS = $(subst .tex,.png,$(FORMULAS))
TEMPLATES = header.tex footer.tex defs.tex
DISTFILES = Makefile $(FORMULAS) $(TEMPLATES)

T = TEMP-

all: formulas

formulas: png-images

png-images: $(PNGS)

%.png: %.tex $(TEMPLATES)
	cat defs.tex header.tex $< footer.tex >$(T)$<
	$(TEX) --jobname $(T)$* --interaction=batchmode $(T)$<
	dvips -q -E -o $(T)$*.ps $(T)$*.dvi
	. $(T)$*.bbox; sed -ri "s/^(%%BoundingBox:).*/\\1 $$x0 $$y0 $$x1 $$y1/" $(T)$*.ps
	$(GS) -sPAPERSIZE=letter -dQUIET -dEPSCrop -r360 -sDEVICE=pnggray -dNOPAUSE -dBATCH -sOutputFile=$(T)$*.png $(T)$*.ps
	@{ \
	    eval $$($(IDENTIFY) $(T)$*.png \
	        | sed -e 's/.* PNG \([0-9]*\)x\([0-9]*\)\(+0+0\)* .*/w=\1 h=\2/'); \
	    if test $$h -gt 1600; then \
	        echo "*** Warning: Your gs doesn't understand -dEPSCrop!" >&2; \
	        . $(T)$*.bbox; \
	        eval $$(sed -n '/^\\.size=/{s/^\\//;s/pt$$//;p}' header.tex); \
	        w=$$((($$w*($$x1 - $$x0) + $$hsize/2)/$$hsize)); \
	        h=$$((($$h*($$y1 - $$y0) + $$vsize/2)/$$vsize)); \
	        $(CONVERT) -type Grayscale -crop $${w}x$$h -scale 38.2% $(T)$*.png $@; \
	    else \
	        $(CONVERT) -type Grayscale -scale 38.2% $(T)$*.png $@; \
	    fi; \
	}
	rm $(T)$*.*

clean:
	-rm -f eq-*.png $(T)* *~

dist:
	mkdir ../$(DISTNAME)/formulas
	cp Makefile $(FORMULAS) $(TEMPLATES) ../$(DISTNAME)/formulas

.PHONY: clean dist formulas all

# Various `identify' output formats
# FC4: eq-area-calculation.png PNG 512x82 512x82+0+0 PseudoClass 256c 5kb
# SuSE9.2: eq-area-calculation.png PNG 512x82+0+0 PseudoClass 256c 8-bit 4.5kb 0.000u 0:01
# Debian3.1: eq-area-calculation.png PNG 512x82 PseudoClass 256c 4kb 0.000u 0:01
