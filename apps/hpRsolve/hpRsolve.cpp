/*-
 * Copyright (c) 2019-2022 Hans Petter Selasky. All rights reserved.
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
#include <hpRsat.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>

static hprsat_var_t vm;
static size_t nsol;

static void
usage(void)
{
	fprintf(stderr, "Usage: cat xxx.txt | hpRsolve [-c] [-C] [-P] [-u <N>] [-v] [-H] [-h]\n");
}

static bool
solve_callback_hpr(void *arg, uint8_t *psol)
{
	printf("# SATISFIABLE\n");

	for (hprsat_var_t x = 0; x < vm; x++) {
		printf("v%zd=%d ", x, psol[x]);
		if (x && (x % 16) == 0)
			printf("\n");
	}
	printf("\n");
	nsol++;

	return (true);
}

static bool
solve_callback_cnf(void *arg, uint8_t *psol)
{
	printf("s SATISFIABLE\n" "v ");

	for (hprsat_var_t x = 1; x < vm; x++) {
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
	bool verbose = false;
	bool count = false;
	bool digraph = false;
	bool probability = false;
	bool cnf = false;
	int underiv = 0;

	signal(SIGPIPE, SIG_IGN);

	while ((c = getopt(argc, argv, "cChHPu:v")) != -1) {
		switch (c) {
		case 'c':
			count = true;
			break;
		case 'C':
			cnf = true;
			break;
		case 'H':
			digraph = true;
			break;
		case 'v':
			verbose = true;
			break;
		case 'u':
			underiv = atoi(optarg);
			break;
		case 'P':
			probability = true;
			break;
		default:
			usage();
			return (0);
		}
	}

	ADD_HEAD_t ahead;
	TAILQ_INIT(&ahead);

	ADD_HEAD_t xhead;
	TAILQ_INIT(&xhead);

	uint8_t *psol = 0;

	if (cnf) {
		if (hprsat_loadcnf(std::cin, &ahead, 0, &vm) != 0) {
			fprintf(stderr, "Failed to load CNF input\n");
				return (1);
		}
	} else {
		if (hprsat_parse(std::cin, &ahead, &vm, verbose) != 0) {
			fprintf(stderr, "Failed to load input\n");
			return (1);
		}
	}

	while (1) {
		if (hprsat_solve(&ahead, &xhead, &vm, probability)) {
			printf("# UNSATISFIABLE\n");
			goto skip;
		}
		if (!underiv--)
			break;

		hprsat_underiv(&ahead, &xhead);
	}

	psol = new uint8_t [vm];
	memset(psol, 0, sizeof(psol[0]) * vm);

	if (digraph) {
		hprsat_print_digraph(std::cout, &xhead);
	} else if (count == 0) {
		if (hprsat_solve_first(&xhead, psol, probability) == false) {
			printf("# UNSATISFIABLE\n");
			goto skip;
		}

		if (cnf)
			solve_callback_cnf(0, psol);
		else
			solve_callback_hpr(0, psol);
	} else {
		hprsat_solve_callback(TAILQ_FIRST(&xhead), psol,
		    cnf ? &solve_callback_cnf : &solve_callback_hpr, 0);

		printf("# SOLUTIONS %zu\n", nsol);
	}
skip:
	hprsat_free(&ahead);
	hprsat_free(&xhead);

	delete [] psol;

	return (0);
}
