/*-
 * Copyright (c) 2019-2021 Hans Petter Selasky. All rights reserved.
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
#include <hp2sat.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>

static hpsat_var_t vm;
static hpsat_var_t vl;
static size_t nsol;

static void
usage(void)
{
	fprintf(stderr, "Usage: cat xxx.cnf | hpsolve [-c] [-h]\n");
}

static bool
solve_callback(void *arg, uint8_t *psol)
{
	printf("s SATISFIABLE\n" "v ");

	for (hpsat_var_t x = 1; x < vl; x++) {
		printf("%zd ", psol[x] ? x : - x);
		if (x && (x % 16) == 0)
			printf("\nv ");
	}
	printf("0\n");
	nsol++;
	return (true);
}

int
main(int argc, char **argv)
{
	int c;
	int demux = 0;
	int count = 0;
	int strip = 0;

	signal(SIGPIPE, SIG_IGN);

	while ((c = getopt(argc, argv, "ch")) != -1) {
		switch (c) {
		case 'c':
			count = 1;
			break;
		default:
			usage();
			return (0);
		}
	}

	ANDMAP_HEAD_t ahead;
	TAILQ_INIT(&ahead);

	if (hpsat_loadcnf(std::cin, &ahead, &vl, &vm) != 0) {
		fprintf(stderr, "Failed to load CNF\n");
		return (1);
	}

	ANDMAP_HEAD_t xhead;
	TAILQ_INIT(&xhead);

	uint8_t *psol = new uint8_t [vm];
	memset(psol, 0, sizeof(psol[0]) * vm);

	if (hpsat_solve(&ahead, &xhead, vm)) {
		printf("UNSATISFIABLE\n");
		goto skip;
	}

	if (count == 0) {
		if (hpsat_solve_first(&xhead, psol) == false) {
			printf("UNSATISFIABLE\n");
			goto skip;
		}
		solve_callback(0, psol);
	} else {
		hpsat_solve_callback(TAILQ_FIRST(&xhead), psol, &solve_callback, 0);

		printf("s SOLUTIONS %zu\n", nsol);
	}
skip:
	hpsat_free(&ahead);
	hpsat_free(&xhead);

	delete [] psol;

	return (0);
}
