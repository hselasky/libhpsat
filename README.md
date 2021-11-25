# LibHPSat
This repository contains a library for solving equations based on boolean
algebra.

Unlike other SAT libraries, this library tries to derive a solution
by equation instead of trial and error. Further the library has been
optimised to handle large amounts of variables.

This library builds on the following boolean relationship: <br>

A XOR B XOR (A AND B) = (A OR B)

<br>

A ^ B ^ (A & B) = (A | B)

## How to build and install under FreeBSD
<pre>
make all install PREFIX=/usr/local
</pre>

## How to solve an equation given by a CNF file
<pre>
make -C apps/hpsolve all install PREFIX=/usr/local

cat test.cnf | hpsolve
</pre>

--HPS
