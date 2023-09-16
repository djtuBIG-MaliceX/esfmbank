#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "drv_inpout32.h"

BOOL IODriver_Init(USHORT first, USHORT last)
{
	return OpenInpOut32();
}


