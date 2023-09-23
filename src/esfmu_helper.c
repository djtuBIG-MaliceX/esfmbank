#include "esfmu_helper.h"

#include <windows.h>
#include <process.h>
#include <stdio.h>

esfm_chip* getESFMuObject() {
    static esfm_chip *esfmu = NULL;
    if (esfmu == NULL) {
        esfmu = (esfm_chip*)malloc(sizeof(esfm_chip));
        ESFM_init(esfmu);
    }
    return esfmu;
}

// WAVE PLAY

static HWAVEOUT	hWaveOut = NULL;
static WAVEHDR	*WaveHdr;
static HANDLE	hEvent;
static DWORD	chunks;
static DWORD	prevPlayPos;
static DWORD	getPosWraps;
static DWORD framesRendered = 0;
static unsigned int sampleRate = 44100;
static unsigned int bufferSize = (44100 * 100 / 1000.f);
static unsigned int chunkSize = (44100 * 10 / 1000.f);
static BOOL	stopProcessing = TRUE;

void Render(short *bufpos, uint32_t totalFrames) {
	ESFM_generate_stream(getESFMuObject(), bufpos, totalFrames);
}

int16_t* GetWaveBuffer() {
    static int16_t *buffer = NULL;
    if (buffer == NULL) {
        buffer = (int16_t*)malloc(2 * bufferSize);
    }
    return buffer;
}

UINT64 GetPos()
{
	MMTIME mmTime;
	mmTime.wType = TIME_SAMPLES;

	if (waveOutGetPosition(hWaveOut, &mmTime, sizeof(MMTIME)) != MMSYSERR_NOERROR)
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

	int delta = mmTime.u.sample - prevPlayPos;
	if (delta < -(1 << 26))
	{
		++getPosWraps;
	}
	prevPlayPos = mmTime.u.sample;
	return mmTime.u.sample + getPosWraps * (1 << 27);
}


void RenderAvailableSpace() 
{
	DWORD playPos = GetPos() % bufferSize;
	DWORD framesToRender;

	if (playPos < framesRendered) {
		// Buffer wrap, render 'till the end of the buffer
		framesToRender = bufferSize - framesRendered;
	} 
	else 
	{
		framesToRender = playPos - framesRendered;
		if (framesToRender < chunkSize) 
		{
			Sleep(1 + (chunkSize - framesToRender) * 1000 / sampleRate);
			return;
		}
	}
	Render(GetWaveBuffer() + 2 * framesRendered, framesToRender);
}


void RenderingThread(void *arg)
{
	if (chunks == 1)
	{
		// Rendering using single looped ring buffer
		while (!stopProcessing)
		{
			RenderAvailableSpace();
		}
	} 
	else 
	{
		while (!stopProcessing)
		{
			BOOL allBuffersRendered = TRUE;
			for (int i = 0; i < chunks && !stopProcessing; i++)
			{
				if (WaveHdr[i].dwFlags & WHDR_DONE)
				{
					allBuffersRendered = FALSE;
					Render((short*)WaveHdr[i].lpData, WaveHdr[i].dwBufferLength / 4);

					if (!stopProcessing && waveOutWrite(hWaveOut,
					&WaveHdr[i],sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
					{
						MessageBox(NULL, "Phase 2 Failed to write block to device", "ESFMu Helper",
						MB_OK | MB_ICONEXCLAMATION);
					}
				}
			}
			if (allBuffersRendered)
			{
				WaitForSingleObject(hEvent, INFINITE);
			}
		}
	}
}


int InitWaveOut() {
	DWORD callbackType = CALLBACK_EVENT;
	hEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
    DWORD_PTR callback = (DWORD_PTR)hEvent;;
	int16_t *buffer = GetWaveBuffer();
	
	callbackType = CALLBACK_EVENT;

	PCMWAVEFORMAT wFormat = {WAVE_FORMAT_PCM, 2, sampleRate, sampleRate * 4, 4, 16};

	// Open waveout device
	int wResult = waveOutOpen(&hWaveOut, WAVE_MAPPER, (LPCWAVEFORMATEX)&wFormat, callback, 0, callbackType);
	if (wResult != MMSYSERR_NOERROR)
	{
		MessageBox(NULL, "Failed to open waveform output device", "ESFMu Helper", MB_OK | MB_ICONEXCLAMATION);
		return 1;
	}

	// Prepare headers
	chunks = bufferSize / chunkSize;
	WaveHdr = calloc(sizeof(WAVEHDR), chunks);
	LPSTR chunkStart = (LPSTR)buffer;
	DWORD chunkBytes = 4 * chunkSize;
	for (int i = 0; i < chunks; i++)
	{
		WaveHdr[i].dwBufferLength = chunkBytes;
		WaveHdr[i].lpData = chunkStart;
		WaveHdr[i].dwFlags = 0L;
		WaveHdr[i].dwLoops = 0L;
		chunkStart += chunkBytes;
	
        wResult = waveOutPrepareHeader(hWaveOut, &WaveHdr[i], sizeof(WAVEHDR));
        if (wResult != MMSYSERR_NOERROR)
        {
            MessageBox(NULL, "Failed to Prepare Header", "ESFMu Helper", MB_OK |
                MB_ICONEXCLAMATION);
            return 1;
        }
    }
	stopProcessing = FALSE;
         
	return 0;
}

int StartWaveOut()
{
	Render(GetWaveBuffer(), bufferSize);
	framesRendered = 0;

	getPosWraps = 0;
	prevPlayPos = 0;
	for (UINT i = 0; i < chunks; i++)
	{
		if (waveOutWrite(hWaveOut, &WaveHdr[i], sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
		{
			// MessageBox(NULL, "Phase 1 Failed to write block to device", "ESFMu Helper", 
			//  	MB_OK | MB_ICONEXCLAMATION);
			// return 4;
		}
	}
	
	_beginthread(RenderingThread, 16384, NULL);
}

int CloseWaveOut() {
	stopProcessing = TRUE;
	SetEvent(hEvent);
	int wResult = waveOutReset(hWaveOut);
	if (wResult != MMSYSERR_NOERROR) 
	{
		MessageBox(NULL, "Failed to Reset WaveOut", "ESFMu Helper", MB_OK | MB_ICONEXCLAMATION);
		return 1;
	}

	for (UINT i = 0; i < chunks; i++)
	{
		wResult = waveOutUnprepareHeader(hWaveOut, &WaveHdr[i], sizeof(WAVEHDR));
		if (wResult != MMSYSERR_NOERROR)
		{
			MessageBox(NULL, "Failed to Unprepare Wave Header", "ESFMu Helper", 
				MB_OK | MB_ICONEXCLAMATION);
			return 1;
		}
	}
	free(WaveHdr);
	WaveHdr = NULL;

	wResult = waveOutClose(hWaveOut);
	if (wResult != MMSYSERR_NOERROR) {
		MessageBox(NULL, "Failed to Close WaveOut", "ESFMu Helper", 
			MB_OK | MB_ICONEXCLAMATION);
		return 1;
	}
	if (hEvent != NULL)
	{
		CloseHandle(hEvent);
		hEvent = NULL;
	}
	return 0;
}