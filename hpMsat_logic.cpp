/*-
 * Copyright (c) 2023 Hans Petter Selasky
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

#include <pthread.h>

struct vstate {
	hpMsat_var_t n;
	EQ_HEAD_t head;
};

static pthread_key_t hpMsat_vstate_key;

static void __attribute__((constructor))
hpMsat_vstate_key_init()
{
	pthread_key_create(&hpMsat_vstate_key, NULL);
}

void
hpMsat_logic_init(hpMsat_var_t v)
{
	vstate *pvs = new vstate;

	TAILQ_INIT(&pvs->head);
	pvs->n = v;

	pthread_setspecific(hpMsat_vstate_key, pvs);
}

void
hpMsat_logic_concat(EQ_HEAD_t *phead)
{
	vstate *pvs = (vstate *)pthread_getspecific(hpMsat_vstate_key);
	TAILQ_CONCAT(phead, &pvs->head, entry);
}

void
hpMsat_logic_uninit()
{
	vstate *pvs = (vstate *)pthread_getspecific(hpMsat_vstate_key);

	hpMsat_free(&pvs->head);
	pthread_setspecific(hpMsat_vstate_key, 0);
	delete pvs;
}

void
hpMsat_logic_get_var(V *pv, hpMsat_var_t num)
{
	vstate *pvs = (vstate *)pthread_getspecific(hpMsat_vstate_key);

	assert(num < HPMSAT_VAR_MAX);
	assert(pvs->n < HPMSAT_VAR_MAX - num);

	while (num--) {
		pv->var = pvs->n;
		pv->neg = false;
		pv++;
		pvs->n += 1;
	}
}

/*
 * (a + b - c - 2 * d) MOD 3 = 0
 *
 *  a  b  c  d
 * ============
 *  0  0  0  0
 *  1  0  1  0
 * -1  0 -1  0
 *  0  1  1  0
 *  1  1  0  1
 * -1  1  0  0
 *  0 -1 -1  0
 *  1 -1  0  0
 * -1 -1  0 -1
 *
 * The function above works like a half adder using a signed bit-result {c,d}:
 * ============
 * c = a XOR b
 * d = a AND b
 */

void
hpMsat_logic_build(const V &a, const V &b, V &c, V &d)
{
	vstate *pvs = (vstate *)pthread_getspecific(hpMsat_vstate_key);
	EQ *pa = new EQ();

	assert(a.var != 0);
	assert(b.var != 0);

	*pa += EQ(VAR(a.var, a.neg ? -1 : 1));
	*pa += EQ(VAR(b.var, b.neg ? -1 : 1));

	*pa += EQ(VAR(pvs->n, -1));
	*pa += EQ(VAR(pvs->n + 1, -2));

	pa->insert_tail(&pvs->head);

	c.var = pvs->n;
	c.neg = false;

	d.var = pvs->n + 1;
	d.neg = false;

	assert(pvs->n < HPMSAT_VAR_MAX - 2);
	pvs->n += 2;
}

void
hpMsat_logic_adder(const V &a, const V &b, const V &c, V &d, V &e)
{
	V t[4];

	hpMsat_logic_build(a,b,t[0],t[1]);
	hpMsat_logic_build(t[0],c,d,t[2]);
	hpMsat_logic_build(t[1],t[2],e,t[3]);

	t[3].set_equal_to(0);
}

V &
V :: set_equal_to(const V &other)
{
	vstate *pvs = (vstate *)pthread_getspecific(hpMsat_vstate_key);
	EQ *pa = new EQ();

	*pa += EQ(VAR(var, neg ? -1 : 1));
	*pa -= EQ(VAR(other.var, other.neg ? -1 : 1));

	pa->insert_tail(&pvs->head);
	return (*this);
}

V &
V :: set_equal_to(hpMsat_val_t value)
{
	vstate *pvs = (vstate *)pthread_getspecific(hpMsat_vstate_key);
	EQ *pa = new EQ();

	*pa += EQ(VAR(0,value));
	*pa -= EQ(VAR(var, neg ? -1 : 1));

	pa->insert_tail(&pvs->head);
	return (*this);
}
