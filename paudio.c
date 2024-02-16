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

// local variables
int nDSoundSegLen = 0; // Seg length in samples (calculated from Rate/Fps)
static PaStream *stream;
static PaError err;
static stereo_sample_t samples[AUDIO_RATE];
static int nowait = 0;


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

static void pa_error_handler() {
    Pa_Terminate();
    fprintf(stderr, "An error occurred while using the portaudio stream\n");
    fprintf(stderr, "Error number: %d\n", err);
    fprintf(stderr, "Error message: %s\n", Pa_GetErrorText(err));
}

// checks the play position to see if we should trigger another update
m1sdr_ret_t m1sdr_TimeCheck(void)
{
    if (nowait)
    {
        m1sdr_Update();

        err = Pa_WriteStream(stream, samples, nDSoundSegLen);
        if(err != paNoError) goto error;
    }
    else
    {
        unsigned long frames = 0;
        while ((frames = Pa_GetStreamWriteAvailable(stream)) >= nDSoundSegLen)
        {
            m1sdr_Update();

            err = Pa_WriteStream(stream, samples, nDSoundSegLen);
            if(err != paNoError) goto error;
        }
    }

    return M1SDR_WAIT;

error:
    pa_error_handler();
    return M1SDR_ERROR;
}

void m1sdr_PrintDevices(void)
{
    err = Pa_Initialize();
    if (err != paNoError) goto error;

    PaHostApiIndex hi = Pa_GetDefaultHostApi();
    if (hi < 0) goto error;

    const PaHostApiInfo* info = Pa_GetHostApiInfo(hi);
    printf("HostApi: %s\n", info->name);

    for (int i=0; i<info->deviceCount; ++i)
    {
        PaDeviceIndex di = Pa_HostApiDeviceIndexToDeviceIndex(hi, i);
        const PaDeviceInfo* device = Pa_GetDeviceInfo(di);
        printf("Device: %s %s\n", device->name,
            di == info->defaultOutputDevice ? "[DEFAULT]" : "");
    }

    Pa_Terminate();
    return;
error:
    pa_error_handler();
    return;
}

/** On error returns paNoDevice */
static PaDeviceIndex pa_get_device_index(char *device) {
    PaHostApiIndex hi = Pa_GetDefaultHostApi();
    if (hi < 0) return paNoDevice;

    const PaHostApiInfo* info = Pa_GetHostApiInfo(hi);

    for (int i=0; i<info->deviceCount; ++i)
    {
        PaDeviceIndex di = Pa_HostApiDeviceIndexToDeviceIndex(hi, i);
        const PaDeviceInfo* thisDevice = Pa_GetDeviceInfo(di);
        if (strcmp(thisDevice->name, device) == 0)
            return di;
    }

    return paNoDevice;
}

// m1sdr_Init - inits the output device and our global state
INT16 m1sdr_Init(char *device, int sample_rate)
{
    hw_present = 0;
    nDSoundSegLen = sample_rate / 60;

    memset(samples, 0, sizeof(samples));

    // PortAudio init
    err = Pa_Initialize();
    if (err != paNoError) goto error;

    // Open an audio I/O stream
    if (!device)
    {
        err = Pa_OpenDefaultStream(&stream,
                            0, // no input channels
                            2, // stereo output
                            paInt16, // 16 bit digital samples
                            sample_rate,
                            nDSoundSegLen, // frames per buffer
                            NULL, // no callback, use blocking api
                            NULL); // no callback, so no callback userData
    }
    else
    {
        PaStreamParameters outParams;
        outParams.device = pa_get_device_index(device);
        if (outParams.device == paNoDevice)
        {
            fprintf(stderr,"Error: Not a valid output device: %s.\n", device);
            goto error;
        }
        outParams.channelCount = 2;
        outParams.sampleFormat = paInt16;
        outParams.suggestedLatency = Pa_GetDeviceInfo(outParams.device)->defaultLowOutputLatency;
        outParams.hostApiSpecificStreamInfo = NULL;

        err = Pa_OpenStream(&stream,
                            NULL,
                            &outParams,
                            sample_rate,
                            nDSoundSegLen,
                            paClipOff,
                            NULL,
                            NULL);
    }

    if (err != paNoError) goto error;

    err = Pa_StartStream(stream);
    if (err != paNoError) goto error;

    hw_present = 1;
    return (1);
error:
    pa_error_handler();
    return err;
}

void m1sdr_Exit(void)
{
    if (!hw_present) return;

    err = Pa_StopStream(stream);
    if (err != paNoError) goto error;
    err = Pa_CloseStream(stream);
    if (err != paNoError) goto error;
    Pa_Terminate();
error:
    pa_error_handler();
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
    nowait = nw;
}
