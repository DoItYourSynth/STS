#include "wavefmt.h"

uint8_t is_valid_wav_header(WaveHeader sample_header)
{
	if (	sample_header.RIFFId 		!= ccRIFF			//'RIFF'
			|| sample_header.fileSize		 < 16			//File size - 8
			|| sample_header.WAVEId 		!= ccWAVE		//'WAVE'
		)
		return 0;
	else
		return 1;

}

uint8_t is_valid_format_chunk(WaveFmtChunk fmt_chunk)
{
	if (	fmt_chunk.fmtId 			== ccFMT				//'fmt '
			&& fmt_chunk.fmtSize 		>= 16					//Format Chunk size is valid

			&&	(
					(		fmt_chunk.audioFormat	== 0x0001	//PCM format and 16/24-bit
					&& (	fmt_chunk.bitsPerSample	== 16 	
						||	fmt_chunk.bitsPerSample	== 8 
						||	fmt_chunk.bitsPerSample	== 24
						||	fmt_chunk.bitsPerSample	== 32
						)
					)
				||
					(	fmt_chunk.audioFormat	== 0x0003		//Float format and 32-bit
					&&	fmt_chunk.bitsPerSample	== 32
					)
				||
					(	fmt_chunk.audioFormat	== 0xFFFE		//Extended format (float) format and 32-bit
					&&	fmt_chunk.bitsPerSample	== 32
					)
				)

			&&	(	fmt_chunk.numChannels 	 == 0x0002
				||	fmt_chunk.numChannels 	 == 0x0001)

			&&	fmt_chunk.sampleRate 	<= 96000				//Between 8k and 96k sampling rate allowed
			&&	fmt_chunk.sampleRate	>= 8000

		)
		return (1);
	else
		return (0);
}

void create_waveheader(WaveHeader *w, WaveFmtChunk *f)
{
	w->RIFFId		= ccRIFF;
	w->fileSize 	= sizeof(WaveHeaderAndChunk) - 8; //size of file, minus 8 for 'RIFFsize'
	w->WAVEId 		= ccWAVE;

	f->fmtId 		= ccFMT;
	f->fmtSize		= 16;
	f->audioFormat	= 1;
	f->numChannels	= 2;
	f->sampleRate	= 44100;
	f->byteRate		= (44100 * 2 * 2); //sampleRate * blockAlign
	f->blockAlign	= 4; //numChannels * bitsPerSample/8
	f->bitsPerSample= 16;
}

void create_chunk(uint32_t chunkId, uint32_t chunkSize, WaveChunk *wc)
{
	wc->chunkId	= chunkId;
	wc->chunkSize = chunkSize;

}
