#include "esfmu_helper.h"

#include <windows.h>
#include <process.h>
#include <stdio.h>

typedef struct eht {
    HWAVEOUT hWaveOut;
    WAVEHDR	*WaveHdr;
    HANDLE	hEvent;
    DWORD	chunks;
    DWORD	prevPlayPos;
    DWORD	getPosWraps;
    DWORD framesRendered;
    unsigned int sampleRate;
    unsigned int bufferSize;
    unsigned int chunkSize;
    BOOL	stopProcessing;
    esfm_chip esfmu;
    BOOL esfm_initialized;
    int16_t buffer[16384];
} esfmu_helper_t;

static esfmu_helper_t helper;

esfm_chip* getESFMuObject() {
    if (!helper.esfm_initialized) {
        ESFM_init(&helper.esfmu);

        // Init ESFM native mode
        ESFM_write_port(&helper.esfmu, 2, 0x05);
        ESFM_write_port(&helper.esfmu, 3, 0x80);
        helper.esfm_initialized = TRUE;
    }
    return &helper.esfmu;
}

void Render(short *bufpos, uint32_t totalFrames) {
	ESFM_generate_stream(getESFMuObject(), bufpos, totalFrames);
}

int16_t* GetWaveBuffer() {
    // static int16_t buffer[8820];
    return helper.buffer;
}

UINT64 GetPos()
{
	MMTIME mmTime;
	mmTime.wType = TIME_SAMPLES;

	if (waveOutGetPosition(helper.hWaveOut, &mmTime, sizeof(MMTIME)) != MMSYSERR_NOERROR)
	{
		MessageBox(NULL, "Failed to get current playback position","ESFMu Helper",
		MB_OK | MB_ICONEXCLAMATION);
		return 10;
	}
	if (mmTime.wType != TIME_SAMPLES)
	{
		MessageBox(NULL, "Failed to get # of samples played", "ESFMu Helper",
			MB_OK | MB_ICONEXCLAMATION);
		return 10;
	}

	int delta = mmTime.u.sample - helper.prevPlayPos;
	if (delta < -(1 << 26))
	{
		++helper.getPosWraps;
	}
	helper.prevPlayPos = mmTime.u.sample;
	return mmTime.u.sample + helper.getPosWraps * (1 << 27);
}


void RenderAvailableSpace() 
{
	DWORD playPos = GetPos() % helper.bufferSize;
	DWORD framesToRender;

	if (playPos < helper.framesRendered) {
		// Buffer wrap, render 'till the end of the buffer
		framesToRender = helper.bufferSize - helper.framesRendered;
	} 
	else 
	{
		framesToRender = playPos - helper.framesRendered;
		if (framesToRender < helper.chunkSize) 
		{
			Sleep(1 + (helper.chunkSize - framesToRender) * 1000 / helper.sampleRate);
			return;
		}
	}
	Render(GetWaveBuffer() + 2 * helper.framesRendered, framesToRender);
}


void RenderingThread(void *arg)
{
    while (!helper.stopProcessing)
    {
        BOOL allBuffersRendered = TRUE;
        for (int i = 0; i < helper.chunks && !helper.stopProcessing; i++)
        {
            if (helper.WaveHdr[i].dwFlags & WHDR_DONE)
            {
                allBuffersRendered = FALSE;
                Render((short*)helper.WaveHdr[i].lpData, helper.WaveHdr[i].dwBufferLength / 4);

                if (!helper.stopProcessing && waveOutWrite(helper.hWaveOut,
                &helper.WaveHdr[i],sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
                {
                    MessageBox(NULL, "Phase 2 Failed to write block to device", "ESFMu Helper",
                    MB_OK | MB_ICONEXCLAMATION);
                }
            }
        }
        if (allBuffersRendered)
        {
            WaitForSingleObject(helper.hEvent, INFINITE);
        }
    }
}


int InitWaveOut()
{
    memset(&helper, 0, sizeof(esfmu_helper_t));
    helper.hWaveOut = NULL;
    helper.framesRendered = 0;
    helper.sampleRate = 49716; //44100;
    helper.bufferSize = (49716 * 100 / 1000.f);
    helper.chunkSize = (49716 * 10 / 1000.f);
    helper.stopProcessing = TRUE;

	DWORD callbackType = CALLBACK_EVENT;
	helper.hEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
    DWORD_PTR callback = (DWORD_PTR)helper.hEvent;
	int16_t *buffer = GetWaveBuffer();
	
	callbackType = CALLBACK_EVENT;

	PCMWAVEFORMAT wFormat = {WAVE_FORMAT_PCM, 2, helper.sampleRate, helper.sampleRate * 4, 4, 16};

	// Open waveout device
	int wResult = waveOutOpen(&helper.hWaveOut, WAVE_MAPPER, (LPCWAVEFORMATEX)&wFormat, callback, 0, callbackType);
	if (wResult != MMSYSERR_NOERROR)
	{
		MessageBox(NULL, "Failed to open waveform output device", "ESFMu Helper", MB_OK | MB_ICONEXCLAMATION);
		return 1;
	}

	// Prepare headers
	helper.chunks = helper.bufferSize / helper.chunkSize;

	if (helper.WaveHdr == NULL) {
		helper.WaveHdr = malloc(sizeof(WAVEHDR) * helper.chunks);
	}
	
	LPSTR chunkStart = (LPSTR)buffer;
	DWORD chunkBytes = 4 * helper.chunkSize;
	for (int i = 0; i < helper.chunks; i++)
	{
		helper.WaveHdr[i].dwBufferLength = chunkBytes;
		helper.WaveHdr[i].lpData = chunkStart;
		helper.WaveHdr[i].dwFlags = 0L;
		helper.WaveHdr[i].dwLoops = 0L;
		chunkStart += chunkBytes;
	
        wResult = waveOutPrepareHeader(helper.hWaveOut, &helper.WaveHdr[i], sizeof(WAVEHDR));
        if (wResult != MMSYSERR_NOERROR)
        {
            MessageBox(NULL, "Failed to Prepare Header", "ESFMu Helper", MB_OK |
                MB_ICONEXCLAMATION);
            return 1;
        }
    }
	helper.stopProcessing = FALSE;
         
	return 0;
}

int StartWaveOut()
{
	static BOOL rendering = FALSE;

	if (rendering) return 0;

	helper.framesRendered = 0;

	helper.getPosWraps = 0;
	helper.prevPlayPos = 0;
	for (UINT i = 0; i < helper.chunks; i++)
	{
		if (waveOutWrite(helper.hWaveOut, &helper.WaveHdr[i], sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
		{
			MessageBox(NULL, "Phase 1 Failed to write block to device", "ESFMu Helper", 
			 	MB_OK | MB_ICONEXCLAMATION);
			return 4;
		}
	}
	
	_beginthread(RenderingThread, 16384, NULL);

	rendering = TRUE;
    return 0;
}

int CloseWaveOut() {
	helper.stopProcessing = TRUE;
	SetEvent(helper.hEvent);
	int wResult = waveOutReset(helper.hWaveOut);
	if (wResult != MMSYSERR_NOERROR) 
	{
		MessageBox(NULL, "Failed to Reset WaveOut", "ESFMu Helper", MB_OK | MB_ICONEXCLAMATION);
		return 1;
	}

	for (UINT i = 0; i < helper.chunks; i++)
	{
		wResult = waveOutUnprepareHeader(helper.hWaveOut, &helper.WaveHdr[i], sizeof(WAVEHDR));
		if (wResult != MMSYSERR_NOERROR)
		{
			MessageBox(NULL, "Failed to Unprepare Wave Header", "ESFMu Helper", 
				MB_OK | MB_ICONEXCLAMATION);
			return 1;
		}
	}
	free(helper.WaveHdr);
	helper.WaveHdr = NULL;

	wResult = waveOutClose(helper.hWaveOut);
	if (wResult != MMSYSERR_NOERROR) {
		MessageBox(NULL, "Failed to Close WaveOut", "ESFMu Helper", 
			MB_OK | MB_ICONEXCLAMATION);
		return 1;
	}
	if (helper.hEvent != NULL)
	{
		CloseHandle(helper.hEvent);
		helper.hEvent = NULL;
	}
	return 0;
}