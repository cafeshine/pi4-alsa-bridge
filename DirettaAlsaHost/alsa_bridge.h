
#define DIRETTA_BR_DELETE 0
#define DIRETTA_BR_ATTACH 1
#define DIRETTA_BR_DETACH 2
#define DIRETTA_BR_WAKEUP 3
#define DIRETTA_BR_NOTIFY 4
#define DIRETTA_BR_EXIT 5



#define DA_STATUS_ACTIV	    (1<<0)
#define DA_STATUS_OPEN	    (1<<1)
#define DA_STATUS_CONNECT	(1<<2)
#define DA_STATUS_PLAY      (1<<3)
#define DA_STATUS_ERROR 	(1<<31)


#define DA_FORMAT_HZ_8000		(1<<0)
#define DA_FORMAT_HZ_44100		(1<<1)
#define DA_FORMAT_HZ_48000		(1<<2)
#define DA_FORMAT_HZ_88200		(1<<3)
#define DA_FORMAT_HZ_96000		(1<<4)
#define DA_FORMAT_HZ_176400		(1<<5)
#define DA_FORMAT_HZ_192000		(1<<6)
#define DA_FORMAT_HZ_352800		(1<<7)
#define DA_FORMAT_HZ_384000		(1<<8)
#define DA_FORMAT_HZ_705600		(1<<9)
#define DA_FORMAT_HZ_768000		(1<<10)
#define DA_FORMAT_HZ_1411200	(1<<11)
#define DA_FORMAT_HZ_1536000	(1<<12)
#define DA_FORMAT_HZ_2822400	(1<<13)
#define DA_FORMAT_HZ_3072000	(1<<14)
#define DA_FORMAT_HZ_5644800	(1<<15)
#define DA_FORMAT_HZ_6144000	(1<<16)
#define DA_FORMAT_HZ_11289600	(1<<17)
#define DA_FORMAT_HZ_12288000	(1<<18)
#define DA_FORMAT_HZ_22579200	(1<<19)
#define DA_FORMAT_HZ_24576000	(1<<20)
#define DA_FORMAT_HZ_45158400	(1<<21)
#define DA_FORMAT_HZ_49152000	(1<<22)
#define DA_FORMAT_HZ_90316800	(1<<23)
#define DA_FORMAT_HZ_98304000	(1<<24)
#define DA_FORMAT_HZ_180633600	(1<<25)
#define DA_FORMAT_HZ_196608000	(1<<26)

#define DA_FORMAT_TYPE_PCM_8		(1<<0)
#define DA_FORMAT_TYPE_PCM_16_LE	(1<<1)
#define DA_FORMAT_TYPE_PCM_16_BE	(1<<2)
#define DA_FORMAT_TYPE_PCM_24_LE	(1<<3)
#define DA_FORMAT_TYPE_PCM_24_BE	(1<<4)
#define DA_FORMAT_TYPE_PCM_24_4LE	(1<<5)
#define DA_FORMAT_TYPE_PCM_24_4BE	(1<<6)
#define DA_FORMAT_TYPE_PCM_32_LE	(1<<7)
#define DA_FORMAT_TYPE_PCM_32_BE	(1<<8)
#define DA_FORMAT_TYPE_PCM_64_LE	(1<<9)
#define DA_FORMAT_TYPE_PCM_64_BE	(1<<10)
#define DA_FORMAT_TYPE_PCM_FLOAT	(1<<11)
#define DA_FORMAT_TYPE_PCM_DOUBLE	(1<<12)
#define DA_FORMAT_TYPE_DSD_8_LSB		(1<<16)
#define DA_FORMAT_TYPE_DSD_8_MSB		(1<<17)
#define DA_FORMAT_TYPE_DSD_16_LSB_LE	(1<<18)
#define DA_FORMAT_TYPE_DSD_16_LSB_BE	(1<<19)
#define DA_FORMAT_TYPE_DSD_16_MSB_LE	(1<<20)
#define DA_FORMAT_TYPE_DSD_16_MSB_BE	(1<<21)
#define DA_FORMAT_TYPE_DSD_16_CS_LSB	(1<<22)
#define DA_FORMAT_TYPE_DSD_16_CS_MSB	(1<<23)
#define DA_FORMAT_TYPE_DSD_32_LSB_LE	(1<<24) //LE 12341234
#define DA_FORMAT_TYPE_DSD_32_LSB_BE	(1<<25) //BE 43214321 (not use)
#define DA_FORMAT_TYPE_DSD_32_MSB_LE	(1<<26) //LE 43214321
#define DA_FORMAT_TYPE_DSD_32_MSB_BE	(1<<27) //BE 12341234 (not use)

#define DA_FORMAT_TYPE_CH_1		(1<<0)
#define DA_FORMAT_TYPE_CH_2		(1<<1)
#define DA_FORMAT_TYPE_CH_3		(1<<2)
#define DA_FORMAT_TYPE_CH_4		(1<<3)
#define DA_FORMAT_TYPE_CH_5		(1<<4)
#define DA_FORMAT_TYPE_CH_6		(1<<5)
#define DA_FORMAT_TYPE_CH_7		(1<<6)
#define DA_FORMAT_TYPE_CH_8		(1<<7)
#define DA_FORMAT_TYPE_CH_9		(1<<8)
#define DA_FORMAT_TYPE_CH_10	(1<<9)
#define DA_FORMAT_TYPE_CH_11	(1<<10)
#define DA_FORMAT_TYPE_CH_12	(1<<11)
#define DA_FORMAT_TYPE_CH_13	(1<<12)
#define DA_FORMAT_TYPE_CH_14	(1<<13)
#define DA_FORMAT_TYPE_CH_15	(1<<14)
#define DA_FORMAT_TYPE_CH_16	(1<<15)


typedef struct _da_sync_mem_st{
	signed long long Delay;
	signed long long supportHz;
	signed long long supportTYPE;
	signed long long supportCH;
	char targetName[64];
	char sinkName[64];
	signed long long playTYPE;
	signed long long playHz;
	signed long long playCH;
	signed int statusTarget;
	signed int statusPlayer;
	
	unsigned int periodSizeMin;
	unsigned int periodSizeMax;
	unsigned int periodMin;
	unsigned int periodMax;
	unsigned int rd;
	unsigned int bufuse;
	unsigned int cntuse;
	unsigned int wd;
	unsigned int cd;
	unsigned int sd;
	unsigned int sds[32];
	
	unsigned int vol;
}da_sync_mem;

