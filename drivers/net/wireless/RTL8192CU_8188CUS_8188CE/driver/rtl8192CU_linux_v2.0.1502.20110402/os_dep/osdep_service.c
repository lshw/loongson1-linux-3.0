/******************************************************************************
 *
 * Copyright(c) 2007 - 2010 Realtek Corporation. All rights reserved.
 *                                        
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/

#define _OSDEP_SERVICE_C_

#include <drv_conf.h>
#include <osdep_service.h>
#include <drv_types.h>
#include <recv_osdep.h>
#include <linux/vmalloc.h>


#define RT_TAG	'1178'


inline u8* _rtw_vmalloc(u32 sz)
{
	u8 	*pbuf;
#ifdef PLATFORM_LINUX	
	pbuf = vmalloc(sz);	
#endif	
	
#ifdef PLATFORM_WINDOWS
	NdisAllocateMemoryWithTag(&pbuf,sz, RT_TAG);	
#endif

	return pbuf;	
}

inline u8* _rtw_zvmalloc(u32 sz)
{
	u8 	*pbuf;
#ifdef PLATFORM_LINUX
	pbuf = _rtw_vmalloc(sz);
	if (pbuf != NULL)
		memset(pbuf, 0, sz);
#endif	
	
#ifdef PLATFORM_WINDOWS
	NdisAllocateMemoryWithTag(&pbuf,sz, RT_TAG);
	if (pbuf != NULL)
		NdisFillMemory(pbuf, sz, 0);
#endif

	return pbuf;	
}

inline void _rtw_vmfree(u8 *pbuf, u32 sz)
{
#ifdef	PLATFORM_LINUX
	vfree(pbuf);
#endif	
	
#ifdef PLATFORM_WINDOWS
	NdisFreeMemory(pbuf,sz, 0);
#endif
}


inline u8* dbg_rtw_vmalloc(u32 sz, const char *func, int line)
{
	DBG_871X("%s:%d %s(%d)\n", func,  line, __FUNCTION__, (sz));
	return _rtw_vmalloc((sz));
}

inline u8* dbg_rtw_zvmalloc(u32 sz, const char *func, int line)
{
	DBG_871X("%s:%d %s(%d)\n", func, line, __FUNCTION__, (sz)); 
	return _rtw_zvmalloc((sz)); 
}

inline void dbg_rtw_vmfree(u8 *pbuf, u32 sz, const char *func, int line)
{
	DBG_871X("%s:%d %s(%p,%d)\n",  func, line, __FUNCTION__, (pbuf), (sz));
	_rtw_vmfree((pbuf), (sz)); 
}

u8* _rtw_zmalloc(u32 sz)
{
	u8 	*pbuf;
#ifdef PLATFORM_LINUX
	// kzalloc(sz, GFP_KERNEL);
	pbuf = 	kmalloc(sz, /*GFP_KERNEL*/GFP_ATOMIC);
	if (pbuf != NULL)
		memset(pbuf, 0, sz);
#endif	
	
#ifdef PLATFORM_WINDOWS
	NdisAllocateMemoryWithTag(&pbuf,sz, RT_TAG);
	if (pbuf != NULL)
		NdisFillMemory(pbuf, sz, 0);
#endif

	return pbuf;	
	
}

u8* _rtw_malloc(u32 sz)
{
	u8 	*pbuf;
#ifdef PLATFORM_LINUX	
	pbuf = 	kmalloc(sz, /*GFP_KERNEL*/GFP_ATOMIC);	
#endif	
	
#ifdef PLATFORM_WINDOWS
	NdisAllocateMemoryWithTag(&pbuf,sz, RT_TAG);	
#endif

	return pbuf;	
	
}

void	_rtw_mfree(u8 *pbuf, u32 sz)
{

#ifdef	PLATFORM_LINUX

	kfree(pbuf);

#endif	
	
#ifdef PLATFORM_WINDOWS

	NdisFreeMemory(pbuf,sz, 0);

#endif
	
	
}


inline u8* dbg_rtw_malloc(u32 sz, const char *func, int line) 
{
	if((sz)>4096) 
		DBG_871X("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!%s:%d %s(%d)\n", func, line, __FUNCTION__, (sz)); 
	return _rtw_malloc((sz)); 
}

inline u8* dbg_rtw_zmalloc(u32 sz, const char *func, int line)
{
	if((sz)>4096)
		DBG_871X("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!%s:%d %s(%d)\n", func, line, __FUNCTION__, (sz));
	return _rtw_zmalloc((sz));
}

inline void dbg_rtw_mfree(u8 *pbuf, u32 sz, const char *func, int line)
{
	//DBG_871X("%s:%d %s(%p,%d)\n", func, line, __FUNCTION__, (pbuf), (sz));
	_rtw_mfree((pbuf), (sz));
}


void _rtw_memcpy(void* dst, void* src, u32 sz)
{

#ifdef PLATFORM_LINUX

	memcpy(dst, src, sz);

#endif	
	
#ifdef PLATFORM_WINDOWS

	NdisMoveMemory(dst, src, sz);

#endif

}

int	_rtw_memcmp(void *dst, void *src, u32 sz)
{

#ifdef PLATFORM_LINUX
//under Linux/GNU/GLibc, the return value of memcmp for two same mem. chunk is 0

	if (!(memcmp(dst, src, sz)))
		return _TRUE;
	else
		return _FALSE;
#endif


#ifdef PLATFORM_WINDOWS
//under Windows, the return value of NdisEqualMemory for two same mem. chunk is 1
	
	if (NdisEqualMemory (dst, src, sz))
		return _TRUE;
	else
		return _FALSE;

#endif	
	
	
	
}

void _rtw_memset(void *pbuf, int c, u32 sz)
{

#ifdef PLATFORM_LINUX

        memset(pbuf, c, sz);

#endif

#ifdef PLATFORM_WINDOWS
#if 0
	NdisZeroMemory(pbuf, sz);
	if (c != 0) memset(pbuf, c, sz);
#else
	NdisFillMemory(pbuf, sz, c);
#endif
#endif

}

void _rtw_init_listhead(_list *list)
{

#ifdef PLATFORM_LINUX

        INIT_LIST_HEAD(list);

#endif

#ifdef PLATFORM_WINDOWS

        NdisInitializeListHead(list);

#endif

}


/*
For the following list_xxx operations, 
caller must guarantee the atomic context.
Otherwise, there will be racing condition.
*/
u32	rtw_is_list_empty(_list *phead)
{

#ifdef PLATFORM_LINUX

	if (list_empty(phead))
		return _TRUE;
	else
		return _FALSE;

#endif
	

#ifdef PLATFORM_WINDOWS

	if (IsListEmpty(phead))
		return _TRUE;
	else
		return _FALSE;

#endif

	
}


void rtw_list_insert_tail(_list *plist, _list *phead)
{

#ifdef PLATFORM_LINUX	
	
	list_add_tail(plist, phead);
	
#endif
	
#ifdef PLATFORM_WINDOWS

  InsertTailList(phead, plist);

#endif		
	
}


/*

Caller must check if the list is empty before calling list_delete

*/


void _rtw_init_sema(_sema	*sema, int init_val)
{

#ifdef PLATFORM_LINUX

	sema_init(sema, init_val);

#endif

#ifdef PLATFORM_OS_XP

	KeInitializeSemaphore(sema, init_val,  SEMA_UPBND); // count=0;

#endif
	
#ifdef PLATFORM_OS_CE
	if(*sema == NULL)
		*sema = CreateSemaphore(NULL, init_val, SEMA_UPBND, NULL);
#endif

}

void _rtw_free_sema(_sema	*sema)
{

#ifdef PLATFORM_OS_CE
	CloseHandle(*sema);
#endif

}

void _rtw_up_sema(_sema	*sema)
{

#ifdef PLATFORM_LINUX

	up(sema);

#endif	

#ifdef PLATFORM_OS_XP

	KeReleaseSemaphore(sema, IO_NETWORK_INCREMENT, 1,  FALSE );

#endif

#ifdef PLATFORM_OS_CE
	ReleaseSemaphore(*sema,  1,  NULL );
#endif
}

u32 _rtw_down_sema(_sema *sema)
{

#ifdef PLATFORM_LINUX
	
	if (down_interruptible(sema))
		return _FAIL;
    else
    	return _SUCCESS;

#endif    	

#ifdef PLATFORM_OS_XP

	if(STATUS_SUCCESS == KeWaitForSingleObject(sema, Executive, KernelMode, TRUE, NULL))
		return  _SUCCESS;
	else
		return _FAIL;
#endif

#ifdef PLATFORM_OS_CE
	if(WAIT_OBJECT_0 == WaitForSingleObject(*sema, INFINITE ))
		return _SUCCESS; 
	else
		return _FAIL;
#endif
}



void	_rtw_mutex_init(_mutex *pmutex)
{
#ifdef PLATFORM_LINUX

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
	mutex_init(pmutex);
#else
	init_MUTEX(pmutex);
#endif
	
#endif

#ifdef PLATFORM_OS_XP

	KeInitializeMutex(pmutex, 0);

#endif

#ifdef PLATFORM_OS_CE
	*pmutex =  CreateMutex( NULL, _FALSE, NULL);
#endif
}


void	_rtw_spinlock_init(_lock *plock)
{

#ifdef PLATFORM_LINUX

	spin_lock_init(plock);

#endif	
	
#ifdef PLATFORM_WINDOWS

	NdisAllocateSpinLock(plock);

#endif
	
}

void	_rtw_spinlock_free(_lock *plock)
{

	
#ifdef PLATFORM_WINDOWS

	NdisFreeSpinLock(plock);

#endif
	
}


void	_rtw_spinlock(_lock	*plock)
{

#ifdef PLATFORM_LINUX

	spin_lock(plock);

#endif
	
#ifdef PLATFORM_WINDOWS

	NdisAcquireSpinLock(plock);

#endif
	
}

void	_rtw_spinunlock(_lock *plock)
{

#ifdef PLATFORM_LINUX

	spin_unlock(plock);

#endif
	
#ifdef PLATFORM_WINDOWS

	NdisReleaseSpinLock(plock);

#endif
}


void	_rtw_spinlock_ex(_lock	*plock)
{

#ifdef PLATFORM_LINUX

	spin_lock(plock);

#endif
	
#ifdef PLATFORM_WINDOWS

	NdisDprAcquireSpinLock(plock);

#endif
	
}

void	_rtw_spinunlock_ex(_lock *plock)
{

#ifdef PLATFORM_LINUX

	spin_unlock(plock);

#endif
	
#ifdef PLATFORM_WINDOWS

	NdisDprReleaseSpinLock(plock);

#endif
}



void	_rtw_init_queue(_queue	*pqueue)
{

	_rtw_init_listhead(&(pqueue->queue));

	_rtw_spinlock_init(&(pqueue->lock));

}

u32	  _rtw_queue_empty(_queue	*pqueue)
{
	return (rtw_is_list_empty(&(pqueue->queue)));
}


u32 rtw_end_of_queue_search(_list *head, _list *plist)
{

	if (head == plist)
		return _TRUE;
	else
		return _FALSE;
		
}


u32	rtw_get_current_time(void)
{
	
#ifdef PLATFORM_LINUX

	return jiffies;

#endif	
	
#ifdef PLATFORM_WINDOWS

	LARGE_INTEGER	SystemTime;
	NdisGetCurrentSystemTime(&SystemTime);
	return (u32)(SystemTime.LowPart);// count of 100-nanosecond intervals 

#endif
	
	
}

void rtw_sleep_schedulable(int ms)	
{

#ifdef PLATFORM_LINUX

    u32 delta;
    
    delta = (ms * HZ)/1000;//(ms)
    if (delta == 0) {
        delta = 1;// 1 ms
    }
    set_current_state(TASK_INTERRUPTIBLE);
    if (schedule_timeout(delta) != 0) {
        return ;
    }
    return;

#endif	
	
#ifdef PLATFORM_WINDOWS

	NdisMSleep(ms*1000); //(us)*1000=(ms)

#endif

}


void rtw_msleep_os(int ms)
{

#ifdef PLATFORM_LINUX

  	msleep((unsigned int)ms);

#endif	
	
#ifdef PLATFORM_WINDOWS

	NdisMSleep(ms*1000); //(us)*1000=(ms)

#endif


}
void rtw_usleep_os(int us)
{

#ifdef PLATFORM_LINUX
  	
      // msleep((unsigned int)us);
      if ( 1 < (us/1000) )
                msleep(1);
      else
		msleep( (us/1000) + 1);

#endif	
	
#ifdef PLATFORM_WINDOWS

	NdisMSleep(us); //(us)

#endif


}

void rtw_mdelay_os(int ms)
{

#ifdef PLATFORM_LINUX

   	mdelay((unsigned long)ms); 

#endif	
	
#ifdef PLATFORM_WINDOWS

	NdisStallExecution(ms*1000); //(us)*1000=(ms)

#endif


}
void rtw_udelay_os(int us)
{

#ifdef PLATFORM_LINUX

      udelay((unsigned long)us); 

#endif	
	
#ifdef PLATFORM_WINDOWS

	NdisStallExecution(us); //(us)

#endif

}

