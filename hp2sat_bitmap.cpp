/*-
 * Copyright (c) 2021 Hans Petter Selasky. All rights reserved.
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

uint8_t bitmap_true[1] = {1};
uint8_t bitmap_false[1] = {0};

ssize_t
hpsat_binsearch(const hpsat_var_t *ptr, size_t max, hpsat_var_t v)
{
	size_t x;

	switch (max) {
	case 0:
		goto failure;
	case 1:
		return ((ptr[0] == v) ? 0 : -1);
	case 2:
		return ((ptr[0] == v) ? 0 :
			(ptr[1] == v) ? 1 : -1);
	default:
		break;
	}

	for (x = 1; x < max; x *= 2) {
		if (ptr[x] > v)
			break;
		else if (ptr[x] == v)
			return (x);
	}

	x /= 2;

	for (size_t y = x / 2;; y /= 2) {
		if ((x + y) >= max)
			continue;
		if (ptr[x + y] < v) {
			if (y == 0)
				goto failure;
			else
				x += y;
		} else if (ptr[x + y] == v) {
			return (x + y);
		} else if (y == 0) {
			goto failure;
		}
	}
failure:
#ifdef DEBUG
	for (size_t y = 0; y != max; y++)
		assert(ptr[y] != v);
#endif
	return (-1);
}

BITMAP &
BITMAP :: xform(void)
{
	size_t max;

	assert(!isXorConst());

	switch (nvar) {
	case 0:
		break;
	case 1:
		table[0] ^= 2 * (table[0] & 1);
		break;
	case 2:
		table[0] ^= 2 * (table[0] & 5);
		table[0] ^= 4 * (table[0] & 3);
		break;
	case 3:
		table[0] ^= 2 * (table[0] & 0x55);
		table[0] ^= 4 * (table[0] & 0x33);
		table[0] ^= 16 * (table[0] & 0x0F);
		break;
	default:
		max = 1UL << (nvar - 3);

		for (size_t x = 0; x != max; x++) {
			table[x] ^= 2 * (table[x] & 0x55);
			table[x] ^= 4 * (table[x] & 0x33);
			table[x] ^= 16 * (table[x] & 0x0F);
		}

		for (size_t x = 1; x != max; x *= 2) {
			for (size_t y = 0; y != max; y += (2 * x)) {
				for (size_t z = 0; z != x; z++) {
					table[y + z + x] ^= table[y + z];
				}
			}
		}
		break;
	}
	return (*this);
}

BITMAP &
BITMAP :: downgrade(void)
{
	if (!isXorConst())
		return (*this);

	assert(nvar < 32);

	size_t size = 1UL << ((nvar < 3) ? 0 : (nvar - 3));
	bool toggle = isInverted();
	table = new uint8_t [size];
	memset(table, 0, size);

	if (toggle)
		set(0);
	for (size_t x = 0; x != nvar; x++)
		set(1UL << x);

	return (xform());
}

BITMAP &
BITMAP :: operator =(const BITMAP &other)
{
	if (this == &other)
		return (*this);
	if (!isXorConst())
		delete [] table;
	if (nvar != other.nvar) {
		nvar = other.nvar;
		delete [] pvar;
		pvar = new hpsat_var_t [nvar];
	}
	memcpy(pvar, other.pvar, sizeof(pvar[0]) * nvar);

	if (other.isXorConst()) {
		table = other.table;
	} else {
		size_t tsize = 1UL << ((nvar < 3) ? 0 : (nvar - 3));
		table = new uint8_t [tsize];
		memcpy(table, other.table, tsize);
	}
	return (*this);
}

BITMAP &
BITMAP :: operator ^=(const BITMAP &other)
{
	if (isXorConst() != other.isXorConst()) {
		if (other.isXorConst()) {
			*this ^= BITMAP(other).downgrade();
			return (*this);
		}
		downgrade();
	}
	if (isXorConst()) {
		/* XOR constant */
		if (table != other.table)
			table = bitmap_true;
		else
			table = bitmap_false;

		/* XOR variables */
		hpsat_var_t *pnew_var;

		size_t x;
		size_t y;
		size_t z;

		pnew_var = new hpsat_var_t [nvar + other.nvar];

		for (x = y = z = 0; x != other.nvar && y != nvar; ) {
			if (other.pvar[x] < pvar[y]) {
				pnew_var[z++] = other.pvar[x++];
			} else if (other.pvar[x] > pvar[y]) {
				pnew_var[z++] = pvar[y++];
			} else {
				x++;
				y++;
			}
		}

		while (x != other.nvar)
			pnew_var[z++] = other.pvar[x++];

		while (y != nvar)
			pnew_var[z++] = pvar[y++];

		delete [] pvar;
		nvar = z;
		pvar = pnew_var;
	} else {
		size_t mask;

		addVar(other.pvar, other.nvar, &mask);

		size_t size = 1UL << other.nvar;
		size_t x;
		size_t y;
		size_t z;

		for (x = z = 0; x != size; x++, z |= mask, z++) {
			if (other.peek(x)) {
				for (y = 0;; y |= ~mask, y++) {
					toggle((y & mask) | (z & ~mask));
					if ((y & mask) == mask)
						break;
				}
			}
		}
	}
	return (*this);
}

BITMAP &
BITMAP :: operator &=(const BITMAP &other)
{
	/* some optimisations */
	if (other.isOne()) {
		return (*this);
	} else if (isOne()) {
		*this = other;
		return (*this);
	}

	/* downgrade first */
	if (isXorConst())
		downgrade();
	if (other.isXorConst()) {
		*this &= BITMAP(other).downgrade();
	} else {
		size_t mask;

		addVar(other.pvar, other.nvar, &mask);

		size_t size = 1UL << other.nvar;
		size_t x;
		size_t y;
		size_t z;

		for (x = z = 0; x != size; x++, z |= mask, z++) {
			if (other.peek(x) == false) {
				for (y = 0;; y |= ~mask, y++) {
					clear((y & mask) | (z & ~mask));
					if ((y & mask) == mask)
						break;
				}
			}
		}
	}
	return (*this);
}

BITMAP &
BITMAP :: operator |=(const BITMAP &other)
{
	/* some optimisations */
	if (isOne()) {
		return (*this);
	} else if (other.isOne()) {
		*this = BITMAP(true);
		return (*this);
	}

	/* downgrade first */
	if (isXorConst())
		downgrade();
	if (other.isXorConst()) {
		*this |= BITMAP(other).downgrade();
	} else {
		size_t mask;

		addVar(other.pvar, other.nvar, &mask);

		size_t size = 1UL << other.nvar;
		size_t x;
		size_t y;
		size_t z;

		for (x = z = 0; x != size; x++, z |= mask, z++) {
			if (other.peek(x) == true) {
				for (y = 0;; y |= ~mask, y++) {
					set((y & mask) | (z & ~mask));
					if ((y & mask) == mask)
						break;
				}
			}
		}
	}
	return (*this);
}

BITMAP &
BITMAP :: addVar(const hpsat_var_t *ptr, size_t num, size_t *pmask)
{
	if (num == 0) {
		*pmask = (1UL << nvar) - 1UL;
		return (*this);
	}

	if (isXorConst())
		downgrade();

	hpsat_var_t *pnew_var;
	uint8_t *pnew_table;
	size_t mask;
	size_t size;

	size_t x;
	size_t y;
	size_t z;

	*pmask = 0;

	for (x = y = z = 0; x != num && y != nvar; z++) {
		if (ptr[x] < pvar[y]) {
			x++;
		} else if (ptr[x] > pvar[y]) {
			*pmask |= 1UL << z;
			y++;
		} else {
			x++;
			y++;
		}
	}

	while (x != num) {
		x++;
		z++;
	}

	while (y != nvar) {
		*pmask |= 1UL << z;
		y++;
		z++;
	}

	if (nvar == z)
		return (*this);

	mask = 0;
	pnew_var = new hpsat_var_t [z];
	size = 1UL << ((z < 3) ? 0 : (z - 3));
	pnew_table = new uint8_t [size];
	memset(pnew_table, 0, size);

	for (x = y = z = 0; x != num && y != nvar; z++) {
		if (ptr[x] < pvar[y]) {
			pnew_var[z] = ptr[x++];
			mask |= 1UL << z;
		} else if (ptr[x] > pvar[y]) {
			pnew_var[z] = pvar[y++];
		} else {
			pnew_var[z] = pvar[y++];
			x++;
		}
	}

	while (x != num) {
		mask |= 1UL << z;
		pnew_var[z++] = ptr[x++];
	}

	while (y != nvar)
		pnew_var[z++] = pvar[y++];

	size_t max = 1UL << nvar;
	size_t t;
	size_t u;
	size_t v;

	for (t = v = 0; t != max; t++, v |= mask, v++) {
		if (peek(t)) {
			for (u = 0;; u |= ~mask, u++) {
				size_t r = (u & mask) | (v & ~mask);
				pnew_table[r / 8] |= (1U << (r % 8));
				if ((u & mask) == mask)
					break;
			}
		}
	}

	delete [] table;
	delete [] pvar;

	table = pnew_table;
	pvar = pnew_var;
	nvar = z;

	return (*this);
}

BITMAP &
BITMAP :: remVar(const hpsat_var_t *ptr, const uint8_t *pvalue, size_t num)
{
	if (num == 0)
		return (*this);

	if (isXorConst()) {
		size_t x;
		size_t y;
		size_t z;

		bool toggle = (table == bitmap_true);

		for (x = y = z = 0; x != num && y != nvar; ) {
			if (ptr[x] < pvar[y]) {
				x++;
			} else if (ptr[x] > pvar[y]) {
				pvar[z++] = pvar[y++];
			} else {
				toggle ^= (pvalue != 0 && pvalue[x] != 0);
				x++;
				y++;
			}
		}

		while (y != nvar)
			pvar[z++] = pvar[y++];
		nvar = z;
		table = toggle ? bitmap_true : bitmap_false;
	} else {
		hpsat_var_t *pnew_var;
		uint8_t *pnew_table;
		size_t mask;
		size_t size;
		size_t value;

		size_t x;
		size_t y;
		size_t z;

		mask = 0;
		value = 0;

		for (x = y = z = 0; x != num && y != nvar; ) {
			if (ptr[x] < pvar[y]) {
				x++;
			} else if (ptr[x] > pvar[y]) {
				y++;
			} else {
				mask |= 1UL << y;
				if (pvalue != 0 && pvalue[x] != 0)
					value |= 1UL << y;
				x++;
				y++;
				z++;
			}
		}

		if (mask == 0) {
			return (*this);
		} else if (nvar == z) {
			/* removed all variables */
			bool toggle = peek(value);
			delete [] table;
			delete [] pvar;
			table = toggle ? bitmap_true : bitmap_false;
			pvar = 0;
			nvar = 0;
			return (*this);
		}

		pnew_var = new hpsat_var_t [nvar - z];
		size = 1UL << (((nvar - z) < 3) ? 0 : (nvar - z - 3));
		pnew_table = new uint8_t [size];
		memset(pnew_table, 0, size);

		for (x = y = z = 0; x != num && y != nvar; ) {
			if (ptr[x] < pvar[y]) {
				x++;
			} else if (ptr[x] > pvar[y]) {
				pnew_var[z++] = pvar[y++];
			} else {
				x++;
				y++;
			}
		}
		while (y != nvar)
			pnew_var[z++] = pvar[y++];

		size_t max = 1UL << nvar;
		size_t t;
		size_t u;

		for (t = u = 0; t != max; t |= mask, t++, u++) {
			if (peek((t & ~mask) | value))
				pnew_table[u / 8] |= (1U << (u % 8));
		}

		assert(u == (1UL << z));

		delete [] table;
		delete [] pvar;

		table = pnew_table;
		pvar = pnew_var;
		nvar = z;
	}
	return (*this);
}

BITMAP &
BITMAP :: sort(void)
{
	if (isXorConst())
		return (*this);

	hpsat_var_t rem[nvar];
	size_t size = 1UL << nvar;
	size_t mask = 0;
	size_t optimised = 0;
	size_t t;
	size_t v;

	xform();

	for (v = 0; v != nvar; v++) {
		if (peek(1UL << v)) {
			clear(1UL << v);
			optimised |= 1UL << v;
		}
	}

	for (size_t x = 0; x != size; x++) {
		if (peek(x))
			mask |= x;
	}

	if (mask == 0) {
		for (t = v = 0; v != nvar; v++) {
			if (~(optimised >> v) & 1)
				rem[t++] = pvar[v];
		}
		bool toggle = peek(0);
		delete [] table;
		table = toggle ? bitmap_true : bitmap_false;
		return (remVar(rem, 0, t));
	}

	for (v = 0; v != nvar; v++) {
		if ((optimised >> v) & 1)
			set(1UL << v);
	}

	xform();

	mask |= optimised;

	if (mask == (size - 1))
		return (*this);

	for (t = v = 0; t != nvar; t++) {
		if (~(mask >> t) & 1)
			rem[v++] = pvar[t];
	}
	return (remVar(rem, 0, v));
}

void
BITMAP :: print() const
{
	if (isXorConst()) {
		if (nvar == 0) {
			printf("%s", (table[0] & 1) ? "1" : "0");
		} else if (table[0] & 1) {
			printf("1");
			if (nvar)
				printf("^");
		}
		for (size_t n = 0; n != nvar; n++) {
			printf("v%zu", pvar[n]);
			if (n != nvar - 1)
				printf("^");
		}
	} else {
		size_t size = 1UL << nvar;

		for (size_t n = 0; n != nvar; n++)
			printf("[v%zu]", pvar[n]);

		printf(" = {");
		for (size_t n = 0; n != size; n++)
			printf("%d", peek(n));
		printf("}");
	}
}

int
BITMAP :: compare(const BITMAP & other, bool do_data) const
{
	if (nvar > other.nvar)
		return (1);
	else if (nvar < other.nvar)
		return (-1);

	for (size_t x = 0; x != nvar; x++) {
		if (pvar[x] > other.pvar[x])
			return (1);
		else if (pvar[x] < other.pvar[x])
			return (-1);
	}

	if (do_data) {
		if (isXorConst() > other.isXorConst())
			return (1);
		else if	(isXorConst() < other.isXorConst())
			return (-1);

		if (isXorConst()) {
			if (table[0] > other.table[0])
				return (1);
			else if (table[0] < other.table[0])
				return (-1);
		} else {
			const size_t size = 1UL << ((nvar < 3) ? 0 : (nvar - 3));
			for (size_t x = 0; x != size; x++) {
				if (table[x] > other.table[x])
					return (1);
				else if (table[x] < other.table[x])
					return (1);
			}
		}
	}
	return (0);
}

bool
BITMAP ::commonVar(const BITMAP &other) const
{
	size_t x;
	size_t y;

	for (x = y = 0; x != nvar && y != other.nvar; ) {
		if (pvar[x] > pvar[y])
			y++;
		else if (pvar[x] < pvar[y])
			x++;
		else
			return (true);
	}
	return (false);
}

bool
BITMAP :: isVarAssigned(size_t index) const
{
	if (isXorConst())
		return (true);

	const size_t size = 1UL << nvar;
	const size_t mask = 1UL << index;

	bool found = false;
	bool inv = false;

	for (size_t y = 0; y != size; y |= mask, y++, y &= ~mask) {
		if (peek(y) != peek(y | mask)) {
			if (found) {
				if (!inv)
					return (false);
			} else {
				found = true;
				inv = true;
			}
		} else {
			if (found) {
				if (inv)
					return (false);
			} else {
				found = true;
				inv = false;
			}
		}
	}
	return (true);
}

hpsat_var_t
BITMAP :: findAndVar(size_t index) const
{
	if (!isXorConst()) {
		for (size_t x = 0; x != nvar; x++) {
			if (!isVarAssigned(x)) {
				if (!index--)
					return (pvar[x]);
			}
		}
	}
	return (HPSAT_VAR_MIN);
}

static int
hpsat_compare(const void *a, const void *b)
{
	return ((BITMAP * const *)a)[0][0].compare(((BITMAP * const *)b)[0][0]);
}

bool
hpsat_squash_or(BITMAP_HEAD_t *phead)
{
	BITMAP *pa;
	BITMAP **pp;
	size_t count = 0;
	size_t x;
	bool any;

	for (pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		if (pa->nvar <= 1)
			pa->sort();
		count++;
	}
	if (count == 0)
		return (false);

	any = false;
	pp = new BITMAP * [count];

	count = 0;
	for (pa = TAILQ_FIRST(phead); pa; pa = pa->next())
		pp[count++] = pa;

	mergesort(pp, count, sizeof(pp[0]), &hpsat_compare);

	TAILQ_INIT(phead);

	for (x = 1; x != count; x++) {
		if (pp[x - 1]->compare(*pp[x], false) == 0) {
			pp[x][0] |= pp[x - 1][0];
			pp[x][0].sort();
			delete pp[x - 1];
			any = true;
		} else if (pp[x - 1]->isZero()) {
			delete pp[x - 1];
		} else if (pp[x - 1]->isOne()) {
			goto failure;
		} else {
			pp[x - 1]->insert_tail(phead);
		}
	}

	if (pp[x - 1]->isZero()) {
		delete pp[x - 1];
	} else if (pp[x - 1]->isOne()) {
		goto failure;
	} else {
		pp[x - 1]->insert_tail(phead);
	}
	delete [] pp;
	return (any);
failure:
	delete pp[x - 1];
	for (size_t y = x; y != count; y++)
		delete pp[y];
	delete [] pp;
	hpsat_free_bitmaps(phead);
	(new BITMAP(true))->insert_tail(phead);
	return (false);
}

bool
hpsat_squash_and(BITMAP_HEAD_t *phead)
{
	BITMAP *pa;
	BITMAP **pp;
	size_t count = 0;
	size_t x;
	bool any;

	for (pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		if (pa->nvar <= 1)
			pa->sort();
		count++;
	}
	if (count == 0)
		return (false);

	any = false;
	pp = new BITMAP * [count];

	count = 0;
	for (pa = TAILQ_FIRST(phead); pa; pa = pa->next())
		pp[count++] = pa;

	mergesort(pp, count, sizeof(pp[0]), &hpsat_compare);

	TAILQ_INIT(phead);

	for (x = 1; x != count; x++) {
		if (pp[x - 1]->compare(*pp[x], false) == 0) {
			pp[x][0] &= pp[x - 1][0];
			pp[x][0].sort();
			delete pp[x - 1];
			any = true;
		} else if (pp[x - 1]->isOne()) {
			delete pp[x - 1];
		} else if (pp[x - 1]->isZero()) {
			goto failure;
		} else {
			pp[x - 1]->insert_tail(phead);
		}
	}

	if (pp[count - 1]->isOne()) {
		delete pp[count - 1];
	} else if (pp[count - 1]->isZero()) {
		goto failure;
	} else {
		pp[count - 1]->insert_tail(phead);
	}
	delete [] pp;
	return (any);
failure:
	delete pp[x - 1];
	for (size_t y = x; y != count; y++)
		delete pp[y];
	delete [] pp;
	hpsat_free_bitmaps(phead);
	(new BITMAP(false))->insert_tail(phead);
	return (false);
}

bool
hpsat_sort_or(BITMAP_HEAD_t *phead)
{
	BITMAP *pa;
	BITMAP **pp;
	size_t count = 0;
	bool did_sort = false;

	for (pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		if (pa->nvar <= 1)
			pa->sort();
		count++;
	}
	if (count == 0)
		return (false);

	pp = new BITMAP * [count];

	count = 0;
	for (pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		if (pa->isOne()) {
			hpsat_free_bitmaps(phead);
			(new BITMAP(true))->insert_tail(phead);
			goto done;
		}
		pp[count++] = pa;
	}

	for (size_t x = 1; x != count; x++) {
		if (pp[x - 1]->compare(*pp[x]) > 0) {
			did_sort = true;
			break;
		}
	}
	if (did_sort)
		mergesort(pp, count, sizeof(pp[0]), &hpsat_compare);

	TAILQ_INIT(phead);

	for (size_t x = 1; x != count; x++) {
		if (pp[x - 1]->isZero() ||
		    pp[x - 1]->compare(*pp[x]) == 0)
			delete pp[x - 1];
		else
			pp[x - 1]->insert_tail(phead);
	}

	if (pp[count - 1]->isZero())
		delete pp[count - 1];
	else
		pp[count - 1]->insert_tail(phead);

done:
	delete [] pp;

	return (did_sort);
}

bool
hpsat_sort_and(BITMAP_HEAD_t *phead)
{
	BITMAP *pa;
	BITMAP **pp;
	size_t count = 0;
	bool did_sort = false;

	for (pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		if (pa->nvar <= 1)
			pa->sort();
		count++;
	}
	if (count == 0)
		return (false);
	pp = new BITMAP * [count];

	count = 0;
	for (pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		if (pa->isZero()) {
			hpsat_free_bitmaps(phead);
			(new BITMAP(false))->insert_tail(phead);
			goto done;
		}
		pp[count++] = pa;
	}

	for (size_t x = 1; x != count; x++) {
		if (pp[x - 1]->compare(*pp[x]) > 0) {
			did_sort = true;
			break;
		}
	}
	if (did_sort)
		mergesort(pp, count, sizeof(pp[0]), &hpsat_compare);

	TAILQ_INIT(phead);

	for (size_t x = 1; x != count; x++) {
		if (pp[x - 1]->isOne() ||
		    pp[x - 1]->compare(*pp[x]) == 0)
			delete pp[x - 1];
		else
			pp[x - 1]->insert_tail(phead);
	}

	if (pp[count - 1]->isOne())
		delete pp[count - 1];
	else
		pp[count - 1]->insert_tail(phead);

done:
	delete [] pp;

	return (did_sort);
}

hpsat_var_t
hpsat_maxvar(const BITMAP_HEAD_t *phead, hpsat_var_t limit)
{
	hpsat_var_t max = HPSAT_VAR_MIN;

	for (BITMAP *pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		const hpsat_var_t v = pa->maxVar(limit);
		if (max < v)
			max = v;
	}
	return (max);
}

hpsat_var_t
hpsat_minvar(const BITMAP_HEAD_t *phead, hpsat_var_t limit)
{
	hpsat_var_t min = HPSAT_VAR_MIN;

	for (BITMAP *pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		const hpsat_var_t v = pa->minVar(limit);
		if (v > HPSAT_VAR_MIN) {
			if (min == HPSAT_VAR_MIN || min > v)
				min = v;
		}
	}
	return (min);
}

void
hpsat_free_bitmaps(BITMAP_HEAD_t *phead)
{
	BITMAP *pa;

	while ((pa = TAILQ_FIRST(phead)) != 0)
		delete pa->remove(phead);
}

void
hpsat_check_inversion(BITMAP_HEAD_t *phead)
{
	BITMAP_HEAD_t temp;
	BITMAP *ba;
	BITMAP *bb;

	TAILQ_INIT(&temp);

	for (ba = TAILQ_FIRST(phead); ba; ba = ba->next())
		(new BITMAP(*ba))->toggleInverted().insert_tail(&temp);

	hpsat_sort_or(&temp);
	hpsat_sort_or(phead);

	ba = TAILQ_FIRST(phead);
	bb = TAILQ_FIRST(&temp);

	while (ba && bb) {
		switch (ba->compare(*bb)) {
		case -1:
			bb = bb->next();
			break;
		case 1:
			ba = ba->next();
			break;
		default:
			hpsat_free_bitmaps(phead);
			hpsat_free_bitmaps(&temp);
			(new BITMAP(true))->insert_tail(phead);
			return;
		}
	}

	hpsat_free_bitmaps(&temp);
}
