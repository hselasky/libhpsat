/*-
 * Copyright (c) 2019-2023 Hans Petter Selasky. All rights reserved.
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
#include <hpNsat.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>

static hpNsat_var_t vm;
static hpNsat_var_t vl;
static hpNsat_val_t *pmax;
static size_t nsol;
static int count;

static void
usage(void)
{
	fprintf(stderr, "Usage: cat xxx.cnf | hpsolve [-c] [-h]\n");
}

static bool
solve_callback(void *arg, hpNsat_val_t *psol)
{
	printf("s SATISFIABLE\n" "v ");

	for (hpNsat_var_t x = 1; x < vl; x++) {
		printf("%zd ", psol[x] ? x : - x);
		if (x > 1 && (x % 16) == 1)
			printf("\nv ");
	}
	printf("0\n");
	nsol++;
	return (count == 0);
}

int
main(int argc, char **argv)
{
	int c;
	int demux = 0;
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

	if (hpNsat_loadcnf(std::cin, &ahead, &vl, &vm, &pmax) != 0) {
		fprintf(stderr, "Failed to load CNF\n");
		return (1);
	}

	ANDMAP_HEAD_t xhead;
	TAILQ_INIT(&xhead);

	hpNsat_val_t *psol = new hpNsat_val_t [vm];
	memset(psol, 0, sizeof(psol[0]) * vm);

	if (hpNsat_solve(&ahead, &xhead, vm, pmax)) {
		printf("UNSATISFIABLE\n");
		goto skip;
	}

	hpNsat_solve_callback(TAILQ_FIRST(&xhead), psol, pmax, &solve_callback, 0);

	if (nsol == 0)
		printf("UNSATISFIABLE\n");
	else if (count != 0)
		printf("s SOLUTIONS %zu\n", nsol);
skip:
	hpNsat_free(&ahead);
	hpNsat_free(&xhead);

	delete [] psol;
	delete [] pmax;

	return (0);
}
