FORMULAS = \
	autocorrelation-definition.tex \
	kurtosis-skewness.tex

PNGS = $(subst .tex,.png,$(FORMULAS))
TEMPLATES = header.tex footer.tex defs.tex
DISTFILES = Makefile $(FORMULAS) $(TEMPLATES)

all: formulas

formulas: $(PNGS)

clean:
	-rm -f $(PNGS) *~

%.png: %.tex $(TEMPLATES)
	cat defs.tex header.tex $< footer.tex >$*.tmp.tex
	tex $*.tmp.tex
	dvips -E $*.tmp.dvi
	. $*.tmp.bbox; sed -ri "s/^(%%BoundingBox:).*/\\1 $$x0 $$y0 $$x1 $$y1/" $*.tmp.ps
	gs -sPAPERSIZE=letter -dQUIET -dEPSCrop -r400 -sDEVICE=pnggray -dNOPAUSE -dBATCH -sOutputFile=$*.tmp.png $*.tmp.ps
	convert -type Grayscale -scale 30% $*.tmp.png $@
	rm $*.tmp.*
