CFLAGS = -g -Wall -std=c99

all: calculate

clean:
	rm -f calculate

calculate: calculate.c
	$(CC) $(CFLAGS) -o $@ $< -lgsl -lgslcblas -lm

plot: calculate
	./calculate >DATA
	gnuplot plot-function.gnuplot

ploti: calculate
	./calculate
	display PLOT-*.png

video:
	for i in `seq 0 10` ; do cp PLOT-`printf '%04d' $$i`.png PLOT-`printf %04d $$((21-i))`.png ; done
	avconv -y -r 8 -i PLOT-%04d.png -r 24 -vcodec mpeg4 test.avi

video2:
	for i in `seq 0 140` ; do cp test-`printf '%04d' $$i`-plot.png test-`printf %04d $$((281-i))`-plot.png ; done
	avconv -y -r 12 -i test-%04d-plot.png -r 24 -qscale 4 -vcodec mpeg4 test.avi

slides.pdf: slides.tex
	pdflatex slides.tex

slides: slides.pdf
	evince -f slides.pdf
