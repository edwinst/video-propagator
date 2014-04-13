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
	avconv -i PLOT-%04d.png -vcodec mpeg4 test.avi
	vlc --no-video-title test.avi
