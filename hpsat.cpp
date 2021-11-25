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

void
hpsat_free(EQ_HEAD_t *phead)
{
	EQ *peq;

	while ((peq = TAILQ_FIRST(phead)))
		delete peq->remove(phead);
}

bool
hpsat_verify(const EQ &a, const EQ &b, const EQ &c, uint8_t function)
{
	hpsat_var_t vm = HPSAT_VAR_MIN - 1;
	hpsat_var_t v;

	v = a.maxVar();
	if (v > vm)
		vm = v;
	v = b.maxVar();
	if (v > vm)
		vm = v;
	v = c.maxVar();
	if (v > vm)
		vm = v;

	vm++;

	uint8_t *pused = new uint8_t [vm];
	uint8_t *pval = new uint8_t [vm];

	memset(pused, 0, sizeof(pused[0]) * vm);
	memset(pval, 0, sizeof(pval[0]) * vm);

	a.usedVar(pused);
	b.usedVar(pused);
	c.usedVar(pused);

	for (;;) {
		hpsat_var_t z;

		switch (function) {
		case 0:
			if ((a.expand_all(pval) ^ b.expand_all(pval)) != c.expand_all(pval))
				return (false);
			break;
		case 1:
			if ((a.expand_all(pval) || b.expand_all(pval)) != c.expand_all(pval))
				return (false);
			break;
		case 2:
			if ((a.expand_all(pval) && b.expand_all(pval)) != c.expand_all(pval))
				return (false);
			break;
		default:
			break;
		}

		for (z = HPSAT_VAR_MIN; z != vm; z++) {
			if (pused[z] == 0)
				continue;
			pval[z] ^= true;
			if (pval[z])
				break;
		}
		if (z == vm)
			break;
	}

	delete [] pused;
	delete [] pval;

	return (true);
}

EQ
EQ :: operator !() const
{
	EQ one(HPSAT_VAR_ONE);
	return (one ^ *this);
}

EQ &
EQ :: operator |=(const EQ &other)
{
	EQ temp(HPSAT_VAR_ORED);
	EQ *pa = new EQ();
	EQ *pb = new EQ();

	pa->insert_tail(&temp.head);
	pb->insert_tail(&temp.head);

	if (compare(other) < 0) {
		*pa = *this;
		*pb = EQ(other);
	} else {
		*pa = EQ(other);
		*pb = *this;
	}
	*this = temp.sort();
	return (*this);
}

EQ
EQ :: operator |(const EQ &other) const
{
	EQ temp(HPSAT_VAR_ORED);
	EQ *pa = new EQ();
	EQ *pb = new EQ();

	pa->insert_tail(&temp.head);
	pb->insert_tail(&temp.head);

	if (compare(other) < 0) {
		*pa = EQ(*this);
		*pb = EQ(other);
	} else {
		*pa = EQ(other);
		*pb = EQ(*this);
	}
	return (temp.sort());
}

EQ &
EQ :: operator ^=(const EQ &other)
{
	EQ temp(HPSAT_VAR_XORED);
	EQ *pa = new EQ();
	EQ *pb = new EQ();

	pa->insert_tail(&temp.head);
	pb->insert_tail(&temp.head);

	if (compare(other) < 0) {
		*pa = *this;
		*pb = EQ(other);
	} else {
		*pa = EQ(other);
		*pb = *this;
	}
	*this = temp.sort();
	return (*this);
}

EQ
EQ :: operator ^(const EQ &other) const
{
	EQ temp(HPSAT_VAR_XORED);
	EQ *pa = new EQ();
	EQ *pb = new EQ();

	pa->insert_tail(&temp.head);
	pb->insert_tail(&temp.head);

	if (compare(other) < 0) {
		*pa = EQ(*this);
		*pb = EQ(other);
	} else {
		*pa = EQ(other);
		*pb = EQ(*this);
	}
	return (temp.sort());
}

EQ &
EQ :: operator &=(const EQ &other)
{
	EQ temp(HPSAT_VAR_ANDED);
	EQ *pa = new EQ();
	EQ *pb = new EQ();

	pa->insert_tail(&temp.head);
	pb->insert_tail(&temp.head);

	if (compare(other) < 0) {
		*pa = *this;
		*pb = EQ(other);
	} else {
		*pa = EQ(other);
		*pb = *this;
	}
	*this = temp.sort();
	return (*this);
}

EQ
EQ :: operator &(const EQ &other) const
{
	EQ temp(HPSAT_VAR_ANDED);
	EQ *pa = new EQ();
	EQ *pb = new EQ();

	pa->insert_tail(&temp.head);
	pb->insert_tail(&temp.head);

	if (compare(other) < 0) {
		*pa = EQ(*this);
		*pb = EQ(other);
	} else {
		*pa = EQ(other);
		*pb = EQ(*this);
	}
	return (temp.sort());
}

static void
hpsat_merge(EQ_HEAD_t *pah, EQ_HEAD_t *pbh, EQ_HEAD_t *pch, bool isXor)
{
	EQ *pa = TAILQ_FIRST(pah);
	EQ *pb = TAILQ_FIRST(pbh);
	EQ *pan;
	EQ *pbn;

	TAILQ_INIT(pch);

	while (pa && pb) {
		pan = pa->next();
		pbn = pb->next();

		switch (pa->compare(*pb)) {
		case -1:
			pa->remove(pah)->insert_tail_non_zero(pch);
			pa = pan;
			break;
		case 0:
			if (isXor) {
				delete pa->remove(pah);
				delete pb->remove(pbh);
			} else {
				delete pa->remove(pah);
				pb->remove(pbh)->insert_tail_non_zero(pch);
			}
			pa = pan;
			pb = pbn;
			break;
		default:
			pb->remove(pbh)->insert_tail_non_zero(pch);
			pb = pbn;
			break;
		}
	}

	while (pa) {
		pan = pa->next();
		pa->remove(pah)->insert_tail_non_zero(pch);
		pa = pan;
	}

	while (pb) {
		pbn = pb->next();
		pb->remove(pbh)->insert_tail_non_zero(pch);
		pb = pbn;
	}
}

EQ &
EQ :: sort()
{
	EQ_HEAD_t temp;
	EQ *peq;
	EQ *pfq;
	EQ *pgq;
	EQ *phq;

#if defined(DEBUG) && defined(VERIFY)
	EQ copy(*this);
#endif
	if (first() == 0)
		goto done;

	assert(var == HPSAT_VAR_XORED || var == HPSAT_VAR_ORED || var == HPSAT_VAR_ANDED);

	/* recurse */
	for (peq = first(); peq; peq = pfq) {
		pfq = peq->sort().next();

		switch (peq->var) {
		case HPSAT_VAR_ZERO:
			if (var == HPSAT_VAR_ANDED)
				goto select;
			else
				delete peq->remove(&head);
			break;
		case HPSAT_VAR_ONE:
			if (var == HPSAT_VAR_ORED)
				goto select;
			else if (var == HPSAT_VAR_ANDED)
				delete peq->remove(&head);
			break;
		default:
			break;
		}
	}

	if (first() == 0)
		goto done;

	/* insertion sort */
	for (peq = first()->next(); peq; peq = phq) {
		phq = peq->next();

		for (pfq = peq; pfq != first(); pfq = pgq) {
			pgq = pfq->prev();

			int cmp = pgq->compare(*pfq);

			if (cmp > 0) {
				HPSAT_SWAP(*pgq, *pfq);
			} else if (cmp < 0) {
				break;
			} else {
				if (var == HPSAT_VAR_XORED) {
					delete pfq->remove(&head);
					delete pgq->remove(&head);
				} else {
					delete pfq->remove(&head);
				}
				break;
			}
		}
	}

	/* join same group type */
	for (peq = first(); peq; ) {
		if (peq->var == var) {
			peq->remove(&head);
			hpsat_merge(&head, &peq->head, &temp, var == HPSAT_VAR_XORED);
			delete peq;
			TAILQ_CONCAT(&head, &temp, entry);
			peq = first();
		} else {
			peq = peq->next();
		}
	}

	/* pullup */
	if ((peq = first())) {
		if (peq->next() == 0) {
			peq->remove(&head);
			*this = *peq;
			delete peq;
		}
	}
done:
	/* check if group is empty */
	if (first() == 0 && var < HPSAT_VAR_MIN)
		var = (var == HPSAT_VAR_ONE || var == HPSAT_VAR_ANDED) ? HPSAT_VAR_ONE : HPSAT_VAR_ZERO;

#if defined(DEBUG) && defined(VERIFY)
	if (hpsat_verify(copy, EQ(), *this, 0) == false) {
		copy.print(); printf(" BEFORE\n");
		print(); printf(" AFTER (invalid)\n");
		assert(false);
	}
#endif
	return (*this);

select:
	peq->remove(&head);
	*this = *peq;
	delete peq;
	goto done;
}

EQ &
EQ :: optimise()
{
#if defined(DEBUG) && defined(VERIFY)
	EQ copy(*this);
#endif
	sort();

	if (hpsat_simplify(*this))
		printf("SIMPLIFIED\n");

#if defined(DEBUG) && defined(VERIFY)
	if (hpsat_verify(copy, EQ(), *this, 0) == false) {
		copy.print(); printf(" BEFORE\n");
		print(); printf(" AFTER (invalid)\n");
		assert(false);
	}
#endif
	return (*this);
}

EQ &
EQ :: expand(hpsat_var_t _var, bool _value)
{
	if (var == _var) {
		var = _value ? HPSAT_VAR_ONE : HPSAT_VAR_ZERO;
	} else {
		for (EQ *peq = first(); peq; peq = peq->next())
			peq->expand(_var, _value);
	}
	return (*this);
}

bool
EQ :: expand_all(const uint8_t *pval) const
{
	bool temp;

	switch (var) {
	case HPSAT_VAR_XORED:
		temp = false;
		for (EQ *peq = first(); peq; peq = peq->next())
			temp ^= peq->expand_all(pval);
		break;
	case HPSAT_VAR_ORED:
		temp = false;
		for (EQ *peq = first(); peq; peq = peq->next())
			temp |= peq->expand_all(pval);
		break;
	case HPSAT_VAR_ANDED:
		temp = true;
		for (EQ *peq = first(); peq; peq = peq->next())
			temp &= peq->expand_all(pval);
		break;
	case HPSAT_VAR_ZERO:
		temp = false;
		break;
	case HPSAT_VAR_ONE:
		temp = true;
		break;
	default:
		temp = (pval[var] != 0);
		break;
	}
	return (temp);
}

void
EQ :: print(std::ostream &out) const
{
	switch (var) {
	case HPSAT_VAR_ZERO:
		out << "0";
		break;
	case HPSAT_VAR_ONE:
		out << "1";
		break;
	case HPSAT_VAR_ORED:
		out << "(";
		for (EQ *peq = first(); peq; peq = peq->next()) {
			peq->print();
			if (peq->next())
				out << "|";
		}
		out << ")";
		break;
	case HPSAT_VAR_XORED:
		out << "(";
		for (EQ *peq = first(); peq; peq = peq->next()) {
			peq->print();
			if (peq->next())
				out << "^";
		}
		out << ")";
		break;
	case HPSAT_VAR_ANDED:
		out << "(";
		for (EQ *peq = first(); peq; peq = peq->next()) {
			peq->print();
			if (peq->next())
				out << "&";
		}
		out << ")";
		break;
	default:
		out << "v[";
		out << var;
		out << "]";
		break;
	}
}

int
EQ :: compare(const EQ & other) const
{
	EQ *pa;
	EQ *pb;

	if (var > other.var)
		return (1);
	else if (var < other.var)
		return (-1);

	pa = first();
	pb = other.first();

	while (pa && pb) {
		int cmp = pa->compare(*pb);
		if (cmp != 0)
			return (cmp);
		pa = pa->next();
		pb = pb->next();
	}
	return ((pa != 0) - (pb != 0));
}

bool *
EQ :: toTable(uint8_t *pval, hpsat_var_t vMax) const
{
	uint8_t *ptemp = new uint8_t [vMax];
	hpsat_var_t log2 = 0;
	bool *ptable;

	memset(ptemp, 0, sizeof(ptemp[0]) * vMax);

	for (hpsat_var_t x = HPSAT_VAR_MIN; x < vMax; x++)
		log2 += (pval[x] != 0);

	if (log2 > 24)
		return (0);

	ptable = new bool [1UL << log2];

	for (hpsat_var_t x = 0; x != (1UL << log2); x++) {

		ptable[x] = expand_all(ptemp);

		for (hpsat_var_t z = HPSAT_VAR_MIN; z < vMax; z++) {
			if (pval[z] == 0)
				continue;
			ptemp[z] ^= true;
			if (ptemp[z])
				break;
		}
	}
	return (ptable);
}
