/*-
 * Copyright (c) 2023 Hans Petter Selasky
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>

#include <hpsat.h>

static hpsat_var_t vm;
static uint8_t *psol;
static bool first = true;
static size_t nsol;

static void
usage(void)
{
	fprintf(stderr, "Usage: cat xxx.cnf | hpsolve [-ch]\n");
}

static bool
callback(const uint8_t *psol, void *arg)
{
	printf("s SATISFIABLE\n" "v ");

	for (hpsat_var_t v = HPSAT_VAR_MIN; v < vm; v++) {
		const ssize_t x = v - HPSAT_VAR_MIN + 1;
		printf("%zd ", psol[v] ? x : - x);
		if (x && (x % 16) == 0)
			printf("\nv ");
	}
	printf("0\n");

	if (nsol != SIZE_MAX)
		nsol++;

	return (first);
}

int
main(int argc, char **argv)
{
	int c;

	signal(SIGPIPE, SIG_IGN);

	while ((c = getopt(argc, argv, "ch")) != -1) {
		switch (c) {
		case 'c':
			first = false;
			break;
		default:
			usage();
			return (0);
		}
	}

	EQ eq;

	if (eq.from_cnf(std::cin) != 0) {
		fprintf(stderr, "Failed to load CNF\n");
		return (1);
	}

	vm = eq.maxVar() + 1;

	uint8_t *psol = new uint8_t [vm];

	if (eq.solve(psol, vm, &callback, 0) == false) {
		if (first)
			printf("UNSATISFIABLE\n");
		else
			printf("s SOLUTIONS %zu\n", nsol);
	}

	delete [] psol;

	return (0);
}
