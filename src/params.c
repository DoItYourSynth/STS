/*
 * params.c
 *
 *  Created on: Mar 27, 2015
 *      Author: design
 */


#include "globals.h"
#include "adc.h"
#include "dig_pins.h"
#include "params.h"
#include "buttons.h"
#include "sampler.h"
#include "wav_recording.h"
#include "rgb_leds.h"

#include "equal_pow_pan_padded.h"
#include "voltoct.h"
#include "log_taper_padded.h"
#include "pitch_pot_cv.h"
#include "buttons.h"
#include "edit_mode.h"
#include "calibration.h"

extern float pitch_pot_cv[4096];
const float voltoct[4096];


extern enum PlayStates play_state[NUM_PLAY_CHAN];

extern __IO uint16_t potadc_buffer[NUM_POT_ADCS];
extern __IO uint16_t cvadc_buffer[NUM_CV_ADCS];

extern Sample samples[MAX_NUM_BANKS][NUM_SAMPLES_PER_BANK];

extern uint8_t disable_mode_changes;

volatile float 	f_param[NUM_PLAY_CHAN][NUM_F_PARAMS];
uint8_t i_param[NUM_ALL_CHAN][NUM_I_PARAMS];
uint8_t settings[NUM_ALL_CHAN][NUM_CHAN_SETTINGS];

float	global_param[NUM_GLOBAL_PARAMS];
uint32_t global_i_param[NUM_GLOBAL_I_PARAMS];
uint8_t	global_mode[NUM_GLOBAL_MODES];

uint8_t flags[NUM_FLAGS];
uint8_t flag_pot_changed[NUM_POT_ADCS];

extern uint8_t recording_enabled;


 /*** Move to adc.c interrupts ***/
int32_t MIN_POT_ADC_CHANGE[NUM_POT_ADCS];
int32_t MIN_CV_ADC_CHANGE[NUM_CV_ADCS];

extern int16_t CV_CALIBRATION_OFFSET[NUM_CV_ADCS];

float POT_LPF_COEF[NUM_POT_ADCS];
float CV_LPF_COEF[NUM_CV_ADCS];

//Low Pass filtered adc values:
float smoothed_potadc[NUM_POT_ADCS];
float smoothed_cvadc[NUM_CV_ADCS];

//Integer-ized low pass filtered adc values:
int16_t i_smoothed_potadc[NUM_POT_ADCS];
int16_t i_smoothed_cvadc[NUM_CV_ADCS];

int16_t old_i_smoothed_cvadc[NUM_CV_ADCS];
int16_t old_i_smoothed_potadc[NUM_POT_ADCS];

float smoothed_rawcvadc[NUM_CV_ADCS];
int16_t i_smoothed_rawcvadc[NUM_CV_ADCS];

//Change in pot since last process_adc
int32_t pot_delta[NUM_POT_ADCS];
int32_t cv_delta[NUM_CV_ADCS];

/* end move to adc.c */


void init_params(void)
{
	uint8_t chan,i;

	set_default_calibration_values();

	for (chan=0;chan<NUM_PLAY_CHAN;chan++){
		f_param[chan][PITCH] 	= 1.0;
		f_param[chan][START] 	= 0.0;
		f_param[chan][LENGTH] 	= 1.0;

		f_param[chan][TRACKING_COMP] = 1.00;

		i_param[chan][BANK] 		= 0;
		i_param[chan][SAMPLE] 	= 0;
		i_param[chan][REV] 		= 0;
		i_param[chan][STEREO_MODE]=0;
		i_param[chan][LOOPING]	 =0;
	}

	i_param[REC][BANK] = 0;
	i_param[REC][SAMPLE] = 0;

	global_param[SLOW_FADE_INCREMENT] = 0.001;
	global_i_param[LED_BRIGHTNESS] = 2;

	for (i=0;i<NUM_FLAGS;i++)
	{
		flags[i]=0;
	}
}

//initializes modes that aren't read from flash ram
void init_modes(void)
{
	global_mode[CALIBRATE] = 0;
	global_mode[SYSTEM_SETTINGS] = 0;
	global_mode[MONITOR_RECORDING] = 0;
	global_mode[ENABLE_RECORDING] = 0;
	global_mode[STEREO_LINK] = 1;
	global_mode[EDIT_MODE] = 0;

}



void init_LowPassCoefs(void)
{
	float t;
	uint8_t i;

	t=10.0;

	CV_LPF_COEF[PITCH_CV*2] = 1.0-(1.0/t);
	CV_LPF_COEF[PITCH_CV*2+1] = 1.0-(1.0/t);

	t=50.0;

	CV_LPF_COEF[START_CV*2] = 1.0-(1.0/t);
	CV_LPF_COEF[START_CV*2+1] = 1.0-(1.0/t);

	CV_LPF_COEF[LENGTH_CV*2] = 1.0-(1.0/t);
	CV_LPF_COEF[LENGTH_CV*2+1] = 1.0-(1.0/t);

	CV_LPF_COEF[SAMPLE_CV*2] = 1.0-(1.0/t);
	CV_LPF_COEF[SAMPLE_CV*2+1] = 1.0-(1.0/t);

	MIN_CV_ADC_CHANGE[PITCH_CV*2] = 15;
	MIN_CV_ADC_CHANGE[PITCH_CV*2+1] = 15;

	MIN_CV_ADC_CHANGE[START_CV*2] = 20;
	MIN_CV_ADC_CHANGE[START_CV*2+1] = 20;

	MIN_CV_ADC_CHANGE[LENGTH_CV*2] = 20;
	MIN_CV_ADC_CHANGE[LENGTH_CV*2+1] = 20;

	MIN_CV_ADC_CHANGE[SAMPLE_CV*2] = 20;
	MIN_CV_ADC_CHANGE[SAMPLE_CV*2+1] = 20;

	t=20.0; //50.0 = about 100ms to turn a knob fully

	POT_LPF_COEF[PITCH_POT*2] = 1.0-(1.0/t);
	POT_LPF_COEF[PITCH_POT*2+1] = 1.0-(1.0/t);

	POT_LPF_COEF[START_POT*2] = 1.0-(1.0/t);
	POT_LPF_COEF[START_POT*2+1] = 1.0-(1.0/t);

	POT_LPF_COEF[LENGTH_POT*2] = 1.0-(1.0/t);
	POT_LPF_COEF[LENGTH_POT*2+1] = 1.0-(1.0/t);

	POT_LPF_COEF[SAMPLE_POT*2] = 1.0-(1.0/t);
	POT_LPF_COEF[SAMPLE_POT*2+1] = 1.0-(1.0/t);

	POT_LPF_COEF[RECSAMPLE_POT] = 1.0-(1.0/t);



	MIN_POT_ADC_CHANGE[PITCH_POT*2] = 40;
	MIN_POT_ADC_CHANGE[PITCH_POT*2+1] = 40;

	MIN_POT_ADC_CHANGE[START_POT*2] = 10;
	MIN_POT_ADC_CHANGE[START_POT*2+1] = 10;

	MIN_POT_ADC_CHANGE[LENGTH_POT*2] = 10;
	MIN_POT_ADC_CHANGE[LENGTH_POT*2+1] = 10;

	MIN_POT_ADC_CHANGE[SAMPLE_POT*2] = 60;
	MIN_POT_ADC_CHANGE[SAMPLE_POT*2+1] = 60;

	MIN_POT_ADC_CHANGE[RECSAMPLE_POT] = 60;


	for (i=0;i<NUM_POT_ADCS;i++)
	{
		smoothed_potadc[i]		=0;
		old_i_smoothed_potadc[i]=0;
		i_smoothed_potadc[i]	=0x7FFF;
		pot_delta[i]			=0;
	}
	for (i=0;i<NUM_CV_ADCS;i++)
	{
		smoothed_cvadc[i]		=0;
		smoothed_rawcvadc[i]	=0;
		old_i_smoothed_cvadc[i]	=0;
		i_smoothed_cvadc[i]		=0x7FFF;
		i_smoothed_rawcvadc[i]	=0x7FFF;
		cv_delta[i]				=0;
	}
}


void process_adc(void)
{
	uint8_t i;
	int32_t t;

	static uint32_t track_moving_pot[NUM_POT_ADCS]={0,0,0,0,0,0,0,0,0};

	//
	// Run a LPF on the pots and CV jacks
	//
	for (i=0;i<NUM_POT_ADCS;i++)
	{
		flag_pot_changed[i]=0;

		smoothed_potadc[i] = LowPassSmoothingFilter(smoothed_potadc[i], (float)potadc_buffer[i], POT_LPF_COEF[i]);
		i_smoothed_potadc[i] = (int16_t)smoothed_potadc[i];

		t=i_smoothed_potadc[i] - old_i_smoothed_potadc[i];
		if ((t>MIN_POT_ADC_CHANGE[i]) || (t<-MIN_POT_ADC_CHANGE[i]))
			track_moving_pot[i]=250;

		if (track_moving_pot[i])
		{
			track_moving_pot[i]--;
			flag_pot_changed[i]=1;
			pot_delta[i] = t;
			old_i_smoothed_potadc[i] = i_smoothed_potadc[i];
		}
	}


	for (i=0;i<NUM_CV_ADCS;i++)
	{
		smoothed_cvadc[i] = LowPassSmoothingFilter(smoothed_cvadc[i], (float)(cvadc_buffer[i]+CV_CALIBRATION_OFFSET[i]), CV_LPF_COEF[i]);
		i_smoothed_cvadc[i] = (int16_t)smoothed_cvadc[i];
		if (i_smoothed_cvadc[i] < 0) i_smoothed_cvadc[i] = 0;
		if (i_smoothed_cvadc[i] > 4095) i_smoothed_cvadc[i] = 4095;

		if (global_mode[CALIBRATE])
		{
			smoothed_rawcvadc[i] = LowPassSmoothingFilter(smoothed_rawcvadc[i], (float)(cvadc_buffer[i]), CV_LPF_COEF[i]);
			i_smoothed_rawcvadc[i] = (int16_t)smoothed_rawcvadc[i];
			if (i_smoothed_rawcvadc[i] < 0) i_smoothed_rawcvadc[i] = 0;
			if (i_smoothed_rawcvadc[i] > 4095) i_smoothed_rawcvadc[i] = 4095;
		}

		t=i_smoothed_cvadc[i] - old_i_smoothed_cvadc[i];
		if ((t>MIN_CV_ADC_CHANGE[i]) || (t<-MIN_CV_ADC_CHANGE[i]))
		{
			cv_delta[i] = t;
			old_i_smoothed_cvadc[i] = i_smoothed_cvadc[i];
		}
	}

}




void update_params(void)
{
	uint8_t chan;
	uint8_t old_val;
	uint8_t new_val;
	//uint8_t ok_sampleslot;
	float t_f;
	float t_fine=0, t_coarse=0;
	//uint32_t t_32;
	uint8_t samplenum, banknum;

	recording_enabled=1;

	//
	// Edit mode
	//
	if (global_mode[EDIT_MODE])
	{
		samplenum = i_param[0][SAMPLE];
		banknum = i_param[0][BANK];

		//
		// Trim Size 
		// 
		if (flag_pot_changed[LENGTH_POT*2+0])
		{
			t_coarse 	 = old_i_smoothed_potadc[LENGTH_POT*2+0] / 4096.0;
			set_sample_trim_size(&samples[banknum][samplenum], t_coarse);

			flag_pot_changed[LENGTH_POT*2+0] = 0;

			f_param[0][START] = 0.999f;
			f_param[0][LENGTH] = 0.501f;
			i_param[0][LOOPING] = 1;
			i_param[0][REV] = 0;
			if (play_state[0] == SILENT) flags[Play1Trig] = 1;

		}

		if (flag_pot_changed[LENGTH_POT*2+1])
		{
			nudge_trim_size(&samples[banknum][samplenum], pot_delta[LENGTH_POT*2+1]);

			flag_pot_changed[LENGTH_POT*2+1] = 0;
			//pot_delta[LENGTH_POT*2+1] = 0;

			f_param[0][START] = 0.999f;
			f_param[0][LENGTH] = 0.201f;
			i_param[0][LOOPING] = 1;
			i_param[0][REV] = 0;
			if (play_state[0] == SILENT) flags[Play1Trig] = 1;

		}
		

		//
		// Trim Start
		// 
		if (flag_pot_changed[START_POT*2+0])
		{
			t_coarse 	 = old_i_smoothed_potadc[START_POT*2+0] / 4096.0;
			set_sample_trim_start(&samples[banknum][samplenum], t_coarse, 0);
			flag_pot_changed[START_POT*2+0] = 0;

			f_param[0][START] = 0.000f;
			f_param[0][LENGTH] = 0.201f;
			i_param[0][LOOPING] = 1;
			i_param[0][REV] = 0;
			if (play_state[0] == SILENT) flags[Play1Trig] = 1;
		}

		if (flag_pot_changed[START_POT*2+1])
		{
			nudge_trim_start(&samples[banknum][samplenum], pot_delta[START_POT*2+1]);
			flag_pot_changed[START_POT*2+0] = 0;
			//pot_delta[START_POT*2+1] = 0;

			f_param[0][START] = 0.000f;
			f_param[0][LENGTH] = 0.201f;
			i_param[0][LOOPING] = 1;
			i_param[0][REV] = 0;
			if (play_state[0] == SILENT) flags[Play1Trig] = 1;
		}

	//	clear_is_buffered_to_file_end(0);
	//	check_trim_bounds();

		//
		// Gain (sample ch2 pot): 
		// was: 0.1 to 2.1 with just the knob (jack disabled)
		// 0.1 to 5.1 with just the knob (jack disabled)
		//
		if (flag_pot_changed[SAMPLE_POT*2+1])
		{
		//	t_f 	= (old_i_smoothed_potadc[SAMPLE_POT*2+1] + 204.8f) / 2048.0f;
			t_f 	= (old_i_smoothed_potadc[SAMPLE_POT*2+1] + 81.92f) / 819.2f;
			set_sample_gain(&samples[banknum][samplenum], t_f);
		}

		//
		// PITCH POT
		//

		if ((old_i_smoothed_cvadc[PITCH_CV*2+0] < 2038) || (old_i_smoothed_cvadc[PITCH_CV*2+0] > 2058)) //positive voltage on 1V/oct jack
		{
			f_param[0][PITCH] = pitch_pot_cv[i_smoothed_potadc[PITCH_POT*2+0]] * voltoct[old_i_smoothed_cvadc[PITCH_CV*2+0]];
		}
		else
			f_param[0][PITCH] = pitch_pot_cv[i_smoothed_potadc[PITCH_POT*2+0]];

		if (f_param[0][PITCH] > MAX_RS)
			f_param[0][PITCH] = MAX_RS;

		//
		// SAMPLE POT + CV
		//
		old_val = i_param[0][SAMPLE];
		new_val = detent_num( old_i_smoothed_potadc[SAMPLE_POT*2+0] + old_i_smoothed_cvadc[SAMPLE_CV*2+0] );

		if (old_val != new_val)
		{
			i_param[0][SAMPLE] = new_val;
			flags[PlaySample1Changed] = 1;

			if (samples[ i_param[0][BANK] ][ new_val ].filename[0] == 0) //not a valid sample
				flags[PlaySample1Changed_empty] = 6;
			else
				flags[PlaySample1Changed_valid] = 6;
		}

	


	} //if not EDIT_MODE
	else 
	{

		for (chan=0;chan<2;chan++)
		{
			//
			// LENGTH POT + CV
			//
			f_param[chan][LENGTH] 	= (old_i_smoothed_potadc[LENGTH_POT*2+chan] + old_i_smoothed_cvadc[LENGTH_CV*2+chan]) / 4096.0;

			if (f_param[chan][LENGTH] > 1.0)			f_param[chan][LENGTH] = 1.0;
			if (f_param[chan][LENGTH] <= 0.000244)	f_param[chan][LENGTH] = 0.000244;


			//
			// START POT + CV
			//
			f_param[chan][START] 	= (old_i_smoothed_potadc[START_POT*2+chan] + old_i_smoothed_cvadc[START_CV*2+chan]) / 4096.0;

			if (f_param[chan][START] > 1.0)
				f_param[chan][START] = 1.0;

			//
			// PITCH POT + CV
			//

			if ((old_i_smoothed_cvadc[PITCH_CV*2+chan] < 2038) || (old_i_smoothed_cvadc[PITCH_CV*2+chan] > 2058)) //positive voltage on 1V/oct jack
			{
				f_param[chan][PITCH] = pitch_pot_cv[i_smoothed_potadc[PITCH_POT*2+chan]] * voltoct[old_i_smoothed_cvadc[PITCH_CV*2+chan]];
				//if (chan==1) f_param[chan][PITCH] *= 1.009227524f;
			}
			else
				f_param[chan][PITCH] = pitch_pot_cv[i_smoothed_potadc[PITCH_POT*2+chan]];

			if (f_param[chan][PITCH] > MAX_RS)
				f_param[chan][PITCH] = MAX_RS;

			//
			// SAMPLE POT + CV
			//
			old_val = i_param[chan][SAMPLE];
			new_val = detent_num( old_i_smoothed_potadc[SAMPLE_POT*2+chan] + old_i_smoothed_cvadc[SAMPLE_CV*2+chan] );

			if (old_val != new_val)
			{
				i_param[chan][SAMPLE] = new_val;
				flags[PlaySample1Changed + chan*2] = 1;

				if (samples[ i_param[chan][BANK] ][ new_val ].filename[0] == 0) //not a valid sample
					flags[PlaySample1Changed_empty + chan] = 6;
				else
					flags[PlaySample1Changed_valid + chan] = 6;
			}

		} //for chan


		//
		// REC SAMPLE POT
		//
		old_val = i_param[REC][SAMPLE];
		new_val = detent_num(i_smoothed_potadc[RECSAMPLE_POT]);

		if (old_val != new_val)
		{
			i_param[REC][SAMPLE] = new_val;
			flags[RecSampleChanged] = 1;
			if (global_mode[MONITOR_RECORDING])
			{
				flags[RecSampleChanged_light] = 10;
				//?? DO THIS???
				//play the sample selected by rec bank/sample
				//have to override i_param[0][BANK] and [SAMPLE] and all params (pitch, length...)
			}
		}

	} //else if EDIT_MODE
}


//
// Handle all flags to change modes
//
void process_mode_flags(void)
{
	DEBUG0_ON;

	if (!disable_mode_changes)
	{


		if (flags[Rev1Trig])
		{
			flags[Rev1Trig]=0;

			// if (global_mode[STEREO_LINK] && i_param[0][REV]!=i_param[1][REV])
			// 	i_param[1][REV] = i_param[0][REV];

			toggle_reverse(0);

			// if (global_mode[STEREO_LINK])
			// 	toggle_reverse(1);

		}
		if (flags[Rev2Trig])
		{

			flags[Rev2Trig]=0;

			// if (global_mode[STEREO_LINK] && i_param[0][REV]!=i_param[1][REV])
			// 	i_param[0][REV] = i_param[1][REV];

			toggle_reverse(1);

			// if (global_mode[STEREO_LINK])
			// 	toggle_reverse(0);

		}



		if (flags[Play1But])
		{
			flags[Play1But]=0;

			// if (global_mode[STEREO_LINK])
			// {
			// 	play_state[1] = play_state[0];
			// 	//if (play_state[0]==SILENT && (play_state[1]== PLAYING || play_state[1] == PLAYING_PERC)) play_state[1]=SILENT;
			// 	if (i_param[0][REV]!=i_param[1][REV])
			// 		toggle_reverse(0);

			// 	i_param[1][LOOPING] = i_param[0][LOOPING];

			// 	toggle_playing(0);
			// 	toggle_playing(1);
			// }
			// else
				toggle_playing(0);
		}

		if (flags[Play2But])
		{
			flags[Play2But]=0;

			// if (global_mode[STEREO_LINK])
			// {
			// 	play_state[0] = play_state[1];
			// 	if (i_param[0][REV]!=i_param[1][REV])
			// 		toggle_reverse(1);

			// 	toggle_playing(0);
			// 	toggle_playing(1);

			// 	i_param[0][LOOPING] = i_param[1][LOOPING];
			// }
			// else
				toggle_playing(1);

			//if (play_state[1]==SILENT && (play_state[0]== PLAYING || play_state[0] == PLAYING_PERC)) play_state[1]=SILENT;
		}

		if (flags[Play1Trig])
		{
			flags[Play1Trig] = 0;
			start_playing(0);
		}
		
		if (flags[Play2Trig])
		{
			flags[Play2Trig] = 0;
			start_playing(1);
		}


		if (flags[RecTrig]==1)
		{
			flags[RecTrig]=0;
			toggle_recording();
		}

		if (flags[ToggleMonitor])
		{
			flags[ToggleMonitor] = 0;

			if (global_mode[ENABLE_RECORDING] && global_mode[MONITOR_RECORDING])
			{
				global_mode[ENABLE_RECORDING] = 0;
				global_mode[MONITOR_RECORDING] = 0;
				stop_recording();
			}
			else
			{
				global_mode[ENABLE_RECORDING] = 1;
				global_mode[MONITOR_RECORDING] = 1;
				i_param[0][LOOPING] = 0;
				i_param[1][LOOPING] = 0;
			}
		}


		if (flags[ToggleLooping1])
		{
			flags[ToggleLooping1] = 0;

			if (i_param[0][LOOPING])
			{
				i_param[0][LOOPING] = 0;

				// if (global_mode[STEREO_LINK])
				// 	i_param[1][LOOPING] = 0;
			}
			else
			{
				i_param[0][LOOPING] = 1;
				if (play_state[0] == SILENT) 
					flags[Play1But] = 1;

				// if (global_mode[STEREO_LINK])
				// {
				// 	i_param[1][LOOPING] = 1;
				// 	flags[Play2But] = 1;
				// }

			}


		}

		if (flags[ToggleLooping2])
		{
			flags[ToggleLooping2] = 0;

			// if (!global_mode[STEREO_LINK])
			// {
				if (i_param[1][LOOPING])
				{
					i_param[1][LOOPING] = 0;
				}
				else
				{
					i_param[1][LOOPING] = 1;
					if (play_state[1] == SILENT) 
						flags[Play2But] = 1;
				}
			// }
		}
	}
	DEBUG0_OFF;
}

uint8_t detent_num(uint16_t adc_val)
{
	if (adc_val<=212)
		return(0);
	else if (adc_val<=625)
		return(1);
	else if (adc_val<=1131)
		return(2);
	else if (adc_val<=1562)
		return(3);
	else if (adc_val<=1995)
		return(4);
	else if (adc_val<=2475)
		return(5);
	else if (adc_val<=2825)
		return(6);
	else if (adc_val<=3355)
		return(7);
	else if (adc_val<=3840)
		return(8);
	else
		return(9);
}


//uint8_t detent_num(uint16_t adc_val)
//{
//	if (adc_val<=91)
//		return(0);
//	else if (adc_val<=310)
//		return(1);
//	else if (adc_val<=565)
//		return(2);
//	else if (adc_val<=816)
//		return(3);
//	else if (adc_val<=1062)
//		return(4);
//	else if (adc_val<=1304)
//		return(5);
//	else if (adc_val<=1529)
//		return(6);
//	else if (adc_val<=1742)
//		return(7);
//	else if (adc_val<=1950)
//		return(8);
//	else if (adc_val<=2157) // Center
//		return(9);
//	else if (adc_val<=2365)
//		return(10);
//	else if (adc_val<=2580)
//		return(11);
//	else if (adc_val<=2806)
//		return(12);
//	else if (adc_val<=3044)
//		return(13);
//	else if (adc_val<=3289)
//		return(14);
//	else if (adc_val<=3537)
//		return(15);
//	else if (adc_val<=3790)
//		return(16);
//	else if (adc_val<=4003)
//		return(17);
//	else
//		return(18);
//
//}



void adc_param_update_IRQHandler(void)
{

	if (TIM_GetITStatus(TIM9, TIM_IT_Update) != RESET) {


		process_adc();

		if (global_mode[CALIBRATE])
		{
			//update_calibration();
			//update_calibrate_leds();
		}
		else
			update_params();

		//process_mode_flags();

		if (global_mode[SYSTEM_SETTINGS])
		{
			//update_system_settings();
			//update_system_settings_leds();
		}


		//check_entering_system_mode();


		TIM_ClearITPendingBit(TIM9, TIM_IT_Update);

	}
}

