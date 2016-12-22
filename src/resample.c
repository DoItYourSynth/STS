/*
 * resample.c
 *
 */

#include "globals.h"
#include "dig_pins.h"
#include "audio_sdram.h"
#include "audio_util.h"
#include "sampler.h"
#include "params.h"
#include "resample.h"

extern uint8_t flags[NUM_FLAGS];
extern enum g_Errors g_error;

extern uint32_t play_buff_out_addr[NUM_PLAY_CHAN];

void safe_inc_play_addr(uint8_t chan, uint32_t limit_addr, uint8_t blockAlign);

inline void safe_inc_play_addr(uint8_t chan, uint32_t limit_addr, uint8_t blockAlign)
{
	play_buff_out_addr[chan] = offset_addr(play_buff_out_addr[chan], chan, blockAlign);
	if (play_buff_out_addr[chan] == limit_addr)		{g_error |= (READ_BUFF1_OVERRUN<<chan);}
}

inline void safe_inc_num_play_addr(uint8_t chan, uint32_t limit_addr, uint8_t num)
{
	while (num--)
	{
		play_buff_out_addr[chan] = inc_addr(play_buff_out_addr[chan], chan, 0);
		if (play_buff_out_addr[chan] == limit_addr)		{g_error |= (READ_BUFF1_OVERRUN<<chan);break;}
	}
}


int16_t get_16b_sample(uint32_t addr, uint8_t stereomode)
{
	uint32_t r;
	int16_t a;
	int16_t b;

	r = memory_read_32bword(addr);

	if (stereomode==STEREO_SUM)
	{
		a = (int16_t)(r >> 16);
		b = (int16_t)(r & 0x0000FFFF);
		return ((a+b)/2);
	}
	if (stereomode==STEREO_RIGHT)
	{
		//return((int16_t)(r & 0x0000FFFF));
		return((int16_t)((r & 0xFFFF0000)>>16));
	}
	if (stereomode==MONO)
	{
		return (int16_t)(r);
	}
	//STEREO_LEFT or MONO

	return((int16_t)(r));

}

//ToDo: Optimize this by having a different resample_read function for stereo-16bit, mono-16bit, L+R-16bit, stereo-8bit, mono-8bit, L+R-8bit, etc...
//Then we also need a different memory_read_sample function and safe_inc_num_play_addr function
//This will save the "if" for every memory read, and the "while" in every safe_inc_num_play_addr

void resample_read16(float rs, uint32_t *input_buff_addr, uint32_t limit_addr, uint32_t buff_len, enum Stereo_Modes stereomode, uint8_t block_align, uint8_t chan, float *fractional_pos, int32_t *out)
{
	static float xm1[2], x0[2], x1[2], x2[2];
	float a,b,c;
	uint32_t outpos;

//	int16_t x[MAX_RS_READ_BUFF_LEN + 4];
//	uint32_t i;
//
//	for (i=0;i<(rs*buff_len + 4);i++)
//	{
//load all the memory efficiently by reading 32-bits once and splitting it into two 16-bit samples
//This really only helps in STEREO_SUM, 16-bit or 8-bit mode
//	}

	//fill the resampling buffer with three points
	if (flags[PlayBuff1_Discontinuity+chan])
	{
		flags[PlayBuff1_Discontinuity+chan] = 0;

		x0[chan] = get_16b_sample(input_buff_addr[chan], stereomode);
		safe_inc_play_addr(chan, limit_addr, block_align);

		x1[chan] = get_16b_sample(input_buff_addr[chan], stereomode);
		safe_inc_play_addr(chan, limit_addr, block_align);

		x2[chan] = get_16b_sample(input_buff_addr[chan], stereomode);
		safe_inc_play_addr(chan, limit_addr, block_align);

		fractional_pos[chan] = 0.0;
	}

	outpos=0;
	while (outpos < buff_len)
	{
		//Optimize for resample rates >= 4
		if (fractional_pos[chan] >= 4.0)
		{
			fractional_pos[chan] = fractional_pos[chan] - 4.0;

			//shift samples back one
			//and read a new sample
			xm1[chan] 	= get_16b_sample(input_buff_addr[chan], stereomode);
			safe_inc_play_addr(chan, limit_addr, block_align);

			x0[chan] 	= get_16b_sample(input_buff_addr[chan], stereomode);
			safe_inc_play_addr(chan, limit_addr, block_align);

			x1[chan] 	= get_16b_sample(input_buff_addr[chan], stereomode);
			safe_inc_play_addr(chan, limit_addr, block_align);

			x2[chan] 	= get_16b_sample(input_buff_addr[chan], stereomode);
			safe_inc_play_addr(chan, limit_addr, block_align);

		}
		//Optimize for resample rates >= 3
		if (fractional_pos[chan] >= 3.0)
		{
			fractional_pos[chan] = fractional_pos[chan] - 3.0;

			//shift samples back one
			//and read a new sample
			xm1[chan] 	= x2[chan];

			x0[chan] 	= get_16b_sample(input_buff_addr[chan], stereomode);
			safe_inc_play_addr(chan, limit_addr, block_align);

			x1[chan] 	= get_16b_sample(input_buff_addr[chan], stereomode);
			safe_inc_play_addr(chan, limit_addr, block_align);

			x2[chan] 	= get_16b_sample(input_buff_addr[chan], stereomode);
			safe_inc_play_addr(chan, limit_addr, block_align);

		}
		//Optimize for resample rates >= 2
		if (fractional_pos[chan] >= 2.0)
		{
			fractional_pos[chan] = fractional_pos[chan] - 2.0;

			//shift samples back one
			//and read a new sample
			xm1[chan] 	= x1[chan];
			x0[chan] 	= x2[chan];

			x1[chan] 	= get_16b_sample(input_buff_addr[chan], stereomode);
			safe_inc_play_addr(chan, limit_addr, block_align);

			x2[chan] 	= get_16b_sample(input_buff_addr[chan], stereomode);
			safe_inc_play_addr(chan, limit_addr, block_align);

		}
		//Optimize for resample rates >= 1
		if (fractional_pos[chan] >= 1.0)
		{
			fractional_pos[chan] = fractional_pos[chan] - 1.0;

			//shift samples back one
			//and read a new sample
			xm1[chan] 	= x0[chan];
			x0[chan] 	= x1[chan];
			x1[chan] 	= x2[chan];

			x2[chan] 	= get_16b_sample(input_buff_addr[chan], stereomode);
			safe_inc_play_addr(chan, limit_addr, block_align);

		}

		//calculate coefficients
		a = (3 * (x0[chan]-x1[chan]) - xm1[chan] + x2[chan]) / 2;
		b = 2*x1[chan] + xm1[chan] - (5*x0[chan] + x2[chan]) / 2;
		c = (x1[chan] - xm1[chan]) / 2;

		//calculate as many fractionally placed output points as we can
		while ( fractional_pos[chan] < 1.0 && outpos<buff_len)
		{
			out[outpos++] = (((a * fractional_pos[chan]) + b) * fractional_pos[chan] + c) * fractional_pos[chan] + x0[chan];
			fractional_pos[chan] += rs;
		}


	}

}

