/*-
 * Copyright (c) 2021-2023 Hans Petter Selasky. All rights reserved.
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

#include "hpMsat.h"

static int
hpMsat_var_compare(const void *a, const void *b)
{
	const VAR *ma = (const VAR *)a;
	const VAR *mb = (const VAR *)b;
	return (ma->compare(*mb));
}

void
hpMsat_eq_sort(EQ &c, hpMsat_val_t mod)
{
	size_t x;
	size_t y;
	size_t z;

	/* get it sorted */
	mergesort(c.pvar, c.nvar, sizeof(c.pvar[0]), &hpMsat_var_compare);

	for (x = z = 0; x != c.nvar; ) {
		for (y = x + 1; y != c.nvar; y++) {
			if (c.pvar[x] != c.pvar[y])
				break;
			c.pvar[x].val += c.pvar[y].val;
		}
		if (mod != 0)
			c.pvar[x].val %= mod;
		if (c.pvar[x].val != 0)
			c.pvar[z++] = c.pvar[x];
		x = y;
	}
	c.nvar = z;
}

/* greatest common divisor, Euclid equation */
hpMsat_val_t
hpMsat_gcd(hpMsat_val_t a, hpMsat_val_t b)
{
	hpMsat_val_t an;
	hpMsat_val_t bn;

	while (b != 0) {
		an = b;
		bn = a % b;
		a = an;
		b = bn;
	}
	return (a);
}

void
hpMsat_eq_gcd(EQ &c)
{
	hpMsat_val_t g;

	if (c.nvar == 0)
		return;

	g = c.pvar[0].val;

	/* remove greatest common divisor, if any */
	for (size_t x = 1; x != c.nvar; x++) {
		g = hpMsat_gcd(g, c.pvar[x].val);
		if (g == -1 || g == 1)
			return;
	}

	if (g < 0)
		g = -g;
	if (g > 1) {
		for (size_t x = 0; x != c.nvar; x++)
			c.pvar[x].val /= g;
	}
}

void
hpMsat_eq_add(const EQ &a, const EQ &b, EQ &c, hpMsat_val_t fa, hpMsat_val_t fb)
{
	size_t x;
	size_t y;

	c.nvar = 0;
	delete [] c.pvar;
	c.pvar = new VAR [a.nvar + b.nvar];

	/* from sorted list to sorted list */
	for (x = y = 0; x != a.nvar && y != b.nvar; ) {
		if (a.pvar[x] > b.pvar[y]) {
			c.pvar[c.nvar] = b.pvar[y++];
			c.pvar[c.nvar].val *= fb;
			c.nvar += (c.pvar[c.nvar].val != 0);
		} else if (a.pvar[x] < b.pvar[y]) {
			c.pvar[c.nvar] = a.pvar[x++];
			c.pvar[c.nvar].val *= fa;
			c.nvar += (c.pvar[c.nvar].val != 0);
		} else {
			c.pvar[c.nvar].var = a.pvar[x].var;
			c.pvar[c.nvar].val = a.pvar[x].val * fa + b.pvar[y].val * fb;
			c.nvar += (c.pvar[c.nvar].val != 0);
			x++;
			y++;
		}
	}

	while (x != a.nvar) {
		c.pvar[c.nvar] = a.pvar[x++];
		c.pvar[c.nvar].val *= fa;
		c.nvar += (c.pvar[c.nvar].val != 0);
	}

	while (y != b.nvar) {
		c.pvar[c.nvar] = b.pvar[y++];
		c.pvar[c.nvar].val *= fb;
		c.nvar += (c.pvar[c.nvar].val != 0);
	}
}

size_t
EQ :: index_of_var(hpMsat_var_t v, bool exact) const
{
	hpMsat_var_t x;

	switch (nvar) {
	case 0:
		x = 0;
		goto done;
	case 1:
		x = (v > pvar[0].var ? 1 : 0);
		goto done;
	case 2:
		x = (v > pvar[1].var ? 2 :
		    (v > pvar[0].var ? 1 : 0));
		goto done;
	default:
		break;
	}

	for (x = 1; x < nvar; x *= 2) {
		if (pvar[x].var > v)
			break;
		else if (pvar[x].var == v)
			return (x);
	}

	x /= 2;

	for (size_t y = x / 2; y != 0; y /= 2) {
		if ((x + y) >= nvar)
			continue;
		if (pvar[x + y].var <= v)
			x += y;
	}
	x += (v > pvar[x].var);
 done:
	assert(exact == false || x == nvar || v == pvar[x].var);
	return (x);
}

void
EQ :: get_range(hpMsat_val_t &min, hpMsat_val_t &max) const
{
	min = max = 0;

	for (size_t i = 0; i != nvar; i++) {
		hpMsat_val_t val = pvar[i].val;

		if (pvar[i].var == 0) {
			min += val;
			max += val;
		} else {
			if (val < 0) {
				min += val;
			} else {
				max += val;
			}
		}
	}
}

EQ &
EQ :: mod(hpMsat_val_t val)
{
	hpMsat_eq_sort(*this, val);
	return (*this);
}

EQ &
EQ :: gcd()
{
	hpMsat_eq_gcd(*this);
	return (*this);
}

EQ &
EQ :: expand(hpMsat_var_t var, int8_t value)
{
	for (size_t x = 0; x != nvar; x++)
		pvar[x].expand(var, value);

	hpMsat_eq_sort(*this);
	return (*this);
}

hpMsat_var_t
hpMsat_max_var(const EQ_HEAD_t *phead)
{
	hpMsat_var_t var = 0;
	EQ *pa;

	TAILQ_FOREACH(pa, phead, entry) {
		hpMsat_var_t max = pa->maxVar();
		if (max > var)
			var = max;
	}
	return (var);
}

void
hpMsat_free(EQ_HEAD_t *phead)
{
	EQ *pa;

	while ((pa = TAILQ_FIRST(phead)))
		delete pa->remove(phead);
}
