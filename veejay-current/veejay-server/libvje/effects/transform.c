 /* 
  * Linux VeeJay
  *
  * Copyright(C)2002 Niels Elburg <nwelburg@gmail.com>
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
#include "transform.h"

vj_effect *transform_init(int width,int height)
{
    vj_effect *ve = (vj_effect *) vj_calloc(sizeof(vj_effect));
    ve->num_params = 1;
    ve->defaults = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* default values */
    ve->limits[0] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* min */
    ve->limits[1] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* max */
    ve->defaults[0] = 5;
    ve->limits[0][0] = 1;
    ve->limits[1][0] = (height / 16);

    ve->description = "Transform Cubics";
    ve->sub_format = -1;
    ve->extra_frame = 0;
   	ve->has_user = 0;
	ve->param_description = vje_build_param_list(ve->num_params, "Cubics");
    return ve;
}

void transform_apply(void *ptr, VJFrame *frame, int *args)
{
    unsigned int ty, tx, y, x;
    const unsigned int uv_height = frame->uv_height;
    const unsigned int uv_width = frame->uv_width;
	const unsigned int width = frame->width;
	const unsigned int height = frame->height;
	uint8_t *Y = frame->data[0];
	uint8_t *Cb= frame->data[1];
	uint8_t *Cr= frame->data[2];
    uint8_t *Y2 = frame->data[0];
 	uint8_t *Cb2= frame->data[1];
	uint8_t *Cr2= frame->data[2];
    int size = args[0];

    /* Luminance */
    for (y = 1; y < height; y++)
	{
		ty = y % size - (size >> 1);
		if ((y / size) % 2)
		{
	   		ty = y - ty;
		}
		else
		{
	    	ty = y + ty;
		}
		if (ty < 0)
		    ty = 0;

		if (ty >= height)
		    ty = height - 1;

		for (x = 1; x < width; x++)
		{
	   		tx = x % size - (size >> 1);
	    	if ((x / size) % 2)
			{
				tx = x - tx;
	    	}
			else
			{
				tx = x + tx;
		    }
		    if (tx < 0)
				tx = 0;
		    if (tx >= width)
				tx = width - 1;
		    Y[x + (y * width)] = Y2[(ty * width) + tx];
	}
    }
    /* Chroma */
    for (y = 1; y < uv_height; y++)
	{
		ty = y % size - (size >> 1);
		if ((y / size) % 2) {
		    ty = y - ty;
		}
		else
		{
	   	 ty = y + ty;
		}
		if (ty < 0)
		    ty = 0;

		if (ty >= uv_height)
		    ty = uv_height - 1;

		for (x = 1; x < uv_width; x++)
		{
		    tx = x % size - (size >> 1);
		    if ((x / size) % 2)
			{
				tx = x - tx;
	   		}
			else
			{
				tx = x + tx;
		    }
	   		if (tx < 0)
				tx = 0;
		    if (tx >= uv_width)
				tx = uv_width - 1;
	  	  	Cb[x + (y * uv_width)] = Cb2[(ty * uv_width) + tx];
	    	Cr[x + (y * uv_width)] = Cr2[(ty * uv_width) + tx];
		}
    }
}
