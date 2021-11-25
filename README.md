# LibHPSAT
This repository contains a library for working with binary logic and
satisfiability equations. The three most basic operations, AND, OR and XOR
are fully supported.

Unlike other SAT libraries, this library tries to derive a solution
by equation instead of trial and error. Also the library has been
optimised to handle large amounts of variables.

## How to build under FreeBSD
make all install

## How to build the hpsolve application
make -C apps/hpsolve all install

--HPS
