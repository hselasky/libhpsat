/*-
 * Copyright (c) 2020-2021 Hans Petter Selasky. All rights reserved.
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

#include "hp2sat.h"

static ssize_t
hpsat_read_value(std::string &line, size_t &offset)
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
hpsat_skip_space(std::string &line, size_t &offset)
{
	while (line[offset] == ' ' || line[offset] == '\t')
		offset++;
}

static void
hpsat_build_final_and(ANDMAP_HEAD_t *phead, hpsat_var_t &nvar,
    hpsat_var_t a, hpsat_var_t b)
{
	hpsat_var_t d = nvar++;
	uint8_t mask = 0;

	a *= HPSAT_VAR_ST_MAX;
	b *= HPSAT_VAR_ST_MAX;
	d *= HPSAT_VAR_ST_MAX;

	if (a <= 0) {
		a = -a;
	} else {
		mask |= 1;
	}

	if (b <= 0) {
		b = -b;
	} else {
		mask |= 2;
	}

	for (uint8_t dv = 0; dv != 4; dv++) {
		bool func_a = ((dv & 1) == 1);
		bool func_b = ((dv & 2) == 2);
		bool func_c = ((dv ^ mask) == 3);

		if (func_c)
			(new ANDMAP(ANDMAP(a + func_a) & ANDMAP(b + func_b)))->insert_tail(phead);
	}
}

int
hpsat_loadcnf(std::istream &in, ANDMAP_HEAD_t *phead, hpsat_var_t *plimit, hpsat_var_t *pmax)
{
	std::string line;
	ssize_t nexpr = 0;
	size_t offset;
	hpsat_var_t v_max = 0;
	hpsat_var_t v_limit = 0;
	hpsat_var_t v_temp;

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
			hpsat_skip_space(line, offset);
			v_max = hpsat_read_value(line, offset);
			hpsat_skip_space(line, offset);
			nexpr = hpsat_read_value(line, offset);
			hpsat_skip_space(line, offset);

			if (isdigit(line[offset])) {
				v_limit = hpsat_read_value(line, offset) + 1;
				if (v_limit < 1 || v_limit > v_max)
					return (EINVAL);
			} else {
				v_limit = v_max + 1;
			}
			break;
		}
	}
	if (v_max <= 0 || nexpr <= 0)
		return (EINVAL);

	fprintf(stderr, "c Variables = %zu\n", v_max);
	fprintf(stderr, "c Limit = %zu\n", v_limit);
	fprintf(stderr, "c Expressions = %zu\n", nexpr);

	v_temp = v_max + 1;

	for (size_t x = 0; x != (size_t)nexpr; x++) {
		hpsat_var_t temp[v_max];
		hpsat_var_t nvar = 0;
		hpsat_var_t v;
		BITMAP var(true);
next_line:
		if (!getline(in, line))
			goto error;

		if (line[0] == 'c')
			goto next_line;

		offset = 0;
		while (1) {
			hpsat_skip_space(line, offset);

			if (line[offset] == '-' || isdigit(line[offset])) {
				v = hpsat_read_value(line, offset);
				if (v == 0)
					break;
				if (nvar == v_max || v < -v_max || v > v_max)
					goto error;
				temp[nvar++] = v;
			} else {
				goto next_line;
			}
		}

#if 0
	retry:
#endif
		switch (nvar) {
		case 0:
			break;
		case 1:
			hpsat_build_final_and(phead, v_temp, temp[0], temp[0]);
			break;
		case 2:
			hpsat_build_final_and(phead, v_temp, temp[0], temp[1]);
			break;
		default:
#if 0
			for (hpsat_var_t off = 0; off < nvar; off += 2) {
				if (off + 1 < nvar) {
					v = v_temp++;
					hpsat_build_multi_and(phead, v_temp, temp[off + 0], temp[off + 1], v);
					temp[off / 2] = -v;
				} else {
					temp[off / 2] = temp[off];
				}
			}
			nvar = (nvar + 1) / 2;
			goto retry;
#else
			goto error;
#endif
		}
	}

	/* XXX variable zero is never used */
	hpsat_build_final_and(phead, v_temp, 0, 0);

	if (plimit)
		*plimit = v_limit;
	if (pmax)
		*pmax = v_temp;

	return (0);
error:
	return (EINVAL);
}
