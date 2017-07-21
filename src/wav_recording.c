#include "globals.h"
#include "params.h"
#include "wavefmt.h"
#include "timekeeper.h"
#include "audio_sdram.h"
#include "sdram_driver.h"
#include "ff.h"
#include "sampler.h"
#include "circular_buffer.h"
#include "file_util.h"
#include "rgb_leds.h"
#include "audio_util.h"
#include "wav_recording.h"
#include "sts_filesystem.h"
#include "dig_pins.h"
#include "bank.h"
#include "calibration.h"
#include "sts_fs_index.h"

extern volatile uint32_t sys_tmr;
extern enum g_Errors g_error;

extern const uint32_t AUDIO_MEM_BASE[4];
extern uint8_t SAMPLINGBYTES;

extern uint8_t	i_param[NUM_ALL_CHAN][NUM_I_PARAMS];
extern uint8_t	global_mode[NUM_GLOBAL_MODES];

extern uint8_t 	flags[NUM_FLAGS];

extern enum PlayLoadTriage play_load_triage;

extern Sample samples[MAX_NUM_BANKS][NUM_SAMPLES_PER_BANK];

extern SystemCalibrations *system_calibrations;

//#define WRITE_BLOCK_SIZE 8192
#define WRITE_BLOCK_SIZE 8192

//
// SDRAM buffer address for recording to sdcard
// Codec --> SDRAM (@rec_buff->in) .... SDRAM (@rec_buff->out) --> SD Card:recfil@rec_storage_addr
//

CircularBuffer srec_buff;
CircularBuffer* rec_buff;

enum RecStates	rec_state;
uint32_t		samplebytes_recorded;
uint8_t			sample_num_now_recording;
uint8_t			sample_bank_now_recording;
char 			sample_fname_now_recording[_MAX_LFN];

uint8_t 		recording_enabled;

FIL 			recfil;


void init_rec_buff(void)
{
	rec_buff = &srec_buff;
	rec_buff->in 		= AUDIO_MEM_BASE[REC_CHAN];
	rec_buff->out 		= AUDIO_MEM_BASE[REC_CHAN];
	rec_buff->min		= AUDIO_MEM_BASE[REC_CHAN];
	rec_buff->max		= AUDIO_MEM_BASE[REC_CHAN] + MEM_SIZE;
	rec_buff->size		= MEM_SIZE;
	rec_buff->wrapping	= 0;


}

void stop_recording(void)
{
	if (rec_state==RECORDING || rec_state==CREATING_FILE /* || rec_state==REC_PAUSED*/)
	{
		rec_state=CLOSING_FILE;
	}
}

void toggle_recording(void)
{
	if (rec_state==RECORDING || rec_state==CREATING_FILE)
	{
		rec_state=CLOSING_FILE;

	} else
	{
		if (global_mode[ENABLE_RECORDING])
		{
			CB_init(rec_buff, 0);
			rec_state = CREATING_FILE;
		}
	}
}

//int16_t tmp_buff16[HT16_BUFF_LEN<<1]; //1024 elements, 16b each

void record_audio_to_buffer(int16_t *src)
{
	uint32_t i;
	uint32_t overrun;

	//convenience vars:
	//uint16_t topbyte, bottombyte;
	int32_t dummy;

	//
	// Dump HT16_BUFF_LEN samples of the rx buffer from codec (src) into t_buff
	// Then write t_buff to sdram at rec_buff
	//
	if (rec_state==RECORDING || rec_state==CREATING_FILE)
	{
		overrun = 0;

		for (i=0; i<HT16_BUFF_LEN; i++)
		{
			//
			// Split incoming stereo audio into the two channels
			//
			if (SAMPLINGBYTES==2)
			{
				//The following block is essentially the same as:
				// overrun = memory_write16_cb(rec_buff, src, HT16_BUFF_LEN, 0);
				// but manually skipping every other *src so as to convert the codec's 24-bit into 16-bits
				// This also allows us to add codec_adc_calibration_dcoffset[]

				*((int16_t *)rec_buff->in) = *src++; // + system_calibrations->codec_adc_calibration_dcoffset[i&1];
				dummy=*src++; //ignore bottom bits

				while(SDRAM_IS_BUSY){;}

				CB_offset_in_address(rec_buff, 2, 0);

				if ((rec_buff->in == rec_buff->out)/* && i<(HT16_BUFF_LEN-1)*/) //don't consider the heads being crossed if they end at the same place
					overrun = rec_buff->out;
			}
			// else
			// {
			// 	topbyte 		= (uint16_t)(*src++);
			// 	bottombyte		= (uint16_t)(*src++);
			// 	tmp_buff16[i*2] 	= (topbyte << 16) + (uint16_t)bottombyte;

			// 	topbyte 		= (uint16_t)(*src++);
			// 	bottombyte 		= (uint16_t)(*src++);
			// 	tmp_buff16[i*2+1] = (topbyte << 16) + (uint16_t)bottombyte;

			// }

		}

		if (overrun)
		{
			g_error |= WRITE_BUFF_OVERRUN;
			check_errors();
		}
	}

}


int16_t rec_buff16[WRITE_BLOCK_SIZE>>1]; //4096 elements

void create_new_recording(void)
{
	uint32_t sz;
	FRESULT res;
	WaveHeaderAndChunk whac;
	uint32_t written;
	DIR dir;

	//Make a file with a temp name (tmp-XXXXX.wav), inside the temp dir

	//Open the temp directory
	res = f_opendir(&dir, TMP_DIR);

	//If it doesn't exist, create it
	if (res==FR_NO_PATH) res = f_mkdir(TMP_DIR);

	//If we got an error opening or creating a dir
	//try reloading the SDCard, then opening the dir (and creating if needed)
	if (res!=FR_OK)
	{
		res = reload_sdcard();
		if (res==FR_OK)
		{
			res = f_opendir(&dir, TMP_DIR);
			if (res==FR_NO_PATH) res = f_mkdir(TMP_DIR);
		}
	}


	//If we just can't open or create the tmp dir, just put it in the root dir
	if (res!=FR_OK)
		str_cpy(sample_fname_now_recording, "tmp-");
	
	else
		str_cat(sample_fname_now_recording, TMP_DIR_SLASH, "tmp-");

	sz = str_len(sample_fname_now_recording);
	sz += intToStr(sys_tmr, &(sample_fname_now_recording[sz]), 0);
	sample_fname_now_recording[sz++] = '.';
	sample_fname_now_recording[sz++] = 'w';
	sample_fname_now_recording[sz++] = 'a';
	sample_fname_now_recording[sz++] = 'v';
	sample_fname_now_recording[sz++] = 0;


	create_waveheader(&whac.wh, &whac.fc);
	create_chunk(ccDATA, 0, &whac.wc);

	sz = sizeof(WaveHeaderAndChunk);

	//Try to create the tmp file and write to it, reloading the sd card if needed

	res = f_open(&recfil, sample_fname_now_recording, FA_WRITE | FA_CREATE_NEW);
	if (res==FR_OK)
		res = f_write(&recfil, &whac.wh, sz, &written);

	if (res!=FR_OK)
	{
		f_close(&recfil);
		res = reload_sdcard();
		if (res == FR_OK)
		{
			res = f_open(&recfil, sample_fname_now_recording, FA_WRITE | FA_CREATE_NEW);
			f_sync(&recfil);
			res = f_write(&recfil, &whac.wh, sz, &written);
			f_sync(&recfil);
		}
		else {f_close(&recfil); rec_state=REC_OFF; g_error |= FILE_WRITE_FAIL; check_errors(); return;}
	}

	if (sz!=written)	{f_close(&recfil); rec_state=REC_OFF; g_error |= FILE_UNEXPECTEDEOF_WRITE; check_errors(); return;}

	samplebytes_recorded = 0;

	rec_state=RECORDING;

}

FRESULT write_wav_size(FIL *wavfil, uint32_t wav_data_bytes)
{
	uint32_t data;
	uint32_t orig_pos;
	uint32_t written;
	FRESULT res;

	//cache the original file position
	orig_pos = f_tell(wavfil);

	 //file size - 8
	data = wav_data_bytes + sizeof(WaveHeader) + sizeof(WaveChunk) - 8;
	res = f_lseek(wavfil, 4);
	if (res==FR_OK)
	{
		res = f_write(wavfil, &data, 4, &written);
		f_sync(wavfil);
	}

	if (res!=FR_OK) {g_error |= FILE_WRITE_FAIL; check_errors(); return(res);}
	if (written!=4)	{g_error |= FILE_UNEXPECTEDEOF_WRITE; check_errors(); return(FR_INT_ERR);}

	//data chunk size
	data = wav_data_bytes;
	res = f_lseek(wavfil, 40);
	if (res==FR_OK)
	{
		res = f_write(wavfil, &data, 4, &written);
		f_sync(wavfil);
	}

	if (res!=FR_OK) {g_error |= FILE_WRITE_FAIL; check_errors(); return(res);}
	if (written!=4)	{g_error |= FILE_UNEXPECTEDEOF_WRITE; check_errors(); return(FR_INT_ERR);}

	//restore the original file position
	res = f_lseek(wavfil, orig_pos);
	return(res);

}

void write_buffer_to_storage(void)
{
	uint32_t buffer_lead;
	uint32_t addr_exceeded;
	uint32_t written;


	FRESULT res;
	char final_filepath[_MAX_LFN];


	uint32_t sz;

	if (flags[RecSampleChanged])
	{
		//Currently, nothing happens if you change record sample slot
		flags[RecSampleChanged]=0;
	}

	if (flags[RecBankChanged])
	{
		//Currently, nothing happens if you change record bank
		flags[RecBankChanged]=0;
	}



	// Handle write buffers (transfer SDRAM to SD card)
	switch (rec_state)
	{
		case (CREATING_FILE):	//first time, create a new file
			if (recfil.obj.fs!=0)
			{
				rec_state = CLOSING_FILE;
				//f_close(&recfil);
				//f_sync(&recfil);
			}

			sample_num_now_recording = i_param[REC_CHAN][SAMPLE];
			sample_bank_now_recording = i_param[REC_CHAN][BANK];

			//flags[CreateTmpRecFile] = 1;
			create_new_recording();
			break;

		case (RECORDING):
		//read a block from rec_buff->out
			if (play_load_triage==0)
			{
				buffer_lead = CB_distance(rec_buff, 0);

				if (buffer_lead > WRITE_BLOCK_SIZE) //FixMe: comparing # samples to # bytes. Should be buffer_lead*SAMPLINGBYTES
				{

					addr_exceeded = memory_read16_cb(rec_buff, rec_buff16, WRITE_BLOCK_SIZE>>1, 0);

					if (!addr_exceeded)
					{
						sz = WRITE_BLOCK_SIZE;
						res = f_write(&recfil, rec_buff16, sz, &written);
						f_sync(&recfil);
						
						if (res!=FR_OK)		{if (g_error & FILE_WRITE_FAIL) {f_close(&recfil); rec_state=REC_OFF;} g_error |= FILE_WRITE_FAIL; check_errors();break;}
						if (sz!=written)	{g_error |= FILE_UNEXPECTEDEOF_WRITE; check_errors();}
						samplebytes_recorded += written;

						//Update the wav file size in the wav header
						res = write_wav_size(&recfil, samplebytes_recorded);
						if (res!=FR_OK)		{if (g_error & FILE_WRITE_FAIL) {f_close(&recfil); rec_state=REC_OFF;} g_error |= FILE_WRITE_FAIL; check_errors(); break;}


					}
					else {g_error |= WRITE_BUFF_OVERRUN; check_errors();}
						
				}
			}

		break;

		case (CLOSING_FILE):
			//See if we have more in the buffer to write
			buffer_lead = CB_distance(rec_buff, 0);

			if (buffer_lead)
			{
				//Write out remaining data in buffer, one WRITE_BLOCK_SIZE at a time
				if (buffer_lead > WRITE_BLOCK_SIZE) buffer_lead = WRITE_BLOCK_SIZE;

				addr_exceeded = memory_read16_cb(rec_buff, rec_buff16, buffer_lead>>1, 0);

				if (!addr_exceeded)
				{
					res = f_write(&recfil, rec_buff16, buffer_lead, &written);
					f_sync(&recfil);

					if (res!=FR_OK)				{if (g_error & FILE_WRITE_FAIL) {f_close(&recfil); rec_state=REC_OFF;} g_error |= FILE_WRITE_FAIL; check_errors(); break;}
					if (written!=buffer_lead)	{g_error |= FILE_UNEXPECTEDEOF_WRITE; check_errors();}

					samplebytes_recorded += written;
					//Update the wav file size in the wav header
					res = write_wav_size(&recfil, samplebytes_recorded);
					if (res!=FR_OK)		{f_close(&recfil); rec_state=REC_OFF; g_error |= FILE_WRITE_FAIL; check_errors(); break;}

				}
				else
				{
					g_error = 999; //math error
					check_errors();
				}
			}
			else 
			{
				//Write the file size into the header, and close the file
				res = write_wav_size(&recfil, samplebytes_recorded);
				f_close(&recfil);

				//Rename the tmp file as the proper file in the proper directory
				res = new_filename(sample_bank_now_recording, sample_num_now_recording, final_filepath);
				if (res != FR_OK)
				{
					rec_state=REC_OFF;
					g_error |= SDCARD_CANT_MOUNT;
				}
				else
				{
					res = f_rename(sample_fname_now_recording, final_filepath);
					if (res==FR_OK)
						str_cpy(sample_fname_now_recording, final_filepath);
				}


				str_cpy(samples[sample_bank_now_recording][sample_num_now_recording].filename, sample_fname_now_recording);
				samples[sample_bank_now_recording][sample_num_now_recording].sampleSize = samplebytes_recorded;
				samples[sample_bank_now_recording][sample_num_now_recording].sampleByteSize = 2;
				samples[sample_bank_now_recording][sample_num_now_recording].sampleRate = 44100;
				samples[sample_bank_now_recording][sample_num_now_recording].numChannels = 2;
				samples[sample_bank_now_recording][sample_num_now_recording].blockAlign = 4;
				samples[sample_bank_now_recording][sample_num_now_recording].startOfData = 44;
				samples[sample_bank_now_recording][sample_num_now_recording].PCM = 1;
				samples[sample_bank_now_recording][sample_num_now_recording].file_found = 1;

				samples[sample_bank_now_recording][sample_num_now_recording].inst_start = 0;
				samples[sample_bank_now_recording][sample_num_now_recording].inst_end = samplebytes_recorded & 0xFFFFFFF8;
				samples[sample_bank_now_recording][sample_num_now_recording].inst_size = samplebytes_recorded & 0xFFFFFFF8;
				samples[sample_bank_now_recording][sample_num_now_recording].inst_gain = 1.0f;

				enable_bank(sample_bank_now_recording);

				sample_fname_now_recording[0] = 0;
				sample_num_now_recording = 0xFF;
				sample_bank_now_recording = 0xFF;
				flags[ForceFileReload1] = 1;
				flags[ForceFileReload2] = 1;

				rec_state=REC_OFF;
			}

		break;


		case (REC_OFF):
			// if (recfil.obj.fs!=0)
			// {
			// 	//rec_state = CLOSING_FILE;
			// 	f_close(&recfil);
			// }
		break;

		// case (REC_PAUSED):
		// break;

	}


}
