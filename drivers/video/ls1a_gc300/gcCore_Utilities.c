#include "gcSdk.h"

UINT32 gcReadReg(UINT32 Address)
{
	return *(volatile unsigned int *)(0xbc200000 + (Address << 2));
}

void gcWriteReg(UINT32 Address, UINT32 Data)
{
	*(volatile unsigned int *)(0xbc200000 + (Address << 2)) = Data;
}

UINT32 gcReportIdle(char* Message)
{
	UINT32 idle = gcReadReg(AQHiIdleRegAddrs);

	if (Message != NULL) {
		printk(Message, idle);
	}

	return idle;
}

