/*-
 * Copyright (c) 2020-2023 Hans Petter Selasky. All rights reserved.
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

#include "hpNsat.h"

#define	HPNSAT_CNF_VAR_MAX (HPNSAT_VAR_MAX / 2)

static ssize_t
hpNsat_read_value(std::string &line, size_t &offset)
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
hpNsat_skip_space(std::string &line, size_t &offset)
{
	while (line[offset] == ' ' || line[offset] == '\t')
		offset++;
}

static void
hpNsat_binary_exclude(ANDMAP_HEAD_t *phead, hpNsat_var_t a, uint8_t aval, hpNsat_var_t v, hpNsat_val_t vval)
{
	if (a <= 0) {
		a = -a;
		aval = !aval;
	}

	/* exclude "aval" from solution of variable given by "a" */
	(new ANDMAP(ANDMAP(a, aval) & ANDMAP(v, vval)))->insert_tail(phead);
}

int
hpNsat_loadcnf(std::istream &in, ANDMAP_HEAD_t *phead, hpNsat_var_t *plimit, hpNsat_var_t *pmax, hpNsat_val_t **ppvmax)
{
	std::string line;
	ssize_t nexpr = 0;
	size_t offset;
	hpNsat_var_t v_max = 0;
	hpNsat_var_t v_limit = 0;
	hpNsat_var_t v_temp;
	hpNsat_val_t *pvmax = 0;

	*ppvmax = 0;

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
			hpNsat_skip_space(line, offset);
			v_max = hpNsat_read_value(line, offset);
			hpNsat_skip_space(line, offset);
			nexpr = hpNsat_read_value(line, offset);
			hpNsat_skip_space(line, offset);

			if (isdigit(line[offset])) {
				v_limit = hpNsat_read_value(line, offset) + 1;
				if (v_limit < 1 || v_limit > v_max)
					return (EINVAL);
			} else {
				v_limit = v_max + 1;
			}
			break;
		}
	}
	if (v_max <= 0 || nexpr <= 0 || v_max >= HPNSAT_CNF_VAR_MAX)
		return (EINVAL);

	fprintf(stderr, "c Variables = %zu\n", v_max);
	fprintf(stderr, "c Limit = %zu\n", v_limit);
	fprintf(stderr, "c Expressions = %zu\n", nexpr);

	v_temp = v_max + 1;
	*ppvmax = pvmax = new hpNsat_val_t [v_temp + nexpr];

	pvmax[0] = 0;
	for (hpNsat_var_t v = 1; v != v_temp; v++)
		pvmax[v] = 2;
	for (hpNsat_var_t v = v_temp; v != (v_temp + nexpr); v++)
		pvmax[v] = 0;

	for (size_t x = 0; x != (size_t)nexpr; x++) {
		hpNsat_var_t v;
next_line:
		if (!getline(in, line))
			goto error;

		if (line[0] == 'c')
			goto next_line;

		offset = 0;
		while (1) {
			hpNsat_skip_space(line, offset);

			if (line[offset] == '-' || isdigit(line[offset])) {
				v = hpNsat_read_value(line, offset);
				if (v == 0)
					break;
				if (v < -v_max || v > v_max)
					goto error;
				
				if (v_temp + x == HPNSAT_CNF_VAR_MAX)
					goto error;
				hpNsat_binary_exclude(phead, v, 0, v_temp + x, pvmax[v_temp + x]++);
			} else {
				goto next_line;
			}
		}
	}

	hpNsat_sort_or(phead);

	*plimit = v_limit;
	*pmax = v_temp + nexpr;

	return (0);
error:
	delete [] pvmax;
	*ppvmax = 0;
	return (EINVAL);
}
