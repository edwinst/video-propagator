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
