SHELL = /bin/bash
CFLAGS = -g -Wall -std=c99

all: calculate

clean:
	rm -rf slides-png links tmp calculate \
            slides.{pdf,aux,toc,log,nav,out,snm} Makefile.generated \
            label-integral{,-crop}.{pdf,aux,toc,log,nav,out,snm,png} \
            test.avi

calculate: calculate.c
	$(CC) $(CFLAGS) -o $@ $< -lgsl -lgslcblas -lm

video: calculate animate.pl slides-png label-integral.png
	rm -rf links Makefile.generated
	mkdir -p links tmp
	./animate.pl <slides.tex
	$(MAKE) -f Makefile.generated gen-frames
	avconv -y -r 24 -i links/frame-%06d.png -r 24 -qscale 4 -vcodec mpeg4 test.avi

slides.pdf: slides.tex
	pdflatex slides.tex

slides: slides.pdf
	evince -f slides.pdf

slides-png: slides.pdf
	rm -rf slides-png
	mkdir -p slides-png
	gs -sDEVICE=pngalpha -sOutputFile=slides-png/slide-%04d.png -r144 -dBATCH -dNOPAUSE slides.pdf

label-integral.pdf: label-integral.tex
	pdflatex $<

label-integral-crop.pdf: label-integral.pdf
	pdfcrop $<

label-integral.png: label-integral-crop.pdf
	convert -density 160 $< $@
