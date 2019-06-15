/* 
 * Linux VeeJay
 *
 * EffecTV - Realtime Digital Video Effector
 * Copyright (C) 2001-2006 FUKUCHI Kentaro
 *
 * BaltanTV - like StreakTV, but following for a long time
 * Copyright (C) 2001-2002 FUKUCHI Kentaro
 * Ported to veejay by Niels Elburg 
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307 , USA.
 */

#include "common.h"
#include <veejaycore/vjmem.h>
#include "baltantv.h"

#define PLANES 32

vj_effect *baltantv_init(int w, int h)
{
    vj_effect *ve = (vj_effect *) vj_calloc(sizeof(vj_effect));
    ve->num_params = 2;
    ve->defaults = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* default values */
    ve->limits[0] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* min */
    ve->limits[1] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* max */
    ve->limits[0][0] = 2;
    ve->limits[1][0] = PLANES;
    ve->limits[0][1] = 0;
    ve->limits[1][1] = 1;
    ve->defaults[0] = 8;
    ve->defaults[1] = 0;
    ve->description = "BaltanTV (EffecTV)";
    ve->sub_format = -1;
    ve->extra_frame = 0;
    ve->has_user = 0;
	ve->param_description = vje_build_param_list( ve->num_params, "Stride", "Mode" );


	ve->hints = vje_init_value_hint_list( ve->num_params );

	vje_build_value_hint_list( ve->hints, ve->limits[1][1], 1, "Decaying", "Normal" );


    return ve;
}

static	unsigned int	plane_ = 0;
static	uint8_t	*planetableY_ = NULL;
static	int8_t	*planetableU_ = NULL;
static	int8_t	*planetableV_ = NULL;


int	baltantv_malloc(int w, int h)
{
	planetableY_ = (uint8_t*) vj_malloc( sizeof(uint8_t*) * PLANES * RUP8(w * h));
	planetableU_ = (int8_t*) vj_malloc( sizeof(int8_t*) * PLANES * RUP8(w * h));
	planetableV_ = (int8_t*) vj_malloc( sizeof(int8_t*) * PLANES * RUP8(w * h));

    veejay_memset( planetableY_, 0, PLANES * RUP8(w*h));
    veejay_memset( planetableU_, 0, PLANES * RUP8(w*h));
    veejay_memset( planetableV_, 0, PLANES * RUP8(w*h));

	if(!planetableY_ || !planetableU_ || !planetableV_) {
		veejay_msg(0,"Not enough memory to allocate %d planes of size %dx%d", PLANES,w,h);
		baltantv_free();
        return 0;
	}

	return 1;
}

void	baltantv_free()
{
	plane_ = 0;
    if(planetableY_) {
        free(planetableY_);
        planetableY_ = NULL;
    }
    if(planetableU_) {
        free(planetableU_);
        planetableU_ = NULL;
    }
    if(planetableV_) {
        free(planetableV_);
        planetableV_ = NULL;
    }
}

void baltantv_apply( VJFrame *frame, int stride, int mode)
{
	unsigned int i,cf;
	const int len = frame->len;
    const int uv_len = frame->uv_len;
	uint8_t *Y = frame->data[0];
    uint8_t *U = frame->data[1];
    uint8_t *V = frame->data[2];
	uint8_t *pDstY = planetableY_ + (plane_ * len);
    int8_t *pDstU = planetableU_ + (plane_ * uv_len);
    int8_t *pDstV = planetableV_ + (plane_ * uv_len);
    uint32_t y;
    int32_t u,v;

	for( i = 0; i < len ; i ++ ) {
		pDstY[i] = (Y[i] >> 2 );
    }
    for( i = 0; i < uv_len; i ++ ) {
        pDstU[i] = ((U[i]-128)) >> 2;
        pDstV[i] = ((V[i]-128)) >> 2;
    }

	cf = plane_ & (stride-1);

	uint8_t *pSrcY[4] = {
			planetableY_ + (cf * len),
			planetableY_ + ((cf+stride)   * len),
			planetableY_ + ((cf+stride*2) * len),
			planetableY_ + ((cf+stride*3) * len)
		};
    int8_t *pSrcU[4] = {
			planetableU_ + (cf * uv_len),
			planetableU_ + ((cf+stride)   * uv_len),
			planetableU_ + ((cf+stride*2) * uv_len),
			planetableU_ + ((cf+stride*3) * uv_len)
		};
	int8_t *pSrcV[4] = {
			planetableV_ + (cf * uv_len),
			planetableV_ + ((cf+stride)   * uv_len),
			planetableV_ + ((cf+stride*2) * uv_len),
			planetableV_ + ((cf+stride*3) * uv_len)
		};


	if( mode == 0 )
	{
		for( i = 0; i < len; i ++ )
		{	
			y = pSrcY[0][i] + 
				pSrcY[1][i] + 
				pSrcY[2][i] + 
				pSrcY[3][i];
            Y[i] = (y>>2);
			pDstY[i] = (y >> 2 );
		}
        for( i = 0; i < uv_len; i ++ )
        {
            u = pSrcU[0][i] + 
				pSrcU[1][i] + 
				pSrcU[2][i] + 
				pSrcU[3][i];
            U[i] = 128 + (u>>2);
			pDstU[i] = (u >> 2 );
        }
        for( i = 0; i < uv_len; i ++ )
        {
            v = pSrcV[0][i] + 
				pSrcV[1][i] + 
				pSrcV[2][i] + 
				pSrcV[3][i];
            V[i] =128 + (v>>2);
			pDstV[i] = (v >> 2 );
        }
	}
	else
	{
		for( i = 0; i < len ; i++ )
		{
			Y[i] = (pSrcY[0][i] + 
				pSrcY[1][i] + 
				pSrcY[2][i] + 
				pSrcY[3][i]) >> 2;
		}
		for( i = 0; i < uv_len ; i++ )
		{
			U[i] = 128 +( (pSrcU[0][i] + 
				pSrcU[1][i] + 
				pSrcU[2][i] + 
				pSrcU[3][i]) >> 2);
		}
		for( i = 0; i < uv_len ; i++ )
		{
			V[i] = 128 + ((pSrcV[0][i] + 
				pSrcV[1][i] + 
				pSrcV[2][i] + 
				pSrcV[3][i]) >> 2);
		}

	}
	plane_ ++;

	plane_ = plane_ & (PLANES-1);
}
