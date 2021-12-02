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

#ifndef _HPSAT_H_
#define	_HPSAT_H_

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include <sys/queue.h>

#include <iostream>

#define	HPSAT_SWAP(a,b) do {			\
	__typeof(a) __tmp = (a);		\
	(a) = (b);				\
	(b) = __tmp;				\
} while (0)

typedef ssize_t hpsat_var_t;
constexpr hpsat_var_t HPSAT_VAR_MAX = SSIZE_MAX;	/* exclusive */
constexpr hpsat_var_t HPSAT_VAR_MIN = -1;		/* exclusive */

extern uint8_t bitmap_true[1];
extern uint8_t bitmap_false[1];

extern ssize_t hpsat_binsearch(const hpsat_var_t *, size_t, hpsat_var_t);

class BITMAP;
typedef TAILQ_CLASS_HEAD(BITMAP_HEAD, BITMAP) BITMAP_HEAD_t;
typedef TAILQ_CLASS_ENTRY(BITMAP) BITMAP_ENTRY_t;

class ANDMAP;
typedef TAILQ_CLASS_HEAD(ANDMAP_HEAD, ANDMAP) ANDMAP_HEAD_t;
typedef TAILQ_CLASS_ENTRY(ANDMAP) ANDMAP_ENTRY_t;

class XORMAP;
typedef TAILQ_CLASS_HEAD(XORMAP_HEAD, XORMAP) XORMAP_HEAD_t;
typedef TAILQ_CLASS_ENTRY(XORMAP) XORMAP_ENTRY_t;

class BITMAP {
public:
	BITMAP() {
		pvar = 0;
		nvar = 0;
		table = bitmap_false;
	};
	BITMAP(const BITMAP &other) {
		pvar = 0;
		nvar = 0;
		table = 0;
		*this = other;
	};
	BITMAP(bool value) {
		table = value ? bitmap_true : bitmap_false;
		pvar = 0;
		nvar = 0;
	};
	BITMAP(hpsat_var_t var, bool value) {
		table = value ? bitmap_true : bitmap_false;
		pvar = new hpsat_var_t [1];
		nvar = 1;
		pvar[0] = var;
	};
	~BITMAP() {
		if (!isXorConst())
			delete [] table;
		delete [] pvar;
	};
	BITMAP *dup(void) const {
		return (new BITMAP(*this));
	};

	BITMAP_ENTRY_t entry;
	uint8_t *table;
	hpsat_var_t *pvar;
	size_t nvar;

	bool isVarAssigned(size_t) const;

	size_t numAndVar() const {
		size_t num = 0;
		for (size_t x = nvar; x--; ) {
			if (!isVarAssigned(x))
				num++;
		}
		return (num);
	};

	hpsat_var_t findAndVar(size_t = 0) const;

	hpsat_var_t maxVar(hpsat_var_t limit = HPSAT_VAR_MAX) const {
		for (size_t x = nvar; x--; ) {
			if (pvar[x] < limit)
				return (pvar[x]);
		}
		return (HPSAT_VAR_MIN);
	};
	hpsat_var_t minVar(hpsat_var_t limit = HPSAT_VAR_MIN) const {
		for (size_t x = 0; x != nvar; x++) {
			if (pvar[x] > limit)
				return (pvar[x]);
		}
		return (HPSAT_VAR_MIN);
	};
	BITMAP & shiftVar(hpsat_var_t shift) {
		for (size_t x = 0; x != nvar; x++)
			pvar[x] += shift;
		return (*this);
	};
	bool isXorConst() const {
		return (table == bitmap_true || table == bitmap_false);
	};
	bool isOne() const {
		if (isXorConst()) {
			return (nvar == 0 && table == bitmap_true);
		} else {
			size_t size = 1UL << nvar;
			for (size_t x = 0; x != size; x++) {
				if (peek(x) == 0)
					return (false);
			}
			return (true);
		}
	};
	bool isZero() const {
		if (isXorConst()) {
			return (nvar == 0 && table == bitmap_false);
		} else {
			size_t size = 1UL << nvar;
			for (size_t x = 0; x != size; x++) {
				if (peek(x) != 0)
					return (false);
			}
			return (true);
		}
	};
	bool isInverted() const {
		return (table[0] & 1);
	};
	BITMAP & toggleInverted() {
		if (isXorConst()) {
			table = (table[0] & 1) ? bitmap_false : bitmap_true;
		} else {
			table[0] ^= 1;
		}
		return (*this);
	};
	bool peek(size_t bit) const {
#ifdef DEBUG
		assert(bit < (1UL << nvar));
#endif
		return ((table[bit / 8] & (1U << (bit % 8))) ? true : false);
	};
	void toggle(size_t bit) const {
#ifdef DEBUG
		assert(bit < (1UL << nvar));
#endif
		table[bit / 8] ^= (1U << (bit % 8));
	};
	void set(size_t bit) const {
#ifdef DEBUG
		assert(bit < (1UL << nvar));
#endif
		table[bit / 8] |= (1U << (bit % 8));
	};
	void clear(size_t bit) const {
#ifdef DEBUG
		assert(bit < (1UL << nvar));
#endif
		table[bit / 8] &= ~(1U << (bit % 8));
	};
	bool commonVar(const BITMAP &) const;
	bool contains(hpsat_var_t var) const {
		if (nvar == 0)
			return (false);
		if (var < pvar[0] || var > pvar[nvar - 1])
			return (false);
		return (hpsat_binsearch(pvar, nvar, var) != -1);
	};
	BITMAP & operator =(const BITMAP &other);
	BITMAP & operator ^=(const BITMAP &other);
	BITMAP & operator &=(const BITMAP &other);
	BITMAP & operator |=(const BITMAP &other);

	BITMAP operator ^(const BITMAP &other) const {
		BITMAP temp(*this);
		temp ^= other;
		return (temp);
	};
	BITMAP operator &(const BITMAP &other) const {
		BITMAP temp(*this);
		temp &= other;
		return (temp);
	};
	BITMAP operator |(const BITMAP &other) const {
		BITMAP temp(*this);
		temp |= other;
		return (temp);
	};

	BITMAP & xform(void);
	BITMAP & downgrade(void);
	BITMAP & sort(void);
	BITMAP & addVar(const hpsat_var_t *, size_t, size_t *);
	BITMAP & remVar(const hpsat_var_t *, const uint8_t *, size_t);

	BITMAP & addVarSequence(hpsat_var_t start, uint8_t num) {
		hpsat_var_t var[num];
		size_t mask;
		for (uint8_t x = 0; x != num; x++)
			var[x] = start + x;
		return (addVar(var, num, &mask));
	};

	BITMAP & expand(hpsat_var_t var, uint8_t value) {
		if (contains(var))
			return (remVar(&var, &value, 1));
		else
			return (*this);
	};

	bool expand_all(const uint8_t *pval) const {
		bool retval;
		if (isXorConst()) {
			retval = isInverted();
			for (size_t x = 0; x != nvar; x++)
				retval ^= (pval[pvar[x]] & 1);
		} else {
			size_t offset = 0;
			for (size_t x = 0; x != nvar; x++) {
				if (pval[pvar[x]] & 1)
					offset |= 1UL << x;
			}
			retval = peek(offset);
		}
		return (retval);
	};

	void print() const;

	XORMAP toXorMap() const;

	int compare(const BITMAP & other, bool = true) const;

	bool operator >(const BITMAP & other) const {
		return (compare(other) > 0);
	};
	bool operator <(const BITMAP & other) const {
		return (compare(other) < 0);
	};
	bool operator >=(const BITMAP & other) const {
		return (compare(other) >= 0);
	};
	bool operator <=(const BITMAP & other) const {
		return (compare(other) <= 0);
	};
	bool operator == (const BITMAP & other) const {
		return (compare(other) == 0);
	};
	bool operator !=(const BITMAP & other) const {
		return (compare(other) != 0);
	};

	BITMAP & insert_tail(BITMAP_HEAD_t *phead){
		TAILQ_INSERT_TAIL(phead, this, entry);
		return (*this);
	};
	BITMAP & insert_head(BITMAP_HEAD_t *phead){
		TAILQ_INSERT_HEAD(phead, this, entry);
		return (*this);
	};
	BITMAP *remove(BITMAP_HEAD_t *phead){
#ifdef DEBUG
		for (BITMAP *pa = TAILQ_FIRST(phead); pa != this; pa = pa->next())
			assert(pa);
#endif
		TAILQ_REMOVE(phead, this, entry);
		return (this);
	};
	BITMAP *next() const {
		return (TAILQ_NEXT(this, entry));
	};
};

extern hpsat_var_t hpsat_maxvar(const BITMAP_HEAD_t *, hpsat_var_t = HPSAT_VAR_MAX);
extern hpsat_var_t hpsat_minvar(const BITMAP_HEAD_t *, hpsat_var_t = HPSAT_VAR_MIN);
extern void hpsat_simplify_or(BITMAP_HEAD_t *, BITMAP_HEAD_t * = 0);
extern void hpsat_simplify_and(BITMAP_HEAD_t *, BITMAP_HEAD_t * = 0);
extern bool hpsat_sort_or(BITMAP_HEAD_t *);
extern bool hpsat_sort_and(BITMAP_HEAD_t *);
extern bool hpsat_squash_or(BITMAP_HEAD_t *);
extern bool hpsat_squash_and(BITMAP_HEAD_t *);
extern void hpsat_demux(BITMAP_HEAD_t *, hpsat_var_t *);
extern void hpsat_free(BITMAP_HEAD_t *);
extern void hpsat_check_inversion(BITMAP_HEAD_t *);

extern int hpsat_loadcnf(std::istream &, BITMAP_HEAD_t *, hpsat_var_t * = 0, hpsat_var_t * = 0);
extern void hpsat_printcnf(std::ostream &, BITMAP_HEAD_t *, hpsat_var_t = 0, hpsat_var_t = 0);

class ANDMAP {
public:
	ANDMAP_ENTRY_t entry;
	BITMAP_HEAD_t head;

	ANDMAP() {
		TAILQ_INIT(&head);
	};
	ANDMAP(const ANDMAP &other) {
		TAILQ_INIT(&head);
		*this = other;
	};
	ANDMAP(const BITMAP &other) {
		TAILQ_INIT(&head);
		other.dup()->insert_tail(&head);
	};
	ANDMAP(bool _value) {
		TAILQ_INIT(&head);
		if (_value == false)
			(new BITMAP())->insert_tail(&head);
	};
	ANDMAP(hpsat_var_t var, bool value) {
		TAILQ_INIT(&head);
		(new BITMAP(var, value))->insert_tail(&head);
	};
	~ANDMAP() {
		hpsat_free(&head);
	};
	ANDMAP *dup(void) const {
		return (new ANDMAP(*this));
	};
	hpsat_var_t maxVar(hpsat_var_t limit = HPSAT_VAR_MAX) const {
		return (hpsat_maxvar(&head, limit));
	};
	hpsat_var_t minVar(hpsat_var_t limit = HPSAT_VAR_MIN) const {
		return (hpsat_minvar(&head, limit));
	};
	ANDMAP & shiftVar(hpsat_var_t shift) {
		for (BITMAP *pa = TAILQ_FIRST(&head); pa; pa = pa->next())
			pa->shiftVar(shift);
		return (*this);
	};
	bool isAtomic() const {
		const BITMAP *pa = TAILQ_FIRST(&head);
		return (pa == 0 || pa->next() == 0);
	};
	bool isXorConst() const {
		const BITMAP *pa = TAILQ_FIRST(&head);
		if (pa == 0)
			return (true);
		else if (pa->next() == 0)
			return (pa->isXorConst());
		else
			return (false);
	};
	bool isOne() const {
		return (TAILQ_FIRST(&head) == 0);
	};
	bool isZero() const {
		BITMAP *pa = TAILQ_FIRST(&head);
		if (pa != 0)
			return (pa->isZero());
		else
			return (false);
	};
	bool isInverted() const {
		if (isOne())
			return (true);
		if (isXorConst())
			return (first()->isInverted());
		return (false);
	};
	bool contains(hpsat_var_t var) const {
		for (BITMAP *pa = TAILQ_FIRST(&head); pa; pa = pa->next()) {
			if (pa->contains(var))
				return (true);
		}
		return (false);
	};
	ANDMAP & substitute(hpsat_var_t var, const BITMAP &expr);
	ANDMAP & operator =(const ANDMAP &other) {
		if (this == &other)
			return (*this);
		hpsat_free(&head);
		for (BITMAP *pa = TAILQ_FIRST(&other.head); pa; pa = pa->next())
			pa->dup()->insert_tail(&head);
		return (*this);
	};
	ANDMAP & operator ^=(const ANDMAP &other) {
#ifdef DEBUG
		assert(other.isXorConst());
		assert(isXorConst());
#endif
		uint8_t state = 0;

		if (TAILQ_FIRST(&head) == 0)
			state |= 1;
		if (TAILQ_FIRST(&other.head) == 0)
			state |= 2;

		switch (state) {
		case 0:
			*first() ^= *other.first();
			break;
		case 1:
			other.first()->dup()->
			    insert_tail(&head).toggleInverted();
			break;
		case 2:
			first()->toggleInverted();
			break;
		default:
			(new BITMAP())->insert_tail(&head);
			break;
		}
		return (sort());
	};
	ANDMAP operator ^(const ANDMAP &other) const {
		ANDMAP temp(*this);
		temp ^= other;
		return (temp);
	};
	ANDMAP & operator &=(const ANDMAP &other) {
		for (BITMAP *pa = TAILQ_FIRST(&other.head); pa; pa = pa->next())
			pa->dup()->insert_tail(&head);
		return (sort());
	};
	ANDMAP operator &(const ANDMAP &other) const {
		ANDMAP temp(*this);
		temp &= other;
		return (temp);
	};
	ANDMAP & xored(const ANDMAP &, hpsat_var_t);
	ANDMAP & sort();

	ANDMAP & expand(hpsat_var_t var, uint8_t value) {
		for (BITMAP *pa = TAILQ_FIRST(&head); pa; pa = pa->next())
			pa->expand(var, value);
		return (sort());
	};
	bool expand_all(const uint8_t *pval) const {
		bool retval = 1;
		for (BITMAP *pa = TAILQ_FIRST(&head); pa; pa = pa->next())
			retval &= pa->expand_all(pval);
		return (retval);
	};
	void print() const {
		if (isOne()) {
			printf("1");
		} else if (isZero()) {
			printf("0");
		} else for (BITMAP *pa = TAILQ_FIRST(&head); pa; pa = pa->next()) {
			printf("(");
			pa->print();
			printf(")");
			if (pa->next())
				printf("&");
		  }
	};
	ANDMAP & insert_tail(ANDMAP_HEAD_t *phead){
		TAILQ_INSERT_TAIL(phead, this, entry);
		return (*this);
	};
	ANDMAP & insert_head(ANDMAP_HEAD_t *phead){
		TAILQ_INSERT_HEAD(phead, this, entry);
		return (*this);
	};
	ANDMAP *remove(ANDMAP_HEAD_t *phead){
#ifdef DEBUG
		for (ANDMAP *pa = TAILQ_FIRST(phead); pa != this; pa = pa->next())
			assert(pa);
#endif
		TAILQ_REMOVE(phead, this, entry);
		return (this);
	};
	ANDMAP *prev() const {
		return (TAILQ_PREV(this, ANDMAP_HEAD, entry));
	};
	ANDMAP *next() const {
		return (TAILQ_NEXT(this, entry));
	};
	BITMAP *first(void) const {
		return (TAILQ_FIRST(&head));
	};
	BITMAP *last(void) const {
		return (TAILQ_LAST(&head, BITMAP_HEAD));
	};
	int compare(const ANDMAP & other, bool compare_data = true, bool compare_count = true) const {
		if (compare_count) {
			const size_t na = count();
			const size_t nb = other.count();

			if (na > nb)
				return (1);
			else if (na < nb)
				return (-1);
		}

		const BITMAP *pa = TAILQ_FIRST(&head);
		const BITMAP *pb = TAILQ_FIRST(&other.head);

		while (pa && pb) {
			const int ret = pa->compare(*pb, compare_data);
			if (ret)
				return (ret);
			pa = pa->next();
			pb = pb->next();
		}
		return ((pa != 0) - (pb != 0));
	};
	bool operator >(const ANDMAP & other) const {
		return (compare(other) > 0);
	};
	bool operator <(const ANDMAP & other) const {
		return (compare(other) < 0);
	};
	bool operator >=(const ANDMAP & other) const {
		return (compare(other) >= 0);
	};
	bool operator <=(const ANDMAP & other) const {
		return (compare(other) <= 0);
	};
	bool operator == (const ANDMAP & other) const {
		return (compare(other) == 0);
	};
	bool operator !=(const ANDMAP & other) const {
		return (compare(other) != 0);
	};
	bool mergedTo(ANDMAP &) const;
	size_t count() const {
		size_t retval = 0;
		for (BITMAP *pa = TAILQ_FIRST(&head); pa; pa = pa->next())
			retval++;
		return (retval);
	};
	BITMAP toBitmap() const {
		BITMAP temp(true);
		for (BITMAP *pa = TAILQ_FIRST(&head); pa; pa = pa->next()) {
			temp &= *pa;
			temp.sort();
		}
		return (temp);
	};
	ANDMAP & xorify();
};

extern hpsat_var_t hpsat_maxvar(const ANDMAP_HEAD_t *, hpsat_var_t = HPSAT_VAR_MAX);
extern hpsat_var_t hpsat_minvar(const ANDMAP_HEAD_t *, hpsat_var_t = HPSAT_VAR_MIN);
extern size_t hpsat_numvar(const ANDMAP_HEAD_t *, hpsat_var_t = HPSAT_VAR_MAX, BITMAP * = 0);
extern void hpsat_merge(ANDMAP_HEAD_t *);
extern bool hpsat_sort_or(ANDMAP_HEAD_t *);
extern bool hpsat_sort_xor_accumulate(ANDMAP_HEAD_t *);
extern bool hpsat_sort_xor_value(ANDMAP_HEAD_t *);
extern void hpsat_bitmap_to_andmap(const BITMAP_HEAD_t *, ANDMAP_HEAD_t *);
extern void hpsat_free(ANDMAP_HEAD_t *);

class XORMAP {
public:
	XORMAP_ENTRY_t entry;
	ANDMAP_HEAD_t head;

	XORMAP() {
		TAILQ_INIT(&head);
	};
	XORMAP(const XORMAP &other) {
		TAILQ_INIT(&head);
		*this = other;
	};
	XORMAP(const ANDMAP &other) {
		TAILQ_INIT(&head);
		other.dup()->insert_tail(&head);
	};
	XORMAP(const BITMAP &other) {
		TAILQ_INIT(&head);
		(new ANDMAP(other))->insert_tail(&head);
	};
	XORMAP(bool _value) {
		TAILQ_INIT(&head);
		if (_value == true)
			(new ANDMAP())->insert_tail(&head);
	};
	XORMAP(hpsat_var_t var, bool value) {
		TAILQ_INIT(&head);
		(new ANDMAP(var, value))->insert_tail(&head);
	};
	~XORMAP() {
		hpsat_free(&head);
	};
	XORMAP *dup(void) const {
		return (new XORMAP(*this));
	};
	hpsat_var_t maxVar(hpsat_var_t limit = HPSAT_VAR_MAX) const {
		return (hpsat_maxvar(&head, limit));
	};
	hpsat_var_t minVar(hpsat_var_t limit = HPSAT_VAR_MIN) const {
		return (hpsat_minvar(&head, limit));
	};
	hpsat_var_t findAndVar() const;
	hpsat_var_t findMaxUsedVariable() const;

	XORMAP & shiftVar(hpsat_var_t shift) {
		for (ANDMAP *pa = TAILQ_FIRST(&head); pa; pa = pa->next())
			pa->shiftVar(shift);
		return (*this);
	};
	bool isAtomic() const {
		const ANDMAP *pa = TAILQ_FIRST(&head);
		return (pa == 0 || pa->next() == 0);
	};
	bool isXorConst() const {
		const ANDMAP *pa = TAILQ_FIRST(&head);
		if (pa == 0)
			return (true);
		else if (pa != 0 && pa->next() == 0)
			return (pa->isXorConst());
		else
			return (false);
	};
	bool isXorConst(hpsat_var_t var) const {
		for (ANDMAP *pa = TAILQ_FIRST(&head); pa; pa = pa->next()) {
			if (pa->contains(var) && !pa->isXorConst())
				return (false);
		}
		return (true);
	};
	bool isOne() const {
		ANDMAP *pa = TAILQ_FIRST(&head);
		if (pa != 0 && pa->next() == 0)
			return (pa->isOne());
		else
			return (false);
	};
	bool isZero() const {
		return (TAILQ_FIRST(&head) == 0);
	};
	XORMAP & operator ^=(const XORMAP &other) {
		for (ANDMAP *pa = other.first(); pa; pa = pa->next())
			(new ANDMAP(*pa))->insert_tail(&head);
		return (sort());
	};
	XORMAP operator ^(const XORMAP &other) const {
		XORMAP temp(*this);
		temp ^= other;
		return (temp);
	};
	XORMAP & operator &=(const XORMAP &other) {
		*this = *this & other;
		return (*this);
	};
	XORMAP operator &(const XORMAP &other) const {
		XORMAP temp;
		for (ANDMAP *pa = first(); pa; pa = pa->next()) {
			for (ANDMAP *pb = other.first(); pb; pb = pb->next())
				(new ANDMAP(*pa & *pb))->insert_tail(&temp.head);
		}
		return (temp.sort());
	};
	XORMAP & operator |=(const XORMAP &other) {
		*this = (*this & other) ^ *this ^ other;
		return (*this);
	};
	XORMAP operator |(const XORMAP &other) const {
		return ((*this & other) ^ *this ^ other);
	};
	bool contains(hpsat_var_t var) const {
		for (ANDMAP *pa = TAILQ_FIRST(&head); pa; pa = pa->next()) {
			if (pa->contains(var))
				return (true);
		}
		return (false);
	};
	bool contains(const ANDMAP &var, bool isXorConst = false) const {
		/* check for special case */
		if (isXorConst) {
			hpsat_var_t v = var.maxVar();
			if (v != HPSAT_VAR_MIN) {
				for (ANDMAP *pa = TAILQ_FIRST(&head); pa; pa = pa->next()) {
					if (pa->isXorConst() == false)
						continue;
					BITMAP *ba = pa->first();
					if (ba != 0 && ba->contains(v))
						return (true);
				}
			}
			return (false);
		}

		/* fallback logic */
		for (ANDMAP *pa = TAILQ_FIRST(&head); pa; pa = pa->next()) {
			if (*pa == var)
				return (true);
		}
		return (false);
	};
	XORMAP & defactor();
	XORMAP implication();
	XORMAP & substitute(hpsat_var_t var, const BITMAP &expr);
	XORMAP & substitute(hpsat_var_t var, const XORMAP &expr);
	XORMAP & operator =(const XORMAP &other) {
		if (this == &other)
			return (*this);
		hpsat_free(&head);
		for (ANDMAP *pa = TAILQ_FIRST(&other.head); pa; pa = pa->next())
			pa->dup()->insert_tail(&head);
		return (*this);
	};
	bool isXorAble(hpsat_var_t) const;
	const XORMAP & xored(const XORMAP &, hpsat_var_t, XORMAP_HEAD_t *) const;
	XORMAP & sort(bool byValue = false);

	BITMAP toBitmap() const {
		BITMAP temp(false);
		for (ANDMAP *pa = TAILQ_FIRST(&head); pa; pa = pa->next()) {
			temp ^= pa->toBitmap();
			temp.sort();
		}
		return (temp);
	};
	XORMAP & xorify();

	XORMAP & expand(hpsat_var_t var, uint8_t value) {
		for (ANDMAP *pa = TAILQ_FIRST(&head); pa; pa = pa->next())
			pa->expand(var, value);
		return (sort());
	};
	bool expand_all(const uint8_t *pval) const {
		bool retval = 0;
		for (ANDMAP *pa = TAILQ_FIRST(&head); pa; pa = pa->next())
			retval ^= pa->expand_all(pval);
		return (retval);
	};
	void print() const {
		if (isOne()) {
			printf("1");
		} else if (isZero()) {
			printf("0");
		} else for (ANDMAP *pa = TAILQ_FIRST(&head); pa; pa = pa->next()) {
			if (pa->isXorConst() == false)
				printf("(");
			pa->print();
			if (pa->isXorConst() == false)
				printf(")");
			if (pa->next())
				printf("^");
		  }
	};
	XORMAP & insert_before(XORMAP *pother) {
		TAILQ_INSERT_BEFORE(pother, this, entry);
		return (*this);
	};
	XORMAP & insert_tail(XORMAP_HEAD_t *phead) {
		TAILQ_INSERT_TAIL(phead, this, entry);
		return (*this);
	};
	XORMAP & insert_head(XORMAP_HEAD_t *phead) {
		TAILQ_INSERT_HEAD(phead, this, entry);
		return (*this);
	};
	XORMAP *remove(XORMAP_HEAD_t *phead){
#ifdef DEBUG
		for (XORMAP *pa = TAILQ_FIRST(phead); pa != this; pa = pa->next())
			assert(pa);
#endif
		TAILQ_REMOVE(phead, this, entry);
		return (this);
	};
	XORMAP *prev() const {
		return (TAILQ_PREV(this, XORMAP_HEAD, entry));
	};
	XORMAP *next() const {
		return (TAILQ_NEXT(this, entry));
	};
	ANDMAP *first(void) const {
		return (TAILQ_FIRST(&head));
	};
	ANDMAP *last(void) const {
		return (TAILQ_LAST(&head, ANDMAP_HEAD));
	};
	int compare(const XORMAP & other) const {
		const size_t na = count();
		const size_t nb = other.count();

		if (na > nb)
			return (1);
		else if (na < nb)
			return (-1);

		const ANDMAP *pa = TAILQ_FIRST(&head);
		const ANDMAP *pb = TAILQ_FIRST(&other.head);

		while (pa && pb) {
			const int ret = pa->compare(*pb);
			if (ret)
				return (ret);
			pa = pa->next();
			pb = pb->next();
		}
		return ((pa != 0) - (pb != 0));
	};
	bool operator >(const XORMAP & other) const {
		return (compare(other) > 0);
	};
	bool operator <(const XORMAP & other) const {
		return (compare(other) < 0);
	};
	bool operator >=(const XORMAP & other) const {
		return (compare(other) >= 0);
	};
	bool operator <=(const XORMAP & other) const {
		return (compare(other) <= 0);
	};
	bool operator == (const XORMAP & other) const {
		return (compare(other) == 0);
	};
	bool operator !=(const XORMAP & other) const {
		return (compare(other) != 0);
	};
	size_t count() const {
		size_t retval = 0;
		for (ANDMAP *pa = TAILQ_FIRST(&head); pa; pa = pa->next())
			retval++;
		return (retval);
	};
};

extern hpsat_var_t hpsat_maxvar(const XORMAP_HEAD_t *, hpsat_var_t = HPSAT_VAR_MAX);
extern hpsat_var_t hpsat_minvar(const XORMAP_HEAD_t *, hpsat_var_t = HPSAT_VAR_MIN);
extern size_t hpsat_numvar(const XORMAP_HEAD_t *, hpsat_var_t = HPSAT_VAR_MAX, BITMAP * = 0);
extern void hpsat_demux(XORMAP_HEAD_t *, hpsat_var_t *);
extern void hpsat_bitmap_to_xormap(const BITMAP_HEAD_t *, XORMAP_HEAD_t *);
extern bool hpsat_sort_or(XORMAP_HEAD_t *);
extern void hpsat_free(XORMAP_HEAD_t *);
extern void hpsat_merge(XORMAP_HEAD_t *);
extern void hpsat_find_ored(XORMAP_HEAD_t *);
extern void hpsat_find_anded(XORMAP_HEAD_t *);

/* solve functions */

typedef bool (hpsat_solve_callback_t)(void *, uint8_t *);

extern bool hpsat_solve(XORMAP_HEAD_t *, XORMAP_HEAD_t *, hpsat_var_t);
extern void hpsat_solve_strip(XORMAP_HEAD_t *, XORMAP_HEAD_t *, hpsat_var_t, hpsat_var_t);
extern bool hpsat_solve_first(XORMAP_HEAD_t *, uint8_t *);
extern size_t hpsat_solve_count(XORMAP_HEAD_t *, uint8_t *);
extern bool hpsat_solve_callback(XORMAP *, uint8_t *, hpsat_solve_callback_t *, void *);
extern void hpsat_underiv(XORMAP_HEAD_t *, XORMAP_HEAD_t *);

/* simplify functions */

extern void hpsat_simplify_or(ANDMAP_HEAD_t *, ANDMAP_HEAD_t * = 0);
extern void hpsat_simplify_or(XORMAP_HEAD_t *);
extern bool hpsat_simplify_deriv(XORMAP_HEAD_t *, XORMAP_HEAD_t * = 0);
extern bool hpsat_simplify_mixed(XORMAP_HEAD_t *, XORMAP_HEAD_t *, BITMAP_HEAD_t *, hpsat_var_t, hpsat_var_t);
extern bool hpsat_simplify_all(XORMAP_HEAD_t *, XORMAP_HEAD_t *, hpsat_var_t);
extern bool hpsat_simplify_xormap(XORMAP_HEAD_t *, XORMAP_HEAD_t * = 0);
extern void hpsat_simplify_defactor(XORMAP_HEAD_t *);
extern bool hpsat_simplify_defactor(ANDMAP_HEAD_t *);
extern bool hpsat_simplify_factor(XORMAP_HEAD_t *, ANDMAP_HEAD_t *);
extern void hpsat_simplify_factor(XORMAP_HEAD_t *);
extern bool hpsat_simplify_find_zeros(XORMAP_HEAD_t *);
extern bool hpsat_simplify_join(XORMAP_HEAD_t *);
extern void hpsat_simplify_split(BITMAP &, BITMAP &, BITMAP &);
extern bool hpsat_simplify_assignments(XORMAP_HEAD_t *, BITMAP_HEAD_t *, const BITMAP &, bool);
extern bool hpsat_simplify_symmetry(XORMAP_HEAD_t *, XORMAP_HEAD_t *, bool);
extern bool hpsat_simplify_variable_map(XORMAP_HEAD_t *, BITMAP_HEAD_t *);

/* explore path functions */

extern size_t hpsat_find_longest_path(XORMAP_HEAD_t *, hpsat_var_t, hpsat_var_t, hpsat_var_t *);
extern size_t hpsat_find_shortest_path(XORMAP_HEAD_t *, hpsat_var_t, hpsat_var_t, hpsat_var_t *);

/* compress functions */

extern void hpsat_compress(XORMAP_HEAD_t *, ANDMAP_HEAD_t *);

/* logic builder classes and functions */

class variable_t {
public:
	BITMAP v;

	variable_t(void) {};
	variable_t(const BITMAP &other) : v(other) {};
	variable_t &operator =(const variable_t &other);
	bool isInverted(void) const;
	void toggleInverted(void);
	void equal_to_const(bool) const;
	void equal_to_var(const variable_t &other) const;

	variable_t operator ~(void) const;
	variable_t operator &(const variable_t &) const;
	variable_t &operator &=(const variable_t &);
	variable_t operator ^(const variable_t &) const;
	variable_t &operator ^=(const variable_t &);
	variable_t operator |(const variable_t &) const;
	variable_t &operator |=(const variable_t &);
};

class var_t {
public:
	variable_t *z;

	var_t(void);
	var_t(const var_t &);
	~var_t(void);

	var_t &operator =(const var_t &);
	var_t &alloc(size_t max = 0, bool is_signed = false);
	var_t &from_const(uint64_t);
	var_t &equal_to_const(bool);
	var_t &equal_to_var(const var_t &);

	var_t operator ~(void) const;
	var_t operator ^(const var_t &) const;
	var_t &operator ^=(const var_t &);
	var_t operator ^(const variable_t &) const;
	var_t operator &(const var_t &) const;
	var_t &operator &=(const var_t &);
	var_t operator &(const variable_t &) const;
	var_t operator |(const var_t &) const;
	var_t &operator |=(const var_t &);
	var_t operator |(const variable_t &) const;
	var_t operator <<(size_t) const;
	var_t &operator <<=(size_t);
	var_t operator >>(size_t) const;
	var_t &operator >>=(size_t);
	var_t operator +(const var_t &) const;

	var_t &operator +=(const var_t &);
	var_t operator -(const var_t &) const;
	var_t &operator -=(const var_t &);
	var_t operator *(const var_t &) const;
	var_t &operator *=(const var_t &);

	var_t operator %(const var_t &) const;
	var_t &operator %=(const var_t &);
	variable_t operator >(const var_t &) const;
	variable_t operator >=(const var_t &) const;
	variable_t operator <(const var_t &) const;
	variable_t operator <=(const var_t &) const;
};

extern void hpsat_logic_builder_init(size_t);
extern void hpsat_logic_builder_finish(void);
extern variable_t hpsat_logic_builder_new_variable(void);
extern BITMAP_HEAD_t *hpsat_logic_builder_equation(void);
extern size_t hpsat_logic_builder_maxvar(void);

#endif					/* _HPSAT_H_ */
