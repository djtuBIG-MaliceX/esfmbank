#ifndef ESFMU_HELPER_H
#define ESFMU_HELPER_H

#include "../extern/ESFMu/esfm.h"

// Fucking disgusting hax to make the same ESFMu object visible across all files
esfm_chip* getESFMuObject();

// wAV PLAY
void Render(short *bufpos, uint32_t totalFrames);
uint64_t GetPos();
void RenderAvailableSpace();
void RenderingThread(void *arg);
int InitWaveOut();
int StartWaveOut();
int CloseWaveOut();

#endif