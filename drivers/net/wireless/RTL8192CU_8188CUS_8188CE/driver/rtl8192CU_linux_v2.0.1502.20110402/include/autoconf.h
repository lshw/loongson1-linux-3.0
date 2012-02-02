/*
 * Automatically generated C config: don't edit
 */

#define AUTOCONF_INCLUDED
#define RTL871X_MODULE_NAME "92CU"

//#define CONFIG_DEBUG_RTL871X 1

#define CONFIG_USB_HCI	1
#undef  CONFIG_SDIO_HCI
#undef CONFIG_PCIE_HCI

#undef CONFIG_RTL8711
#undef  CONFIG_RTL8712
#define	CONFIG_RTL8192C 1
#define	CONFIG_RTL8192D 1


//#define CONFIG_LITTLE_ENDIAN 1 //move to Makefile depends on platforms
//#undef CONFIG_BIG_ENDIAN

#undef PLATFORM_WINDOWS
#undef PLATFORM_OS_XP 
#undef PLATFORM_OS_CE


#define PLATFORM_LINUX 1

//#define CONFIG_PWRCTRL	1
//#define CONFIG_H2CLBK 1

//#define CONFIG_MP_INCLUDED 1

//#undef CONFIG_EMBEDDED_FWIMG
#define CONFIG_EMBEDDED_FWIMG 1

#define CONFIG_R871X_TEST 1

#define CONFIG_80211N_HT 1

#define CONFIG_RECV_REORDERING_CTRL 1

//#define CONFIG_RTL8712_TCP_CSUM_OFFLOAD_RX 1

//#define CONFIG_DRVEXT_MODULE 1


#define CONFIG_IPS	1
#ifdef CONFIG_IPS
	#define CONFIG_IPS_LEVEL2	
#endif
#define CONFIG_LPS	1
#define CONFIG_BT_COEXIST  	1
#define CONFIG_ANTENNA_DIVERSITY	1
//#define CONFIG_WOWLAN 1

#define SUPPORT_HW_RFOFF_DETECTED	1

#ifdef PLATFORM_LINUX
//	#define CONFIG_PROC_DEBUG 1
#endif

#ifdef CONFIG_RTL8192C

	#define DBG 0

	#define CONFIG_DEBUG_RTL8192C				1

	#define DEV_BUS_PCI_INTERFACE				1
	#define DEV_BUS_USB_INTERFACE				2	

	#define RTL8192C_WEP_ISSUE					0
	
	#define RTL8192C_RX_PACKET_NO_INCLUDE_CRC	1

	#define SUPPORTED_BLOCK_IO
	
	#ifdef CONFIG_USB_HCI

		#define DEV_BUS_TYPE	DEV_BUS_USB_INTERFACE

		#ifdef CONFIG_MINIMAL_MEMORY_USAGE
			#define USB_TX_AGGREGATION_92C	0
			#define USB_RX_AGGREGATION_92C	0
		#else
			#define USB_TX_AGGREGATION_92C	1
			#define USB_RX_AGGREGATION_92C	1		
		#endif
		
		#define CONFIG_PS_CMD				1

		#ifdef CONFIG_WISTRON_PLATFORM	
			#define SILENT_RESET_FOR_SPECIFIC_PLATFOM	1				
		#endif
		
		#define RTL8192CU_FW_DOWNLOAD_ENABLE	1

		#define CONFIG_ONLY_ONE_OUT_EP_TO_LOW	0
	
		#define CONFIG_OUT_EP_WIFI_MODE	0

		#define ENABLE_USB_DROP_INCORRECT_OUT	0

		#define RTL8192CU_ASIC_VERIFICATION	0	// For ASIC verification.

		#define RTL8192CU_ADHOC_WORKAROUND_SETTING 1

		#ifdef PLATFORM_LINUX
			#define CONFIG_SKB_COPY 			1//for amsdu
			#define CONFIG_PREALLOC_RECV_SKB	1			
			#define CONFIG_REDUCE_USB_TX_INT	1
			#define CONFIG_EASY_REPLACEMENT	1
			#ifdef CONFIG_WISTRON_PLATFORM
			#define DYNAMIC_ALLOCIATE_VENDOR_CMD	0
			#else
			#ifdef RTL8192C_RECONFIG_TO_1T1R
			#define DYNAMIC_ALLOCIATE_VENDOR_CMD	0
			#else
			#define DYNAMIC_ALLOCIATE_VENDOR_CMD	1
			#endif
			#endif
		#endif		

		#ifdef CONFIG_R871X_TEST

			//#define CONFIG_AP_MODE 1
			//#define CONFIG_NATIVEAP_MLME 1

			#ifdef CONFIG_AP_MODE

				#ifndef CONFIG_NATIVEAP_MLME
					#define CONFIG_HOSTAPD_MLME 1
				#endif
			
			#endif
		
		#endif
    
	
	#endif

	#ifdef CONFIG_PCIE_HCI

		#define DEV_BUS_TYPE	DEV_BUS_PCI_INTERFACE
		
	#endif

	
	#define DISABLE_BB_RF	0	

	#define RTL8191C_FPGA_NETWORKTYPE_ADHOC 0

	//#define FW_PROCESS_VENDOR_CMD 1

	#ifdef CONFIG_MP_INCLUDED
		#define MP_DRIVER 1
	#else
		#define MP_DRIVER 0
	#endif

#endif


//#define CONFIG_NON_SKB_TRANSFER_BUFFER 1

#ifdef CONFIG_NON_SKB_TRANSFER_BUFFER
#undef CONFIG_PREALLOC_RECV_SKB
#endif


#define MEM_ALLOC_REFINE
//#define DBG_MEM_ALLOC

#define RESUME_IN_WORKQUEUE // do rtw_resume in workqueue

//#define INDICATE_SCAN_COMPLETE_EVENT // not ready

#define HANDLE_JOINBSS_ON_ASSOC_RSP
//#define REJOIN

#define SET_STAKEY_WITHOUT_CMD
#define SETKEY_WITHOUT_CMD
#define DISCONNECT_HDL_ON_SUSPEND // call disconnect_hdl instead of cmd while rtw_suspend

//#define DBG_SHOW_USETKIPKEY
//#define DBG_RX_DECRYPTOR

//#define ENABLE_HW_ENC_TIMER
//#define ENABLE_HW_ENC_TIMEOUT 5000

#define POWER_SAVING_ALTER01 // remove the status checking of net_closed
//#define DBG_POWER_SAVING
//#define DBG_RF_STATE

//#define DBG_TX
//#define DBG_XMIT_BUF
//#define DBG_XMIT_FRAME

//#define DBG_RX
//#define DBG_RECV_BUF
//#define DBG_RECV_FRAME
//#define DBG_RX_DATA_TOGGLE
//#define DBG_RX_SHOW_802_1X

#define SITESURVEY_CBSSID_DATA

//#define JOIN_CMD_HDL_SET_REG_BSSID
//#define JOIN_CMD_HDL_RCR_CBSSID_DATA
//#define JOIN_CMD_HDL_RCR_CBSSID_BCN
//#define DBG_SETTING_RCR
//#define DBG_SET_NETYPE0_MSR

