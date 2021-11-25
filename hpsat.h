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

#ifndef _HPSAT_H_
#define	_HPSAT_H_

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include <sys/queue.h>

#include <iostream>

#define	HPSAT_SWAP(a,b) do {	       	\
	typeof(a) __tmp = (a);	       	\
	(a) = (b);		       	\
	(b) = __tmp;		       	\
} while (0)

enum {
	HPSAT_VAR_ZERO = 0,
	HPSAT_VAR_ONE = 1,
	HPSAT_VAR_ORED = 2,
	HPSAT_VAR_XORED = 3,
	HPSAT_VAR_ANDED = 4,
	HPSAT_VAR_MIN = 5, /* inclusive */
	HPSAT_VAR_MAX = SIZE_MAX / 2, /* exclusive */
};

typedef size_t hpsat_var_t;

typedef bool (eq_solve_cb_t)(const uint8_t *, void *);

class EQ;
typedef TAILQ_CLASS_HEAD(EQ_HEAD, EQ) EQ_HEAD_t;
typedef TAILQ_CLASS_ENTRY(EQ) EQ_ENTRY_t;

struct EQP {
	EQ *p;
	bool s;
};

class EQ {
public:
	EQ_HEAD_t head;
	EQ_ENTRY_t entry;
	hpsat_var_t var;

	EQ(const EQ &other) {
		TAILQ_INIT(&head);
		var = other.var;
		for (EQ *peq = other.first(); peq; peq = peq->next())
			peq->dup()->insert_tail(&head);
	}

	EQ(hpsat_var_t _var = HPSAT_VAR_ZERO) {
		TAILQ_INIT(&head);
		var = _var;
	}

	~EQ() {
		EQ *peq;

		while ((peq = first()))
			delete peq->remove(&head);
	}

	EQ *dup(void) const {
		return (new EQ(*this));
	}

	bool contains(hpsat_var_t _var) const {
		if (var == _var)
			return (true);

		for (EQ *peq = first(); peq; peq = peq->next()) {
			if (peq->contains(_var))
				return (true);
		}
		return (false);
	}
	EQ & operator =(const EQ &other) {
		EQ *peq;
		EQ *pfq = (EQ *)(uintptr_t)&other;

		if (this == &other)
			return (*this);
		var = other.var;

		while ((peq = first()))
			delete peq->remove(&head);
		TAILQ_CONCAT(&head, &pfq->head, entry);
		return (*this);
	}

	EQ operator !() const;

	EQ & operator |=(const EQ &);
	EQ operator |(const EQ &) const;

	EQ & operator &=(const EQ &);
	EQ operator &(const EQ &) const;

	EQ & operator ^=(const EQ &);
	EQ operator ^(const EQ &) const;

	EQ & expand(hpsat_var_t, bool);

	hpsat_var_t maxVar() const {
		hpsat_var_t vmax = var;
		for (EQ *peq = first(); peq; peq = peq->next()) {
			hpsat_var_t v = peq->maxVar();
			if (v > vmax)
				vmax = v;
		}
		return (vmax);
	}

	hpsat_var_t usedVar(uint8_t *pval) const {
		hpsat_var_t retval = (pval[var] == 0);

		pval[var] = 1;

		for (EQ *pe = first(); pe; pe = pe->next())
			retval += pe->usedVar(pval);

		return (retval);
	}

	bool *toTable(uint8_t *pval, hpsat_var_t vMax) const;

	bool isConst() const {
		return (maxVar() < HPSAT_VAR_MIN);
	}

	bool isXOR() const {
		if (var == HPSAT_VAR_ORED || var == HPSAT_VAR_ANDED)
			return (false);
		for (EQ *peq = first(); peq; peq = peq->next()) {
			if (peq->isXOR() == false)
				return (false);
		}
		return (true);
	}

	bool simplify(EQP *);

	EQ & sort();
	EQ & optimise();

	bool expand_all(const uint8_t *pval) const;

	void print(std::ostream &out = std::cout) const;

	EQ & insert_tail(EQ_HEAD_t *phead) {
		TAILQ_INSERT_TAIL(phead, this, entry);
		return (*this);
	}
	void insert_tail_non_zero(EQ_HEAD_t *phead) {
		if (var == HPSAT_VAR_ZERO)
			delete this;
		else
			TAILQ_INSERT_TAIL(phead, this, entry);
	}
	EQ & insert_head(EQ_HEAD_t *phead) {
		TAILQ_INSERT_HEAD(phead, this, entry);
		return (*this);
	}
	EQ *remove(EQ_HEAD_t *phead) {
#ifdef DEBUG
		for (EQ *pa = TAILQ_FIRST(phead); pa != this; pa = pa->next())
			assert(pa);
#endif
		TAILQ_REMOVE(phead, this, entry);
		return (this);
	}
	EQ *first() const {
		return (TAILQ_FIRST(&head));
	}
	EQ *last() const {
		return (TAILQ_LAST(&head, EQ_HEAD));
	}
	EQ *prev() const {
		return (TAILQ_PREV(this, EQ_HEAD, entry));
	}
	EQ *next() const {
		return (TAILQ_NEXT(this, entry));
	}

	int compare(const EQ & other) const;

	bool operator >(const EQ & other) const {
		return (compare(other) > 0);
	}
	bool operator <(const EQ & other) const {
		return (compare(other) < 0);
	}
	bool operator >=(const EQ & other) const {
		return (compare(other) >= 0);
	}
	bool operator <=(const EQ & other) const {
		return (compare(other) <= 0);
	}
	bool operator == (const EQ & other) const {
		return (compare(other) == 0);
	}
	bool operator !=(const EQ & other) const {
		return (compare(other) != 0);
	}

	bool solve(uint8_t *pvar, hpsat_var_t vmax, eq_solve_cb_t *cb = 0, void *arg = 0);

	int from_cnf(std::istream &);
};

/* simplify function */

extern bool hpsat_simplify(EQ &);

/* generic functions */

extern void hpsat_free(EQ_HEAD_t *);
extern bool hpsat_verify(const EQ &a, const EQ &b, const EQ &c, uint8_t function);

#endif					/* _HPSAT_H_ */
