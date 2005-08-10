PLAINTEX = tex
CONVERT = convert
DVIPS = dvips
GS = gs

FORMULAS = $(wildcard eq-*.tex)
PNGS = $(subst .tex,.png,$(FORMULAS))
TEMPLATES = header.tex footer.tex defs.tex
DISTFILES = Makefile pngsize.c $(FORMULAS) $(TEMPLATES)

T = TEMP-

all: formulas

formulas: pngsize png-images

pngsize: pngsize.c

png-images: $(PNGS)

%.png: %.tex $(TEMPLATES)
	cat defs.tex header.tex $< footer.tex >$(T)$<
	$(TEX) --jobname $(T)$* --interaction=batchmode $(T)$<
	dvips -q -E -o $(T)$*.ps $(T)$*.dvi
	. $(T)$*.bbox; sed -ri "s/^(%%BoundingBox:).*/\\1 $$x0 $$y0 $$x1 $$y1/" $(T)$*.ps
	$(GS) -sPAPERSIZE=letter -dQUIET -dEPSCrop -r360 -sDEVICE=pnggray -dNOPAUSE -dBATCH -sOutputFile=$(T)$*.png $(T)$*.ps
	@{ \
	    eval $$(./pngsize <$(T)$*.png); \
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
	-rm -f eq-*.png pngsize $(T)* *~

dist:
	mkdir ../$(DISTNAME)/formulas
	cp Makefile $(FORMULAS) $(TEMPLATES) ../$(DISTNAME)/formulas

.PHONY: clean dist formulas all
