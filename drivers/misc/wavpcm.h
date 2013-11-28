#ifndef __WAVPCM_H_
#define __WAVPCM_H_

#if !defined(_WAV_INFO_) 
#define _WAV_INFO_ 
#endif

// 一些和声音数据相关的宏 
#define SAMPLE_RATE          22050                  // sample rate，每秒22050个采样点 
#define QUANTIZATION         0x10                   // 16bit量化， 
#define BYTES_EACH_SAMPLE    0x2                    // QUANTIZATION / 8, 所以每个采样点、
// 是short，占个2个字节 
#define CHANNEL_NUN          0x1                    // 单声道 
#define FORMAT_TAG           0x1                    // 线性PCM 

// 一个wave file包括四个CHUNK，除了FACT之外，其它是必须的，并且第一个RIFF是整个文件的头， 
// 所以别名为WAV_HEADER，而不是RIFF 
/*------------------------Wave File Structure ------------------------------------ */ 
typedef struct RIFF_CHUNK
{ 
	  char			  fccID[4]; 			  // must be "RIFF"  
	  unsigned long   dwSize;				  // all bytes of the wave file subtracting 8,	
	// which is the size of fccID and dwSize 
	  char			  fccType[4];			  // must be "WAVE"  
}WAVE_HEADER; 
	// 12 bytes 
	
typedef struct FORMAT_CHUNK
{ 
	  char			  fccID[4]; 			  // must be "fmt " 
	  unsigned long   dwSize;				  // size of this struct, subtracting 8, which	
	// is the sizeof fccID and dwSize 
	  unsigned short  wFormatTag;			  // one of these: 1: linear,6: a law,7:u-law  
	  unsigned short  wChannels;			  // channel number 
	  unsigned long   dwSamplesPerSec;		  // sampling rate 
	  unsigned long   dwAvgBytesPerSec; 	 // bytes number per second  
	  unsigned short  wBlockAlign;			 // 每样本的数据位数(按字节算), 其值为:通道 
	// 数*每样本的数据位值/8，播放软件需要一次处
	// 理多个该值大小的字节数据, 以便将其值用于
	// 缓冲区的调整每样本占几个字节:  
	// NumChannels * uiBitsPerSample/8 
	 
	  unsigned short  uiBitsPerSample;		 // quantization 
}A_FORMAT; 
	
	// 24 bytes  
	// The fact chunk is required for all new WAVE formats. 
	// and is not required for the standard WAVE_FORMAT_PCM files 
	// 也就是说，这个结构体目前不是必须的，一般当wav文件由某些软件转化而成，则包含该Chunk 
	// 但如果这里写了，则必须是如下的结构，并且在四个结构体中的位置也要放在第三 

	 
	// 数据结构 
typedef struct
{ 
	  char			  fccID[4]; 	// must be "data" s
	  unsigned long   dwSize;			  //  byte_number of PCM data in byte 
}A_DATA;	 
	// 8 bytes 

typedef struct s_wav_audio
{
    WAVE_HEADER v_head;
	A_FORMAT v_fmt;
    A_DATA   v_data;
}S_WAV_FMT;
	
#endif

