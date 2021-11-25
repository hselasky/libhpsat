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

static void
hprsat_skip_space(std::string &line, size_t &offset)
{
	while (line[offset] == ' ' ||
	       line[offset] == '\t' ||
	       line[offset] == '\r')
		offset++;
}

static bool
hprsat_parse_add(std::string &line, size_t &offset, ADD &output);

static bool
hprsat_parse_mul(std::string &line, size_t &offset, MUL &output)
{
	bool haveParens = (line[offset] == '(');

	if (haveParens) {
		offset++;
		hprsat_skip_space(line, offset);
	}

	while (true) {
		if (haveParens) {
			if (line[offset] == ')') {
				offset++;
				hprsat_skip_space(line, offset);
				return (false);
			}
		}
		if (line[offset] == 'v') {
			offset++;
			MUL var(1.0, hprsat_read_size_value(line, offset));
			hprsat_skip_space(line, offset);
			output *= var;
		} else if ((line[offset] >= '0' && line[offset] <= '9') ||
			   (line[offset] == '-' &&
			    line[offset+1] >= '0' && line[offset+1] <= '9')) {
			MUL var(hprsat_read_double_value(line, offset));
			hprsat_skip_space(line, offset);
			output *= var;
		} else if (line[offset] == 's' &&
			   line[offset+1] == 'q' &&
			   line[offset+2] == 'r' &&
			   line[offset+3] == 't' &&
			   line[offset+4] == '(') {
			MUL var;
			ADD temp;

			offset += 4;
			if (hprsat_parse_add(line, offset, temp))
				return (true);
			temp.dup()->insert_tail(&var.ahead);
			output *= var;
		} else if (line[offset] == 'c' &&
			   line[offset+1] == 'o' &&
			   line[offset+2] == 's' &&
			   line[offset+3] == '(') {
			double phase;
			MUL var;
			ADD temp;

			offset += 4;
			phase = hprsat_read_double_value(line, offset);
			hprsat_skip_space(line, offset);
			if (line[offset] != ')')
				return (true);
			offset++;
			hprsat_skip_space(line, offset);

			phase -= floor(phase);

			temp = hprsat_cos_32(phase * (1ULL << 32));
			temp *= temp;
			temp.dup()->insert_tail(&var.ahead);

			output *= var;
		} else if (line[offset] == 's' &&
			   line[offset+1] == 'i' &&
			   line[offset+2] == 'n' &&
			   line[offset+3] == '(') {
			double phase;
			MUL var;
			ADD temp;

			offset += 4;
			phase = hprsat_read_double_value(line, offset);
			hprsat_skip_space(line, offset);
			if (line[offset] != ')')
				return (true);
			offset++;
			hprsat_skip_space(line, offset);

			phase -= floor(phase);

			temp = hprsat_sin_32(phase * (1ULL << 32));
			temp *= temp;
			temp.dup()->insert_tail(&var.ahead);

			output *= var;
		} else if (line[offset] == '*') {
			offset++;
			hprsat_skip_space(line, offset);
		} else {
			return (haveParens);
		}
	}
}

static bool
hprsat_parse_add(std::string &line, size_t &offset, ADD &output)
{
	hprsat_skip_space(line, offset);

	bool haveParens = (line[offset] == '(');
	bool sign = false;

	if (haveParens) {
		offset++;
		hprsat_skip_space(line, offset);
	}

	while (true) {
		MUL temp(sign ? -1.0 : 1.0);

		if (hprsat_parse_mul(line, offset, temp))
			return (true);

		temp.dup()->insert_tail(&output.head);

		if (haveParens) {
			if (line[offset] == ')') {
				offset++;
				hprsat_skip_space(line, offset);
				return (false);
			}
		}
		if (line[offset] == '+') {
			sign = false;
			offset++;
			hprsat_skip_space(line, offset);
		} else if (line[offset] == '-') {
			sign = true;
			offset++;
			hprsat_skip_space(line, offset);
		} else if (line[offset] == '\0') {
			return (haveParens);
		} else {
			return (true);
		}
	}
}

int
hprsat_parse(std::istream &in, ADD_HEAD_t *phead, hprsat_var_t *pmax, bool verbose)
{
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
		}
	
		ADD temp;

		offset = 0;

		if (hprsat_parse_add(line, offset, temp)) {
			if (verbose) {
				std::cerr << "# Error parsing line " << lineno << ": '" <<
				    line << "' at offset " << offset << "\n";
			}
			hprsat_free(phead);
			return (EINVAL);
		}

		temp.sort().dup()->insert_tail(phead);
		nexpr++;
	}

	*pmax = hprsat_maxvar(phead) + 1;

	if (verbose) {
		std::cerr << "# Variables = " << *pmax << "\n";
		std::cerr << "# Expressions = " << nexpr << "\n";
	}
	return (0);	/* success */
}
