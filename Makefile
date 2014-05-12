SHELL = /bin/bash
CFLAGS = -O2 -Wall -std=c99

# video resolution
SX = 1280
SY = 720

FRAME_RATE = 24
FRAME_DIV  = 1

DATA_DIR = tmp

all: calculate

clean: clean-tex
	rm -rf slides-png links $(DATA_DIR) calculate \
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
	./animate.pl --prefix $(DATA_DIR)/ --frame-rate $(FRAME_RATE) --frame-div $(FRAME_DIV) \
                     --plot-options '--terminal "pngcairo dashed size $(SX),$(SY)"' < slides.tex

$(DATA_DIR)/bessel-FUNCTION.dat: calculate
	mkdir -p $(DATA_DIR)
	./calculate --prefix $(DATA_DIR)/bessel- --bessel --m 1 --z0 0.1 --z1 10

labels: label-integral.png label-integral-smeared.png label-envelope.png label-integrand.png label-integrand-smeared.png \
        label-branch-cut-neg.png label-branch-cut-pos.png

video: calculate animate slides-png labels $(DATA_DIR)/bessel-FUNCTION.dat
	mkdir -p $(DATA_DIR)
	$(MAKE) -f Makefile.generated gen-frames
	avconv -y -r $(FRAME_RATE) -i links/frame-%06d.png -r $(FRAME_RATE) -vcodec libx264 -b:v 5000k -maxrate 5000k -bufsize 1000k -threads 0 test.mp4

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
