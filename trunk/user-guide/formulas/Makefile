FORMULAS = \
	autocorrelation-definition.tex \
	kurtosis-skewness.tex

PNGS = $(subst .tex,.png,$(FORMULAS))
TEMPLATES = header.tex footer.tex defs.tex
DISTFILES = Makefile $(FORMULAS) $(TEMPLATES)

TMPEXT = .TEMP

all: formulas

formulas: $(PNGS)

clean:
	-rm -f $(PNGS) *$(TMPEXT).* *~

%.png: %.tex $(TEMPLATES)
	cat defs.tex header.tex $< footer.tex >$*$(TMPEXT).tex
	tex $*$(TMPEXT).tex >$*$(TMPEXT).log2
	dvips -q -E $*$(TMPEXT).dvi
	. $*$(TMPEXT).bbox; sed -ri "s/^(%%BoundingBox:).*/\\1 $$x0 $$y0 $$x1 $$y1/" $*$(TMPEXT).ps
	gs -sPAPERSIZE=letter -dQUIET -dEPSCrop -r400 -sDEVICE=pnggray -dNOPAUSE -dBATCH -sOutputFile=$*$(TMPEXT).png $*$(TMPEXT).ps
	convert -type Grayscale -scale 30% $*$(TMPEXT).png $@
	rm $*$(TMPEXT).*
