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

#include "hp3sat.h"

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

int
hpsat_loadcnf(std::istream &in, BITMAP_HEAD_t *phead, hpsat_var_t *plimit, hpsat_var_t *pmax)
{
	std::string line;
	ssize_t nexpr = 0;
	size_t offset;
	hpsat_var_t v_max = 0;
	hpsat_var_t v_limit = 0;
	hpsat_var_t v_temp = 1;

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

	for (size_t x = 0; x != (size_t)nexpr; x++) {
		hpsat_var_t temp;
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
				temp = hpsat_read_value(line, offset);
				if (temp == 0)
					break;
			} else {
				goto next_line;
			}

			if (temp < -v_max || temp > v_max)
				goto error;

			/* split long expressions */
			if (var.nvar == 4) {
				const size_t v = v_max + v_temp++;
				var ^= BITMAP(v, false);
				(new BITMAP(var))->sort().insert_tail(phead);
				var = BITMAP(v, false);
			}

			/* append the AND expression */
			if (temp < 0)
				var &= BITMAP(-temp, false);
			else
				var &= BITMAP(temp, true);
		}

		if (var.isOne())
			goto error;

		(new BITMAP(var))->sort().insert_tail(phead);
	}
	if (plimit)
		*plimit = v_limit;
	if (pmax)
		*pmax = v_max + v_temp;

	/* XXX variable zero is never used */
	(new BITMAP(0,false))->insert_tail(phead);

	return (0);
error:
	return (EINVAL);
}

void
hpsat_printcnf(std::ostream &out, BITMAP_HEAD_t *phead, hpsat_var_t v_limit, hpsat_var_t v_max)
{
	size_t num = 0;

	if (v_max == 0)
		v_max = 1;

	for (BITMAP *pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		if (pa->nvar == 0 || pa->contains(0))
			continue;
		if (v_max < pa->maxVar() + 1)
			v_max = pa->maxVar() + 1;
		if (pa->isXorConst()) {
			num += 1UL << (pa->nvar - 1);
		} else {
			const size_t max = 1UL << pa->nvar;
			for (size_t x = 0; x != max; x++)
				num += pa->peek(x);
		}
	}

	if (v_limit > 0 && v_limit < v_max)
		out << "p cnf " << (v_max - 1) << " " << num << " " << (v_limit - 1) << "\n";
	else
		out << "p cnf " << (v_max - 1) << " " << num << "\n";

	for (BITMAP *pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		if (pa->nvar == 0 || pa->contains(0))
			continue;

		BITMAP temp(*pa);

		if (temp.isXorConst())
			temp.downgrade();

		const size_t max = 1UL << temp.nvar;

		for (size_t x = 0; x != max; x++) {
			if (temp.peek(x) == 0)
				continue;
			for (size_t y = 0; y != temp.nvar; y++)
				out << (((x >> y) & 1) ?
				    -temp.pvar[y] : temp.pvar[y]) << " ";
			out << "0\n";
		}
	}
}
