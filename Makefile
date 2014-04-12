CFLAGS = -g -Wall -std=c99

all: calculate

clean:
	rm -f calculate

calculate: calculate.c
	$(CC) $(CFLAGS) -o $@ $< -lgsl -lgslcblas

plot: calculate
	./calculate >DATA
	gnuplot plot-function.gnuplot
