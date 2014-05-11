SHELL = /bin/bash
CFLAGS = -g -Wall -std=c99

# video resolution
SX = 1280
SY = 720

all: calculate

clean: clean-tex
	rm -rf slides-png links tmp calculate \
               Makefile.generated \
               test.avi

clean-tex:
	rm -f slides.{pdf,aux,toc,log,nav,out,snm} \
              label-*.{pdf,aux,toc,log,nav,out,snm,png}

calculate: calculate.c
	$(CC) $(CFLAGS) -o $@ $< -lgsl -lgslcblas -lm

animate: animate.pl slides.tex
	rm -rf links Makefile.generated
	mkdir -p links
	./animate.pl --plot-options '--terminal "pngcairo dashed size $(SX),$(SY)"' <slides.tex

tmp/bessel-FUNCTION.dat: calculate
	mkdir -p tmp
	./calculate --prefix tmp/bessel- --bessel --m 1 --z0 0.1 --z1 10

labels: label-integral.png label-integral-smeared.png label-envelope.png label-integrand.png label-integrand-smeared.png \
        label-branch-cut-neg.png label-branch-cut-pos.png

video: calculate animate slides-png labels tmp/bessel-FUNCTION.dat
	mkdir -p tmp
	$(MAKE) -f Makefile.generated gen-frames
	avconv -y -r 24 -i links/frame-%06d.png -r 24 -vcodec libx264 -b:v 5000k -maxrate 5000k -bufsize 1000k -threads 0 test.mp4

slides.pdf: slides.tex CC_BY.png
	pdflatex slides.tex

slides: slides.pdf
	evince -f slides.pdf

slides-png: slides.pdf
	rm -rf slides-png
	mkdir -p slides-png
	gs -sDEVICE=pngalpha -sOutputFile=slides-png/slide-%04d.png -g$(SX)x$(SY) -dFitPage -dBATCH -dNOPAUSE slides.pdf

label-%.pdf: label-%.tex
	pdflatex $<

label-%-crop.pdf: label-%.pdf
	pdfcrop $<

label-%.png: label-%-crop.pdf
	convert -density 200 $< $@
