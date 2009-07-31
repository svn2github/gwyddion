GSL_CFLAGS := $(shell pkg-config --cflags gsl)
GSL_LIBS := $(shell pkg-config --libs gsl)
GWY_CFLAGS := $(shell pkg-config --cflags gwyddion)
GWY_LIBS := $(shell pkg-config --libs gwyddion)
COMMON_CFLAGS = -Wall -W -O2 -g

OBJECTS = daubechies.o scaling-func.o wavelet-plot.o windowing.o randomly-rough.o grain-quantity-dump.o

DAUBECHIES = $(patsubst %,wavelet-daubechies%,4 6 12 20) wavelet-haar
WAVELET_GRAPHS = $(patsubst %,%.svg,$(DAUBECHIES))
WAVELET_DATA = $(patsubst %,%-wav.dat,$(DAUBECHIES)) $(patsubst %,%-fft.dat,$(DAUBECHIES))

WINDOW = $(patsubst %,window-%,None Hann Hamming Blackmann Lanzcos Welch Rect Nuttall Flat-top Kaiser-2.5)
WINDOW_GRAPHS = windowing-win.svg windowing-fft.svg
WINDOW_DATA_WIN = $(patsubst %,%-win.dat,$(WINDOW))
WINDOW_DATA_FFT = $(patsubst %,%-fft.dat,$(WINDOW))
WINDOW_DATA = $(WINDOW_DATA_WIN) $(WINDOW_DATA_FFT)

PROFILE = $(patsubst %,profile-interpolation-%,round-natural round-hires key-hires)
PROFILE_GRAPHS = profile-interpolation.svg
PROFILE_DATA = $(patsubst %,%.dat,$(PROFILE))

GAUSSIAN = $(patsubst %,gaussian-%,acf hhcf psdf)
GAUSSIAN_GRAPHS = $(patsubst %,%.svg,$(GAUSSIAN))
GAUSSIAN_DATA = $(patsubst %,%.dat,$(GAUSSIAN))

GRAINQ_DATA = grain-quantities.xml

# These things are cleaned!
GRAPHS = $(WAVELET_GRAPHS) $(WINDOW_GRAPHS) $(PROFILE_GRAPHS) $(GAUSSIAN_GRAPHS)
DATA = $(WAVELET_DATA) $(WINDOW_DATA) $(GAUSSIAN_DATA) $(GRAINQ_DATA) fftw-wisdom.dat
PROGRAMS = wavelet-plot windowing randomly-rough grain-quantity-dump

all: $(WAVELET_GRAPHS) $(WINDOW_GRAPHS) $(PROFILE_GRAPHS) $(GAUSSIAN_GRAPHS) $(GRAINQ_DATA)

wavelet-%.svg: wavelet-%-wav.dat wavelet-%-fft.dat wavelet.gpi style.gpi
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

windowing-win.svg: windowing-win.gpi style.gpi $(WINDOW_DATA_WIN)
	gnuplot $<

windowing-fft.svg: windowing-fft.gpi style.gpi $(WINDOW_DATA_FFT)
	gnuplot $<

window-%-win.dat: windowing
	./$< win $* >$@

window-%-fft.dat: windowing
	./$< fft $* >$@

windowing: windowing.o
	$(CC) $(GWY_LIBS) -o $@ $^

windowing.o: windowing.c
	$(CC) $(COMMON_CFLAGS) $(GWY_CFLAGS) -c -o $@ $<

profile-interpolation.svg: profile-interpolation.gpi style.gpi $(PROFILE_DATA)
	gnuplot $<

gaussian-%.svg: gaussian.gpi gaussian-%.gpi style.gpi gaussian-%.dat
	echo 'name = "$*"; load "gaussian.gpi"' | gnuplot -

gaussian-%.dat: randomly-rough
	./$< $* 600 >$@

randomly-rough: randomly-rough.o
	$(CC) $(GWY_LIBS) -o $@ $^

randomly-rough.o: randomly-rough.c
	$(CC) $(COMMON_CFLAGS) $(GWY_CFLAGS) -c -o $@ $<

grain-quantities.xml: grain-quantity-dump
	./$< >$@

grain-quantity-dump: grain-quantity-dump.o
	$(CC) $(GWY_LIBS) -o $@ $^

grain-quantity-dump.o: grain-quantity-dump.c
	$(CC) $(COMMON_CFLAGS) $(GWY_CFLAGS) -c -o $@ $<

clean:
	rm -f $(PROGRAMS) $(OBJECTS) $(DATA) $(GRAPHS)

.PHONY: clean
.PRECIOUS: $(WAVELET_DATA) $(WINDOW_DATA) $(GAUSSIAN_DATA)
