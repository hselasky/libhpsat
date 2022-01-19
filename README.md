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

## Dependencies
<pre>
pkg install gmp
</pre>

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

### Example 4
Squaring circuit (v0,v1,v2,v3) ** 2 = 49
<pre>
cat << EOF | hpRsolve -v -c
(v0 + v1 * 2 + v2 * 4 + v3 * 8) * (v0 + v1 * 2 + v2 * 4 + v3 * 8) - 49
EOF
</pre>

### Example 5
Multiplying circuit (v0,v1,v2,v3) * (v4,v5,v6,v7) = 143
<pre>
cat << EOF | hpRsolve -v
(v0 + v1 * 2 + v2 * 4 + v3 * 8) * (v4 + v5 * 2 + v6 * 4 + v7 * 8) - 143
EOF
</pre>

### Example 6
Multiplying circuit (v0,v1,v2) * (v3,v4,v5) = 15
<pre>
cat << EOF | hpRsolve -v
v0*v3 - 1
v0*v4 + v1*v3 - 1
v0*v5 + v1*v4 + v2*v3 - 1
v1*v5 + v2*v4 - 1
v2*v5 - 0
EOF
</pre>

--HPS
