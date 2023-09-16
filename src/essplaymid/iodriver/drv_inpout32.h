#pragma once

#include "InpOut32Helper.h"

#define WRITE_PORT_UCHAR(Port, Value) outportb((USHORT)Port, Value)
#define READ_PORT_UCHAR(Port) inportb((USHORT)Port)

#define IODriver_Exit()


