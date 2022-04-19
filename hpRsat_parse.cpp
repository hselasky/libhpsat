/*-
 * Copyright (c) 2021-2022 Hans Petter Selasky. All rights reserved.
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

#include <math.h>

#include "hpRsat.h"

static size_t
hprsat_read_size_value(std::string &line, size_t &offset)
{
	ssize_t value = 0;

	while (line[offset] != 0) {
		if (isdigit(line[offset])) {
			value *= 10;
			value += line[offset] - '0';
			offset++;
		} else {
			break;
		}
	}
	return (value);
}

static double
hprsat_read_double_value(std::string &line, size_t &offset)
{
	double value = 0.0;
	double factor = 1.0;
	bool haveSign = (line[offset] == '-');
	bool decimal = false;

	if (haveSign)
		offset++;

	while (line[offset] != 0) {
		if (isdigit(line[offset])) {
			if (decimal)
				factor *= 10.0;
			value *= 10.0;
			value += line[offset] - '0';
			offset++;
		} else if (decimal == false && line[offset] == '.') {
			decimal = true;
			offset++;
		} else {
			break;
		}
	}
	return (value / factor);
}

static hprsat_val_t
hprsat_read_value(std::string &line, size_t &offset)
{
	hprsat_val_t value = 0;
	bool haveSign = (line[offset] == '-');

	if (haveSign)
		offset++;

	while (line[offset] != 0) {
		if (isdigit(line[offset])) {
			value *= 10;
			value += line[offset] - '0';
			offset++;
		} else {
			break;
		}
	}
	return (value);
}

static void
hprsat_skip_space(std::string &line, size_t &offset)
{
	while (line[offset] == ' ' ||
	       line[offset] == '\t' ||
	       line[offset] == '\r')
		offset++;
}

static bool
hprsat_test_and_skip(std::string &line, size_t &offset, char which)
{
	if (line[offset] == which) {
		offset++;
		hprsat_skip_space(line, offset);
		return (true);
	} else {
		return (false);
	}
}

static bool
hprsat_parse_add(std::string &line, size_t &offset, ADD &output, bool = false, char = ')');

static bool
hprsat_parse_single_mul(std::string &line, size_t &offset, ADD &output)
{
	if (line[offset] == '!' &&
	    line[offset + 1] == 'v' &&
	    (line[offset + 2] >= '0' && line[offset + 2] <= '9')) {
		offset += 2;
		output = !ADD(1, hprsat_read_size_value(line, offset));
		hprsat_skip_space(line, offset);
		return (false);
	} else if (line[offset] == 'v' &&
		   (line[offset + 1] >= '0' && line[offset + 1] <= '9')) {
		offset++;
		output = ADD(1, hprsat_read_size_value(line, offset));
		hprsat_skip_space(line, offset);
		return (false);
	} else if (line[offset] == 'x' &&
		   (line[offset + 1] >= '0' && line[offset + 1] <= '9')) {
		offset++;
		output = ADD(1, hprsat_read_size_value(line, offset), HPRSAT_PWR_UNIT);
		hprsat_skip_space(line, offset);
		return (false);
	} else if ((line[offset] >= '0' && line[offset] <= '9') ||
		   (line[offset] == '-' &&
		    line[offset+1] >= '0' && line[offset+1] <= '9')) {
		output = ADD(hprsat_read_value(line, offset));
		hprsat_skip_space(line, offset);
		return (false);
	} else if (line[offset] == 's' &&
		   line[offset+1] == 'q' &&
		   line[offset+2] == 'r' &&
		   line[offset+3] == 't' &&
		   line[offset+4] == '(') {
		ADD temp;

		offset += 4;
		if (hprsat_parse_add(line, offset, temp))
			return (true);
		output = temp.raisePower(HPRSAT_PWR_SQRT);
		return (false);
	} else if (line[offset] == 'c' &&
		   line[offset+1] == 'o' &&
		   line[offset+2] == 's' &&
		   line[offset+3] == '(') {
		double phase;

		offset += 4;
		phase = hprsat_read_double_value(line, offset);
		hprsat_skip_space(line, offset);
		if (line[offset] != ')')
			return (true);
		offset++;
		hprsat_skip_space(line, offset);

		phase -= floor(phase);

		output = hprsat_cos_32(phase * (1ULL << 32));
		return (false);
	} else if (line[offset+0] == 'v' &&
		   line[offset+1] == 'c' &&
		   line[offset+2] == 'o' &&
		   line[offset+3] == 's' &&
		   line[offset+4] == '(') {
		double phase;
		hprsat_var_t var;

		offset += 5;
		phase = hprsat_read_double_value(line, offset);
		hprsat_skip_space(line, offset);
		if (line[offset] != ',')
			return (true);
		offset++;
		hprsat_skip_space(line, offset);
		if (line[offset] != 'v')
			return (true);
		offset++;
		var = hprsat_read_size_value(line, offset);
		hprsat_skip_space(line, offset);
		if (line[offset] != ')')
			return (true);
		offset++;
		hprsat_skip_space(line, offset);

		phase -= floor(phase);
		output = hprsat_cos_32(phase * (1ULL << 32), 0, var);
		return (false);
	} else if (line[offset] == 's' &&
		   line[offset+1] == 'i' &&
		   line[offset+2] == 'n' &&
		   line[offset+3] == '(') {
		double phase;

		offset += 4;
		phase = hprsat_read_double_value(line, offset);
		hprsat_skip_space(line, offset);
		if (line[offset] != ')')
			return (true);
		offset++;
		hprsat_skip_space(line, offset);

		phase -= floor(phase);

		output = hprsat_sin_32(phase * (1ULL << 32));
		return (false);
	} else if (line[offset+0] == 'v' &&
		   line[offset+1] == 's' &&
		   line[offset+2] == 'i' &&
		   line[offset+3] == 'n' &&
		   line[offset+4] == '(') {
		double phase;
		hprsat_var_t var;

		offset += 5;
		phase = hprsat_read_double_value(line, offset);
		hprsat_skip_space(line, offset);
		if (line[offset] != ',')
			return (true);
		offset++;
		hprsat_skip_space(line, offset);
		if (line[offset] != 'v')
			return (true);
		offset++;
		var = hprsat_read_size_value(line, offset);
		hprsat_skip_space(line, offset);
		if (line[offset] != ')')
			return (true);
		offset++;
		hprsat_skip_space(line, offset);

		phase -= floor(phase);
		output = hprsat_sin_32(phase * (1ULL << 32), 0, var);
		return (false);
	} else if (line[offset] == 'p' &&
		   line[offset+1] == 'o' &&
		   line[offset+2] == 'w' &&
		   line[offset+3] == '(') {
		double power;
		ADD temp;

		offset += 3;
		if (hprsat_parse_add(line, offset, temp, false, ','))
			return (true);

		hprsat_skip_space(line, offset);
		power = hprsat_read_double_value(line, offset);
		if ((HPRSAT_PWR_UNIT * power) != floor(HPRSAT_PWR_UNIT * power))
			return (true);

		hprsat_skip_space(line, offset);
		if (line[offset] != ')')
			return (true);
		offset++;
		hprsat_skip_space(line, offset);

		output = temp.raisePower(floor(HPRSAT_PWR_UNIT * power));
		return (false);
	} else {
		return (true);
	}
}

/*
 * Parse everything until end, or between a pair of parenthesis.
 */
static bool
hprsat_parse_add(std::string &line, size_t &offset, ADD &output, bool haveAll, char termCh)
{
	bool haveParens;
	bool sign;
	char lastOp;
	ADD temp[2];

	haveParens = haveAll || hprsat_test_and_skip(line, offset, '(');
	sign = false;
	lastOp = '*';
	temp[0] = ADD(0);
	temp[1] = ADD(1);

	while (true) {
		if (hprsat_test_and_skip(line, offset, '+')) {
			sign = false;
			lastOp = '+';
		} else if (hprsat_test_and_skip(line, offset, '-')) {
			sign = true;
			lastOp = '-';
		}

		if (line[offset] == '(') {
			if (hprsat_parse_add(line, offset, temp[0]))
				return (true);
		} else {
			if (hprsat_parse_single_mul(line, offset, temp[0]))
				return (true);
		}

		/* Insert the sign, if any. */
		if (sign) {
			temp[0] *= ADD(-1);
			sign = false;
		}

		if (haveParens) {
			if ((haveAll ? (line[offset] == '\0') :
			     hprsat_test_and_skip(line, offset, termCh))) {
				if (lastOp == '*') {
					temp[1] *= temp[0];
					TAILQ_CONCAT(&output.head, &temp[1].head, entry);
				} else {
					TAILQ_CONCAT(&output.head, &temp[0].head, entry);
				}
				return (false);
			}
		}

		if (hprsat_test_and_skip(line, offset, '*')) {
			temp[1] *= temp[0];
			temp[0] = ADD(0);
			lastOp = '*';
		} else {
			if (lastOp == '*') {
				temp[1] *= temp[0];
				TAILQ_CONCAT(&output.head, &temp[1].head, entry);
				temp[0] = ADD(0);
				temp[1] = ADD(1);
			} else {
				TAILQ_CONCAT(&output.head, &temp[0].head, entry);
				temp[0] = ADD(0);
			}
			if (line[offset] == '\0')
				return (haveParens);
		}
	}
}

int
hprsat_parse(std::istream &in, ADD_HEAD_t *phead, hprsat_var_t *pmax, bool verbose)
{
	hprsat_val_t mod = 0;
	std::string line;
	ssize_t nexpr = 0;
	size_t lineno = 0;
	size_t offset;

	while (getline(in, line)) {
		lineno++;

		if (line[0] == '#') {
			if (verbose)
				std::cerr << line << "\n";
			continue;
		} else if (line[0] == '\0') {
			continue;
		} else if (line[0] == 'm' &&
			   line[1] == 'o' &&
			   line[2] == 'd') {
			if (mod != 0)
				goto error;
			offset = 3;
			hprsat_skip_space(line, offset);
			mod = hprsat_read_value(line, offset);
			if (mod <= 1)
				goto error;
			hprsat_set_global_modulus(mod);
			continue;
		}

		if (mod <= 1)
			goto error;
	
		ADD temp;

		offset = 0;

		hprsat_skip_space(line, offset);

		if (hprsat_parse_add(line, offset, temp, true))
			goto error;

		temp.sort().dup()->insert_tail(phead);
		nexpr++;
	}

	*pmax = hprsat_maxvar(phead) + 1;

	if (verbose) {
		std::cerr << "# Variables = " << *pmax << "\n";
		std::cerr << "# Expressions = " << nexpr << "\n";
	}
	return (0);	/* success */
error:
	if (verbose) {
		std::cerr << "# Error parsing line " << lineno << ": '" <<
		    line << "' at offset " << offset << " '" << line[offset] << "'\n";
	}
	hprsat_free(phead);
	return (EINVAL);
}
