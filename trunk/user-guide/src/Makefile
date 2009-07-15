GSL_CFLAGS := $(shell pkg-config --cflags gsl)
GSL_LIBS := $(shell pkg-config --libs gsl)
COMMON_CFLAGS = -Wall -W -O2 -g

OBJECTS = daubechies.o scaling-func.o wavelet-plot.o
DAUBECHIES = $(patsubst %,wavelet-daubechies%,4 6 12 20) wavelet-haar
WAVELET_GRAPHS = $(patsubst %,%.svg,$(DAUBECHIES))
WAVELET_DATA = $(patsubst %,%-wav.dat,$(DAUBECHIES)) $(patsubst %,%-fft.dat,$(DAUBECHIES))
GRAPHS = $(WAVELET_GRAPHS)
DATA = $(WAVELET_DATA)
PROGRAMS = wavelet-plot

wavelet-graphs: $(WAVELET_GRAPHS)

wavelet-%.svg: wavelet-%-wav.dat wavelet-%-fft.dat wavelet.gpi
	echo 'name = "wavelet-$*"; load "wavelet.gpi"' | gnuplot -

wavelet-daubechies%-wav.dat: wavelet-plot
	./$< $* 8 >$@

wavelet-daubechies%-fft.dat: wavelet-plot
	./$< -$* 8 >$@

wavelet-haar-wav.dat: wavelet-plot
	./$< 2 8 >$@

wavelet-haar-fft.dat: wavelet-plot
	./$< -2 8 >$@

wavelet-plot: wavelet-plot.o scaling-func.o daubechies.o
	$(CC) $(GSL_LIBS) -o $@ $^

wavelet-plot.o: wavelet-plot.c wavelet.h
	$(CC) $(COMMON_CFLAGS) $(GSL_CFLAGS) -c -o $@ $<

scaling-func.o: scaling-func.c wavelet.h
	$(CC) $(COMMON_CFLAGS) $(GSL_CFLAGS) -c -o $@ $<

daubechies.o: daubechies.c wavelet.h
	$(CC) $(COMMON_CFLAGS) $(GSL_CFLAGS) -c -o $@ $<

clean:
	rm -f $(PROGRAMS) $(OBJECTS) $(DATA) $(GRAPHS)

.PHONY: clean
.PRECIOUS: $(WAVELET_DATA)
