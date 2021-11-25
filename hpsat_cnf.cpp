/*-
 * Copyright (c) 2023 Hans Petter Selasky. All rights reserved.
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

#include "hpsat.h"

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
EQ :: from_cnf(std::istream &in)
{
	std::string line;
	ssize_t nexpr = 0;
	ssize_t v_max = 0;
	size_t offset;

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
			break;
		}
	}
	if (v_max <= 0 || nexpr <= 0)
		return (EINVAL);

	fprintf(stderr, "c Variables = %zu\n", v_max);
	fprintf(stderr, "c Expressions = %zu\n", nexpr);

	for (size_t x = 0; x != (size_t)nexpr; x++) {
		ssize_t temp;
		EQ var(HPSAT_VAR_ONE);
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

			/* append expression as AND */
			if (temp < 0)
				var &= EQ((hpsat_var_t)(-temp + HPSAT_VAR_MIN - 1));
			else
				var &= !EQ((hpsat_var_t)(temp + HPSAT_VAR_MIN - 1));
		}

		if (var == EQ(HPSAT_VAR_ONE))
			goto error;

		/* TODO: Optimise this OR */

		*this |= var;
	}
	return (0);
error:
	return (EINVAL);
}
