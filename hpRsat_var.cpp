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

hprsat_var_t
hprsat_maxvar(const VAR_HEAD_t *phead, hprsat_var_t limit)
{
	hprsat_var_t max = HPRSAT_VAR_MIN;
	hprsat_var_t v;

	for (VAR *pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		if (pa->add != 0) {
			v = pa->add->maxVar(limit);
		} else {
			v = pa->var;
			if (v >= limit)
				continue;
		}
		if (max < v)
			max = v;
	}
	return (max);
}

hprsat_var_t
hprsat_minvar(const VAR_HEAD_t *phead, hprsat_var_t limit)
{
	hprsat_var_t min = HPRSAT_VAR_MIN;
	hprsat_var_t v;

	for (VAR *pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		if (pa->add != 0) {
			v = pa->add->minVar(limit);
		} else {
			v = pa->var;
		}
		if (v > limit &&
		    (min == HPRSAT_VAR_MIN || min > v))
			min = v;
	}
	return (min);
}

void
hprsat_free(VAR_HEAD_t *phead)
{
	VAR *pa;

	while ((pa = TAILQ_FIRST(phead)) != 0)
		delete pa->remove(phead);
}

VAR :: ~VAR()
{
	delete add;
}

VAR &
VAR :: addVar(hprsat_var_t delta)
{
	if (add != 0)
		add->addVar(delta);
	else
		var += delta;
	return (*this);
}

VAR &
VAR :: mulVar(hprsat_var_t factor)
{
	if (add != 0)
		add->mulVar(factor);
	else
		var *= factor;
	return (*this);
}

bool
VAR :: isVariable() const
{
	if (pwr != 0) {
		if (add != 0)
			return (add->isVariable());
		else
			return (true);
	} else {
		return (false);
	}
}

bool
VAR :: contains(hprsat_var_t _var) const
{
	if (add != 0)
		return (add->contains(_var));
	else
		return (var == _var);
}

VAR &
VAR :: sort()
{
	const hprsat_pwr_t m = (hprsat_global_mod - 1) * HPRSAT_PWR_UNIT;

	if (add != 0)
		add->sort();

	if (pwr != 0) {
		pwr %= m;
		if (pwr < 0)
			pwr += m;
		if (pwr == 0)
			pwr = m;
	}
	return (*this);
}

VAR *
VAR :: sort_insert_tail(VAR_HEAD_t *phead, hprsat_val_t &factor)
{
	bool isNaN;
	const hprsat_val_t value = getConst(isNaN);
	VAR *pn = next();

	if (isNaN) {
		TAILQ_INSERT_TAIL(phead, this, entry);
	} else {
		delete this;
		factor *= value;
	}
	return (pn);
}

void
VAR :: print(std::ostream &out) const
{
	/* handle special case first */
	if (add == 0 && pwr == (hprsat_global_mod - 1) * HPRSAT_PWR_UNIT) {
		out << "v" << var;
		return;
	}

	if (pwr != HPRSAT_PWR_UNIT) {
		if (add != 0) {
			out << "pow(";
			add->print(out);
			out << "," << (float)pwr / (float)HPRSAT_PWR_UNIT << ")";
		} else {
			out << "pow(x" << var << "," << (float)pwr / (float)HPRSAT_PWR_UNIT << ")";
		}
	} else {
		if (add != 0) {
			out << "(";
			add->print(out);
			out << ")";
		} else {
			out << "x" << var;
		}
	}
}

VAR &
VAR :: operator =(const VAR &other)
{
	if (this == &other)
		return (*this);

	delete add;

	if (other.add != 0)
		add = new ADD(*other.add);
	else
		add = 0;

	var = other.var;
	pwr = other.pwr;

	return (*this);
}

int
VAR :: compare(const VAR & other, bool exprOnly) const
{
	int ret = (add != 0) - (other.add != 0);
	if (ret)
		return (ret);
	if (add != 0) {
		ret = add->compare(*other.add);
		if (ret)
			return (ret);
	} else {
		if (var < other.var)
			return (-1);
		else if (var > other.var)
			return (1);
	}

	if (exprOnly)
		return (0);
	else if (pwr < other.pwr)
		return (-1);
	else if (pwr > other.pwr)
		return (1);
	else
		return (0);
}

hprsat_val_t
VAR :: getConst(bool &isNaN) const
{
	isNaN = (pwr != 0);
	if (isNaN) {
		if (add != 0) {
			hprsat_val_t val = add->getConst(isNaN);
			if (isNaN) {
				return (val);
			} else {
				const hprsat_pwr_t m = (hprsat_global_mod - 1) * HPRSAT_PWR_UNIT;

				hprsat_pwr_t _exp;
				hprsat_val_t _base;

				_exp = pwr;
				if (_exp != 0) {
					_exp = _exp % m;
					if (_exp < 0)
						_exp += m;
					if (_exp == 0)
						_exp = m;
				}

				if (_exp & HPRSAT_PWR_SQRT) {
					_base = hprsat_global_sqrt[val];
					if (val != 0 && _base == 0) {
						isNaN = true;
						return (0);
					}
				} else {
					_base = 1;
				}

				while (_exp >= HPRSAT_PWR_UNIT) {
					if (_exp & HPRSAT_PWR_UNIT) {
						_base *= val;
						_base %= hprsat_global_mod;
					}
					_exp /= 2;
					val *= val;
					val %= hprsat_global_mod;
				}
				return (_base);
			}
		} else {
			return (0);
		}
	} else {
		return (1);
	}
}

static int
hprsat_compare(const void *a, const void *b)
{
	return ((VAR * const *)a)[0][0].compare(((VAR * const *)b)[0][0]);
}

bool
hprsat_sort_var(VAR_HEAD_t *phead, hprsat_val_t &defactor)
{
	VAR *pa;
	VAR **pp;
	size_t count = 0;
	size_t x;
	size_t y;
	bool did_sort = false;

	for (pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		pa->sort();
		count++;
	}

	if (count == 0)
		return (false);

	pp = new VAR * [count];

	count = 0;
	for (pa = TAILQ_FIRST(phead); pa; pa = pa->next())
		pp[count++] = pa;

	for (x = 1; x != count; x++) {
		if ((pp + x - 1)[0][0].compare((pp + x)[0][0], true) > 0) {
			did_sort = true;
			break;
		}
	}
	if (did_sort)
		mergesort(pp, count, sizeof(pp[0]), &hprsat_compare);

	TAILQ_INIT(phead);

	for (x = 0; x != count; ) {
		bool isNaN;
		hprsat_val_t value;

		for (y = x + 1; y != count; y++) {
			if ((pp + x)[0][0].compare((pp + y)[0][0], true) != 0)
				break;
			*pp[x] *= *pp[y];
		}

		if (y != x + 1)
			pp[x]->sort();

		value = pp[x]->getConst(isNaN);
		if (isNaN == false) {
			defactor *= value;
			defactor %= hprsat_global_mod;
			did_sort = true;
			delete pp[x];
		} else {
			pp[x]->insert_tail(phead);
		}
		while (++x != y) {
			did_sort = true;
			delete pp[x];
		}
	}

	delete [] pp;

	return (did_sort);
}
