/*
 * Audio Overload SDK
 *
 * Audio driver for PortAudio
 *
 * Author: feydor
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "ao.h"
#include "m1sdr.h"
#include "portaudio.h"

#define VALGRIND 	(0)

// local variables
int nDSoundSegLen = 0; // Seg length in samples (calculated from Rate/Fps)
static PaStream *stream;
static PaError err;
static stereo_sample_t samples[AUDIO_RATE];
static stereo_sample_t data;


// set # of samples per update
void m1sdr_SetSamplesPerTick(UINT32 spf)
{
	nDSoundSegLen = spf;
}

// m1sdr_Update - timer callback routine: runs sequencer and mixes sound
void m1sdr_Update(void)
{
	if (!hw_present) return;

	if (m1sdr_Callback)
	{
		m1sdr_Callback(nDSoundSegLen, samples);
	}
}

/* This routine will be called by the PortAudio engine when audio is needed.
** It may called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int patestCallback(const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData)
{
    /* Cast data passed through stream to our structure. */
    stereo_sample_t *data = (stereo_sample_t*)userData;
    int16 *out = (int16*)outputBuffer;
    (void) inputBuffer;

    m1sdr_Update();

    for(unsigned int i=0; i<framesPerBuffer; i++ )
    {
        *out++ = samples[i].l;
        *out++ = samples[i].r;
    }
    return 0;
}

// checks the play position to see if we should trigger another update
m1sdr_ret_t m1sdr_TimeCheck(void)
{
	return M1SDR_WAIT;
}

void m1sdr_PrintDevices(void)
{
}

// m1sdr_Init - inits the output device and our global state
INT16 m1sdr_Init(char *device, int sample_rate)
{
	hw_present = 0;

	nDSoundSegLen = sample_rate / 60; // 44100 / 60 = 735

	memset(samples, 0, sizeof(samples));	// zero out samples

	// PortAudio init
	err = Pa_Initialize();
    if(err != paNoError) goto error;

    /* Open an audio I/O stream. */
    err = Pa_OpenDefaultStream(&stream,
                                0,          /* no input channels */
                                2,          /* stereo output */
                                paInt16,    /* 16 bit digital output */
                                sample_rate,
                                nDSoundSegLen, /* frames per buffer (256?) */
                                patestCallback,
                                &data);

	if(err != paNoError) goto error;

	err = Pa_StartStream(stream);
    if(err != paNoError) goto error;

	hw_present = 1;
	(void) device;
	return (1);
error:
    Pa_Terminate();
    fprintf(stderr, "An error occurred while using the portaudio stream\n");
    fprintf(stderr, "Error number: %d\n", err);
    fprintf(stderr, "Error message: %s\n", Pa_GetErrorText(err));
    return err;
}

void m1sdr_Exit(void)
{
	if (!hw_present) return;

	err = Pa_StopStream(stream);
    if(err != paNoError) goto error;
    err = Pa_CloseStream(stream);
    if(err != paNoError) goto error;
    Pa_Terminate();
error:
    Pa_Terminate();
    fprintf(stderr, "An error occurred while using the portaudio stream\n");
    fprintf(stderr, "Error number: %d\n", err);
    fprintf(stderr, "Error message: %s\n", Pa_GetErrorText(err));
}

// unused stubs for this driver, but the Win32 driver needs them
void m1sdr_PlayStart(void)
{
}

void m1sdr_PlayStop(void)
{
}

void m1sdr_FlushAudio(void)
{
	memset(samples, 0, nDSoundSegLen * 4);
}

void m1sdr_SetNoWait(int nw)
{
	(void)nw;
}
