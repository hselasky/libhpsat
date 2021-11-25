/*-
 * Copyright (c) 2019-2023 Hans Petter Selasky
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

#include <hpMsat.h>

static hpMsat_var_t vm;
static int8_t *psol;

#define	HPMSAT_CNF_VAR_MAX (HPMSAT_VAR_MAX / 2)

static ssize_t
hpMsat_read_value(std::string &line, size_t &offset)
{
	bool sign = 0;
	ssize_t value = 0;

	while (line[offset] != 0) {
		if (isdigit(line[offset])) {
			value *= 10;
			value += line[offset] - '0';
			offset++;
		} else if (line[offset] == '-') {
			sign = 1;
			offset++;
		} else {
			break;
		}
	}
	return (sign ? -value : value);
}

static void
hpMsat_skip_space(std::string &line, size_t &offset)
{
	while (line[offset] == ' ' || line[offset] == '\t')
		offset++;
}

static int
hpMsat_loadcnf(std::istream &in, EQ_HEAD_t *phead, hpMsat_var_t *plimit)
{
	std::string line;
	ssize_t nexpr = 0;
	size_t offset;
	ssize_t v_max = 0;
	ssize_t v_limit = 0;

	while (getline(in, line)) {
		if (line[0] == 'c') {
			std::cerr << line << "\n";
			continue;
		}
		if (line[0] == 'p') {
			if (line[1] != ' ' &&
			    line[2] != 'c' &&
			    line[3] != 'n' &&
			    line[4] != 'f' &&
			    line[5] != ' ')
				return (EINVAL);

			offset = 6;
			hpMsat_skip_space(line, offset);
			v_max = hpMsat_read_value(line, offset);
			hpMsat_skip_space(line, offset);
			nexpr = hpMsat_read_value(line, offset);
			hpMsat_skip_space(line, offset);

			if (isdigit(line[offset])) {
				v_limit = hpMsat_read_value(line, offset) + 1;
				if (v_limit < 1 || v_limit > v_max)
					return (EINVAL);
			} else {
				v_limit = v_max + 1;
			}
			break;
		}
	}
	if (v_max <= 0 || nexpr <= 0 || v_max >= HPMSAT_CNF_VAR_MAX)
		return (EINVAL);

	fprintf(stderr, "c Variables = %zd\n", v_max);
	fprintf(stderr, "c Limit = %zd\n", v_limit);
	fprintf(stderr, "c Expressions = %zd\n", nexpr);

	hpMsat_logic_init(v_max + 1);

	for (ssize_t x = 0; x != nexpr; x++) {
		V expr;
next_line:
		if (!getline(in, line))
			goto error;

		if (line[0] == 'c')
			goto next_line;

		offset = 0;
		while (1) {
			hpMsat_skip_space(line, offset);

			if (line[offset] == '-' || isdigit(line[offset])) {
				ssize_t v = hpMsat_read_value(line, offset);
				if (v == 0)
					break;
				if (v < -v_max || v > v_max)
					goto error;

				V temp;
				if (v < 0)
					temp = V(-v, -1);
				else
					temp = V(v, 1);

				if (expr.var == 0)
					expr = temp;
				else
					expr |= temp;
			} else {
				goto next_line;
			}
		}

		expr.set_equal_to(1);
	}
	hpMsat_logic_concat(phead);
	hpMsat_logic_uninit();

	*plimit = v_max + 1;

	return (0);
error:
	return (EINVAL);
}

static void
usage(void)
{
	fprintf(stderr, "Usage: cat xxx.cnf | hpMsat [-h]\n");
}

static void
hpMsat_print_solution(int8_t *psol)
{
	printf("s SATISFIABLE\n" "v ");

	for (hpMsat_var_t x = 1; x < vm; x++) {
		printf("%zd (%d) ", psol[x] ? x : - x, psol[x]);
		if (x && (x % 16) == 0)
			printf("\nv ");
	}
	printf("0\n");
}

int
main(int argc, char **argv)
{
	int c;

	signal(SIGPIPE, SIG_IGN);

	while ((c = getopt(argc, argv, "h")) != -1) {
		switch (c) {
		default:
			usage();
			return (0);
		}
	}

	EQ_HEAD_t head;

	TAILQ_INIT(&head);

	if (hpMsat_loadcnf(std::cin, &head, &vm) != 0) {
		fprintf(stderr, "Failed to load CNF\n");
		return (1);
	}

	int8_t *psol = 0;

	hpMsat_var_t num = vm;

	if (hpMsat_solve(&head, &psol, num)) {
		printf("UNSATISFIABLE\n");
		goto skip;
	}
	hpMsat_print_solution(psol);
skip:
	hpMsat_free(&head);

	delete [] psol;

	return (0);
}
