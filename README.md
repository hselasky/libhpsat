# LibHPSAT
This repository contains a library for working with binary logic and
satisfiability equations. The three most basic operations, AND, OR and XOR
are fully supported.

Unlike other SAT libraries, this library tries to derive a solution
by equation instead of trial and error. Also the library has been
optimised to handle large amounts of variables.

## How to build under FreeBSD
<pre>
make all install
</pre>

## How to build the hp3solve application
<pre>
make -C apps/hp3solve all install
</pre>

## Examples
### Example 1
<IMG SRC="https://raw.githubusercontent.com/hselasky/libhpsat/3sat/www/hpsat_generate_1_4.svg"></IMG>
<pre>
hpsat_generate -f 1 -b 4 -V | hp3solve -sH | \
	grep -v ^c | dot -Tsvg > hpsat_generate_1_4.svg
</pre>

### Example 2
<pre>
hpsat_generate -f 1 -b 4 -V | hp3solve -sc | \
	hpsat_generate -f 1 -b 4 -p
</pre>

--HPS
