# Root-SAT library

This repository contains a library for working with logic and
satisfiability equations. All variables are binary. Operations
supported:

- Multiplication
- Addition
- Subtraction
- Square root
- Sinus
- Cosinus
- Fast Fourier Transform

Unlike other SAT libraries, this library tries to derive a solution
by equation instead of trial and error. Further the library has been
optimised to handle large amounts of variables.

## How to build under FreeBSD
<pre>
make all install
</pre>

## How to build the hpRsolve application
<pre>
make -C apps/hpRsolve all install
</pre>

## Examples

### Example 1
<pre>
cat << EOF | hpRsolve -v -c
v0 + v1 + sqrt(v2)
EOF
</pre>

### Example 2
<pre>
cat << EOF | hpRsolve -v -c
v0 * cos(0.125) - v1 * cos(0.125)
EOF
</pre>

### Example 3
<pre>
cat << EOF | hpRsolve -v -c
v0 * 3 + v1 * 5 + v2 * 7 + v3 * 9 - 12
EOF
</pre>

--HPS
