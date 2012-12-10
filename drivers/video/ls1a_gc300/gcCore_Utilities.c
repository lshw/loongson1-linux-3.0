#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include "gcSdk.h"

//UINT32 gcREG_BASE = 0xBC200000;	/* gc300寄存器基地址 */

UINT32 gcReadReg(UINT32 Address)
{
//	return *(PUINT32) (gcREG_BASE + (Address << 2));
	return *(volatile unsigned int *)(0xBC200000 + (Address << 2));
}

void gcWriteReg(UINT32 Address, UINT32 Data)
{
//	*(PUINT32) (gcREG_BASE + (Address << 2)) = Data;
	*(volatile unsigned int *)(0xBC200000 + (Address << 2)) = Data;
}

UINT32 gcReportIdle(char* Message)
{
	UINT32 idle = gcReadReg(AQHiIdleRegAddrs);

	if (Message != NULL) {
		printk(Message, idle);
	}

	return idle;
}

UINT32 gcReportRegs(void)
{
	UINT32 ClockControl 	= gcReadReg(AQHiClockControlRegAddrs);
	UINT32 HiIdleReg 		= gcReadReg(AQHiIdleRegAddrs);
	UINT32 AxiConfigReg 	= gcReadReg(AQAxiConfigRegAddrs);
	UINT32 AxiStatusReg 	= gcReadReg(AQAxiStatusRegAddrs);
	UINT32 IntrAcknowledge 	= gcReadReg(AQIntrAcknowledgeRegAddrs);
	UINT32 IntrEnblReg 		= gcReadReg(AQIntrEnblRegAddrs);
	UINT32 IdentReg			= gcReadReg(AQIdentRegAddrs);
	UINT32 FeaturesReg		= gcReadReg(GCFeaturesRegAddrs);
	
	UINT32 StreamBaseAddr	= gcReadReg(AQIndexStreamBaseAddrRegAddrs);
	UINT32 IndexStreamCtrl  = gcReadReg(AQIndexStreamCtrlRegAddrs);
	UINT32 CmdBufferAddr 	= gcReadReg(AQCmdBufferAddrRegAddrs);
	UINT32 CmdBufferCtrl 	= gcReadReg(AQCmdBufferCtrlRegAddrs);
	UINT32 FEStatusReg 		= gcReadReg(AQFEStatusRegAddrs);
	UINT32 FEDebugState 	= gcReadReg(AQFEDebugStateRegAddrs);
	UINT32 FEDebugCurCmd 	= gcReadReg(AQFEDebugCurCmdAdrRegAddrs);
	UINT32 FEDebugCmdLow 	= gcReadReg(AQFEDebugCmdLowRegRegAddrs);

	printk("ClockControl 	= %x\n", ClockControl	);
	printk("HiIdleReg    	= %x\n", HiIdleReg   	);
	printk("AxiConfigReg 	= %x\n", AxiConfigReg	);
	printk("AxiStatusReg 	= %x\n", AxiStatusReg	);
	printk("IntrAcknowledge = %x\n", IntrAcknowledge);
	printk("IntrEnblReg 	= %x\n", IntrEnblReg	);	
	printk("IdentReg    	= %x\n", IdentReg   	);	
	printk("FeaturesReg 	= %x\n", FeaturesReg	);
	
	printk("registers in FE module!\n");
	printk("StreamBaseAddr 	= %x\n", StreamBaseAddr	);
	printk("IndexStreamCtrl = %x\n", IndexStreamCtrl);
	printk("CmdBufferAddr 	= %x\n", CmdBufferAddr	);
	printk("CmdBufferCtrl	= %x\n", CmdBufferCtrl	);
	printk("FEStatusReg 	= %x\n", FEStatusReg	);
	printk("FEDebugState 	= %x\n", FEDebugState	);
	printk("FEDebugCurCmd 	= %x\n", FEDebugCurCmd	);	
	printk("FEDebugCmdLow	= %x\n", FEDebugCmdLow	);

	return 1;
}

