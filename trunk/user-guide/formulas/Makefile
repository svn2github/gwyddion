FORMULAS = $(wildcard eq-*.tex)
PNGS = $(subst .tex,.png,$(FORMULAS))
TEMPLATES = header.tex footer.tex defs.tex
DISTFILES = Makefile $(FORMULAS) $(TEMPLATES)

T = TEMP-

all: formulas

formulas: $(PNGS)

clean:
	-rm -f $(PNGS) $(T)* *~

%.png: %.tex $(TEMPLATES)
	cat defs.tex header.tex $< footer.tex >$(T)$<
	tex --jobname $(T)$* --interaction=batchmode $(T)$<
	dvips -q -E -o $(T)$*.ps $(T)$*.dvi
	. $(T)$*.bbox; sed -ri "s/^(%%BoundingBox:).*/\\1 $$x0 $$y0 $$x1 $$y1/" $(T)$*.ps
	gs -sPAPERSIZE=letter -dQUIET -dEPSCrop -r340 -sDEVICE=pnggray -dNOPAUSE -dBATCH -sOutputFile=$(T)$*.png $(T)$*.ps
	convert -type Grayscale -scale 38.2% $(T)$*.png $@
	rm $(T)$*.*
