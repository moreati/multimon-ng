/*
 *      demod_googletone.c -- Google Tone demodulator
 *
 *      Copyright (C) 1996, 2015
 *          Thomas Sailer (sailer@ife.ee.ethz.ch, hb9jnx@hb9w.che.eu)
 *          Alex Willmer (alex@moreati.org.uk)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* ---------------------------------------------------------------------- */

#include "multimon.h"
#include "filter.h"
#include <math.h>
#include <string.h>

#define SAMPLE_RATE 22050
#define BLOCKLEN (SAMPLE_RATE/100)  /* 10ms blocks */
#define BLOCKNUM 4    /* must match numbers in multimon.h */

#define PHINC(x) ((x)*0x10000/SAMPLE_RATE)

static const unsigned int googletone_phinc[10] = {
	PHINC(740), PHINC(831), PHINC(933), PHINC(1109), PHINC(1245),
	PHINC(1480), PHINC(1661), PHINC(1865), PHINC(2217), PHINC(2489)
};

/* ---------------------------------------------------------------------- */
	
static void googletone_init(struct demod_state *s)
{
	memset(&s->l1.googletone, 0, sizeof(s->l1.googletone));
}

/* ---------------------------------------------------------------------- */

static int googletone_find_max_idx(const float *f, int ignore_idx)
{
	float en = 0;
	int idx = -1, i;

	for (i = 0; i < 10; i++)
		if (i != ignore_idx && f[i] > en) {
			en = f[i];
			idx = i;
		}
	if (idx < 0)
		return -1;
	if (ignore_idx >= 0) {
		en *= 0.1;
		for (i = 0; i < 10; i++)
			if (i != ignore_idx && idx != i && f[i] > en)
				return -2;
	}
	return idx;
}

/* ---------------------------------------------------------------------- */

static inline int process_block(struct demod_state *s)
{
	float tote;
	float totte[20];
	int i, j;

	tote = 0;
	for (i = 0; i < BLOCKNUM; i++)
		tote += s->l1.googletone.energy[i];
	for (i = 0; i < 20; i++) {
		totte[i] = 0;
		for (j = 0; j < BLOCKNUM; j++)
			totte[i] += s->l1.googletone.tenergy[j][i];
	}
	for (i = 0; i < 10; i++)
		totte[i] = fsqr(totte[i]) + fsqr(totte[i+10]);
	memmove(s->l1.googletone.energy+1, s->l1.googletone.energy, 
		sizeof(s->l1.googletone.energy) - sizeof(s->l1.googletone.energy[0]));
	s->l1.googletone.energy[0] = 0;
	memmove(s->l1.googletone.tenergy+1, s->l1.googletone.tenergy, 
		sizeof(s->l1.googletone.tenergy) - sizeof(s->l1.googletone.tenergy[0]));
	memset(s->l1.googletone.tenergy, 0, sizeof(s->l1.googletone.tenergy[0]));
	tote *= (BLOCKNUM*BLOCKLEN*0.5);  /* adjust for block lengths */
	verbprintf(10, "GOOGLETONE: Energies: %8.5f  %8.5f %8.5f %8.5f %8.5f %8.5f %8.5f %8.5f %8.5f %8.5f %8.5f\n",
		   tote,
		   totte[0], totte[1], totte[2], totte[3], totte[4],
		   totte[5], totte[6], totte[7], totte[8], totte[9]);
	i = googletone_find_max_idx(totte, -1);
	if (i < 0 || i >= 10) {
		verbprintf(10, "GOOGLETONE: i: %i\n", i);
		return -1;
	}
	j = googletone_find_max_idx(totte, i);
	if (j < 0) {
		verbprintf(10, "GOOGLETONE: i j: %i %i\n", i, j);
		return -1;
	}
	if ((tote * 0.4) > (totte[i] + totte[j])) {
		verbprintf(10, "GOOGLETONE: i j totte[i] totte[j] threshold: %i %i %8.5f %8.5f %8.5f\n",
		   i, j, totte[i], totte[j], tote * 0.2);
		return -1;
	}
	if (i > j) {
		return (i & 0x0f) | ((j << 4) & 0xf0);
	} else {
		return (j & 0x0f) | ((i << 4) & 0xf0);
	}
}

/* ---------------------------------------------------------------------- */

static void googletone_demod(struct demod_state *s, buffer_t buffer, int length)
{
	float s_in;
	int i;

	for (; length > 0; length--, buffer.fbuffer++) {
		s_in = *buffer.fbuffer;
		s->l1.googletone.energy[0] += fsqr(s_in);
		for (i = 0; i < 10; i++) {
			s->l1.googletone.tenergy[0][i] += COS(s->l1.googletone.ph[i]) * s_in;
			s->l1.googletone.tenergy[0][i+10] += SIN(s->l1.googletone.ph[i]) * s_in;
			s->l1.googletone.ph[i] += googletone_phinc[i];
		}
		if ((s->l1.googletone.blkcount--) <= 0) {
			s->l1.googletone.blkcount = BLOCKLEN;
			i = process_block(s);
			if (i != s->l1.googletone.lastch && i >= 0)
				verbprintf(0, "GOOGLETONE: %i %i\n", (i >> 4) & 0xf, i & 0xf);
			s->l1.googletone.lastch = i;
		}
	}
}
				
/* ---------------------------------------------------------------------- */

const struct demod_param demod_googletone = {
    "GOOGLETONE", true, SAMPLE_RATE, 0, googletone_init, googletone_demod, NULL
};

/* ---------------------------------------------------------------------- */
