SHELL = /bin/bash
CFLAGS = -g -Wall -std=c99

all: calculate

clean:
	rm -rf slides-png links calculate slides.{pdf,aux,toc,log,nav,out,snm}

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
	for i in `seq 0 10` ; do cp test-`printf '%04d' $$i`-plot.png test-`printf %04d $$((21-i))`-plot.png ; done
	avconv -y -r 12 -i test-%04d-plot.png -r 24 -qscale 4 -vcodec mpeg4 test.avi

slides.pdf: slides.tex
	pdflatex slides.tex

slides: slides.pdf
	evince -f slides.pdf

slides-png: slides.pdf
	rm -rf slides-png
	mkdir -p slides-png
	gs -sDEVICE=pngalpha -sOutputFile=slides-png/slide-%04d.png -r144 -dBATCH -dNOPAUSE slides.pdf

animate: slides-png
	rm -rf links
	./animate.pl
	avconv -y -r 12 -i links/link-%06d.png -r 24 -qscale 4 -vcodec mpeg4 test.avi

