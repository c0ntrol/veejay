/* 
 * Linux VeeJay
 *
 * Copyright(C)2002-2004 Niels Elburg <nelburg@looze.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License , or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 */
#ifndef VJ_DV_H
#define VJ_DV_H
#include <config.h>
#ifdef SUPPORT_READ_DV2
#include "vj-el.h"
#include <libdv/dv.h>
typedef struct
{
	dv_decoder_t	*decoder;
	uint8_t		*dv_video;
	int		fmt;
} vj_dv_decoder;

typedef struct
{
	dv_encoder_t	*encoder;
	uint8_t		*dv_video;
	int		fmt;
} vj_dv_encoder;


vj_dv_decoder *vj_dv_decoder_init(int width, int height, int pixel_format);

vj_dv_encoder *vj_dv_init_encoder(editlist * el, int pixel_format);

int vj_dv_decode_frame(vj_dv_decoder *d,uint8_t * in, uint8_t * Y,
		       uint8_t * Cb, uint8_t * Cr, int w, int h, int fmt);

int vj_dv_encode_frame(vj_dv_encoder *e,uint8_t * in[3], uint8_t * out);
void vj_dv_free_encoder(vj_dv_encoder *e);
void vj_dv_free_decoder(vj_dv_decoder *d); 

#endif
#endif
