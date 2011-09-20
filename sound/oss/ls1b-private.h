
/*
 * audio private data; 
 *
 */


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++
+ Object:
+ 1.stream/in f_ops->write /out f_ops_read
+ 2.buffers/in/out
+ 3.dma_desc /FIFO /buffers fragram
+ 4.dma_channel/to/ from devices
*++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/****************************************************
 *
 *
 *
 *
 ***************************************************/
#define Z_LEN	0x8
/*dumy*/
typedef struct audio_dma_desc {
	/* partition for hardware */
	u32 ordered;
	u32 saddr;
	u32 daddr;
	u32 length;
	u32 step_length;
	u32 step_times;
	u32 cmd;
	/* partition for software */
	dma_addr_t *dma_handle;
	u32 index[2];
	u32 init;
} audio_dmadesc_t;

typedef struct audio_buf {
	u32 offset;		//buffer num in audio_stream 
	audio_dmadesc_t *dma_desc;	//   
	u32 dma_user;		//user dma index;            
	volatile void *data;		//addr of buffer     
	u32 bytecount;
	u32 endaddr;
	void *master;
	u32 master_size;
} audio_buf_t;

typedef struct audio_stream {
	char *name;		//in or out
	audio_buf_t *buffers;	//pointer of buffer array
	u32 buffer_size;
	volatile u32 cur_buf, free_buf, rec_buf;
	audio_dmadesc_t *ask_dma;
	u32 nbfrags;
	u32 fragsize;
	u32 descs_per_frag;
	struct timer_list buf_timer;
	volatile u32 *dma_addr;	//the port for audio_dmadesc_t->data_addr;      
	volatile u32 *dma_size_cmd;
	volatile u32 *dma_state;
	struct semaphore sem;
	wait_queue_head_t stop_wq;
	wait_queue_head_t frag_wq;

	u32 output:1;
	u32 single:1;
	volatile u32 dma_status:1;
	u32 dma_break:1;
	volatile u32 start_timer:1;
	u32 fmt;
	u32 rate;
	u32 dma_daddr;
	u32 first_time;
	u32 sync_only_once;
	u32 placeholder;
	u32 zero[Z_LEN];
} audio_stream_t;


typedef struct audio_state {
	audio_stream_t *input_stream;
	audio_stream_t *output_stream;
	u32 dev_dsp;
	u32 rd_ref:1;
	u32 wr_ref:1;
	struct semaphore sem;
} audio_state_t;
