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
constexpr hpsat_var_t HPSAT_VAR_ST_MAX = 2;
constexpr hpsat_var_t HPSAT_VAR_MAX = SSIZE_MAX - (SSIZE_MAX % HPSAT_VAR_ST_MAX); /* exclusive */
constexpr hpsat_var_t HPSAT_VAR_MIN = -HPSAT_VAR_ST_MAX;		/* exclusive */

class BITMAP {
public:
	BITMAP() {
		state = HPSAT_VAR_MIN;
	};
	BITMAP(const BITMAP &other) {
		state = other.state;
	};
	BITMAP(hpsat_var_t _state) {
		state = _state;
	};
	BITMAP *dup(void) const {
		return (new BITMAP(*this));
	};

	hpsat_var_t state;

	hpsat_var_t getSt() const {
		return (state % HPSAT_VAR_ST_MAX);
	};
	hpsat_var_t getNo() const {
		return (state - (state % HPSAT_VAR_ST_MAX));
	};
	bool contains(hpsat_var_t _state) const {
		return (getNo() == (_state - (_state % HPSAT_VAR_ST_MAX)));
	};
	BITMAP & operator =(const BITMAP &other) {
		if (this == &other)
			return (*this);
		state = other.state;
		return (*this);
	};
	bool expand_all(const uint8_t *pval) const {
		return (pval[getNo() / HPSAT_VAR_ST_MAX] == getSt());
	};
	void print(std::ostream &out = std::cout) const {
		out << "v" << (getNo() / HPSAT_VAR_ST_MAX) << "=" << getSt();
	};

	int compare(const BITMAP & other) const {
		const hpsat_var_t a = state;
		const hpsat_var_t b = other.state;

		if (a > b)
			return (1);
		else if (a < b)
			return (-1);
		else
			return (0);
	};

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
};

class ANDMAP;
typedef TAILQ_CLASS_HEAD(ANDMAP_HEAD, ANDMAP) ANDMAP_HEAD_t;
typedef TAILQ_CLASS_ENTRY(ANDMAP) ANDMAP_ENTRY_t;

class ANDMAP {
public:
	ANDMAP_ENTRY_t entry;
	BITMAP bitmaps[2];
	uint8_t nummaps;

	ANDMAP() {
		nummaps = 2;
		bitmaps[0].state = 0;
		bitmaps[1].state = 1;
	};
	ANDMAP(const ANDMAP &other) {
		*this = other;
	};
	ANDMAP(const BITMAP &other) {
		bitmaps[0] = other;
		nummaps = 1;
	};
	ANDMAP(hpsat_var_t var) {
		bitmaps[0] = BITMAP(var);
		nummaps = 1;
	};
	ANDMAP *dup(void) const {
		return (new ANDMAP(*this));
	};
	bool isOne() const {
		return (nummaps == 0);
	};
	bool isZero() const {
		if (nummaps != 2)
			return (false);
		if (bitmaps[1].getNo() != bitmaps[0].getNo())
			return (false);
		return (bitmaps[1].getSt() != bitmaps[0].getSt());
	};
	hpsat_var_t maxVar(hpsat_var_t limit = HPSAT_VAR_MAX) const {
		for (uint8_t x = nummaps; x--; ) {
			if (bitmaps[x].getNo() < limit)
				return (bitmaps[x].getNo());
		}
		return (HPSAT_VAR_MIN);
	};
	hpsat_var_t minVar(hpsat_var_t limit = HPSAT_VAR_MIN) const {
		for (uint8_t x = 0; x != nummaps; x++) {
			if (bitmaps[x].getNo() > limit)
				return (bitmaps[x].getNo());
		}
		return (HPSAT_VAR_MIN);
	};
	bool contains(hpsat_var_t var) const {
		for (uint8_t x = 0; x != nummaps; x++) {
			if (bitmaps[x].contains(var))
				return (true);
		}
		return (false);
	};
	ANDMAP & operator =(const ANDMAP &other) {
		if (this == &other)
			return (*this);
		bitmaps[0] = other.bitmaps[0];
		bitmaps[1] = other.bitmaps[1];
		nummaps = other.nummaps;
		return (*this);
	};
	ANDMAP & operator &=(const ANDMAP &other) {
		ANDMAP temp;
		uint8_t x = 0;
		uint8_t y = 0;

		temp.nummaps = 0;

		/* keep it sorted */
		while (x != nummaps && y != other.nummaps) {
			if (bitmaps[x] > other.bitmaps[y]) {
				temp.bitmaps[temp.nummaps++] = other.bitmaps[y++];
			} else if (bitmaps[x] < other.bitmaps[y]) {
				temp.bitmaps[temp.nummaps++] = bitmaps[x++];
			} else {
				temp.bitmaps[temp.nummaps++] = bitmaps[x];
				x++;
				y++;
			}
		}
		while (x != nummaps)
			temp.bitmaps[temp.nummaps++] = bitmaps[x++];
		while (y != other.nummaps)
			temp.bitmaps[temp.nummaps++] = other.bitmaps[y++];

		*this = temp;

		return (*this);
	};
	ANDMAP operator &(const ANDMAP &other) const {
		ANDMAP temp(*this);
		temp &= other;
		return (temp);
	};
	ANDMAP & expand(hpsat_var_t var, uint8_t value) {
		for (uint8_t x = 0; x != nummaps; ) {
			if (bitmaps[x].contains(var) == false) {
				x++;
				continue;
			}
			if (bitmaps[x].getSt() != value) {
				nummaps = 1;
				bitmaps[0].state = 0;
				bitmaps[1].state = 1;
				return (*this);
			}
			for (uint8_t y = x + 1; y != nummaps; y++)
				bitmaps[y - 1] = bitmaps[y];
			nummaps--;
		}
		return (*this);
	};
	bool expand_all(const uint8_t *pval) const {
		bool retval = 1;
		for (uint8_t x = 0; x != nummaps; x++)
			retval &= bitmaps[x].expand_all(pval);
		return (retval);
	};
	void print(std::ostream &out = std::cout) const {
		if (isOne()) {
			out << "true";
		} else if (isZero()) {
			out << "false";
		} else {
			for (uint8_t x = 0; x != nummaps; x++) {
				out << "(";
				bitmaps[x].print(out);
				out << ")";
				if (x + 1 != nummaps)
					out << "&";
			}
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
	int compare(const ANDMAP & other) const {
		uint8_t x;

		if (nummaps > other.nummaps)
			return (1);
		else if (nummaps < other.nummaps)
			return (-1);

		for (x = 0; x != nummaps; x++) {
			const int ret = bitmaps[x].compare(other.bitmaps[x]);
			if (ret)
				return (ret);
		}
		return (0);
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
};

extern hpsat_var_t hpsat_maxvar(const ANDMAP_HEAD_t *, hpsat_var_t = HPSAT_VAR_MAX);
extern hpsat_var_t hpsat_minvar(const ANDMAP_HEAD_t *, hpsat_var_t = HPSAT_VAR_MIN);
extern size_t hpsat_numvar(const ANDMAP_HEAD_t *, hpsat_var_t = HPSAT_VAR_MAX, BITMAP * = 0);
extern bool hpsat_sort_or(ANDMAP_HEAD_t *);
extern void hpsat_free(ANDMAP_HEAD_t *);

/* CNF functions */

extern int hpsat_loadcnf(std::istream &, ANDMAP_HEAD_t *, hpsat_var_t * = 0, hpsat_var_t * = 0);

/* solve functions */

typedef bool (hpsat_solve_callback_t)(void *, uint8_t *);

extern bool hpsat_solve(ANDMAP_HEAD_t *, ANDMAP_HEAD_t *, hpsat_var_t);
extern void hpsat_solve_strip(ANDMAP_HEAD_t *, ANDMAP_HEAD_t *, hpsat_var_t, hpsat_var_t);
extern bool hpsat_solve_first(ANDMAP_HEAD_t *, uint8_t *);
extern size_t hpsat_solve_count(ANDMAP_HEAD_t *, uint8_t *);
extern bool hpsat_solve_callback(ANDMAP *, uint8_t *, hpsat_solve_callback_t *, void *);

#endif					/* _HPSAT_H_ */
