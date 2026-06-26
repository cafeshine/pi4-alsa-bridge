/* CRITICAL: Prevent KASLR extern symbols (page_offset_base/vmemmap_base)
 * from being pulled in via kernel headers. Daphile does NOT export these.
 * CONFIG_DYNAMIC_MEMORY_LAYOUT (not RANDOMIZE_BASE) controls __PAGE_OFFSET,
 * making virt_to_phys() reference page_offset_base as an extern symbol.
 * vmemmap_base is always declared extern in page_64.h and used via SPARSEMEM_VMEMMAP.
 * The CI workflow patches:
 * 1. include/linux/kconfig.h: #undef CONFIG_RANDOMIZE_BASE + CONFIG_DYNAMIC_MEMORY_LAYOUT
 * 2. arch/x86/include/asm/page_64.h: replaces extern declarations with #define constants
 * 3. This file (via sed): inserts #undef before linux/module.h */
#include  <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mm.h>  
#include <linux/io.h>  
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/sched/signal.h>
#include <linux/wait.h>
#include <sound/core.h>
#include <sound/control.h>
#include <sound/tlv.h>
#include <sound/pcm.h>
#include <sound/rawmidi.h>
#include <sound/initval.h>
#include <linux/crc16.h>
#include <linux/cdev.h>
#include <linux/dcache.h>

#define DEBUG 1

#if DEBUG!=0
	#define printkd printk
#else
	#define printkd(...)
#endif


#ifdef __arm__
#define __BIT32__
#include <asm/div64.h>
#endif

#include "alsa_bridge.h"

#define MAX_TARGET 16
#define DEVICE_NAME "diretta-alsa"
typedef struct _diretta_alsa_node_st{
	struct device* dev;
	struct cdev cdev;
	unsigned char* buffer;
	da_sync_mem* info;
	
	struct snd_card *card;
	struct snd_pcm_substream *substream;
	
	wait_queue_head_t sync;
	int sync_wakeup;
	bool close;
	
	unsigned long wd;
	unsigned long bufuse;
	unsigned long cntuse;
	
	bool playbackOpen;
	struct page* pageinfo;
	int order;
	bool doStart;
	bool doPrStart;
	
	snd_pcm_uframes_t back_buffer_size;
	unsigned int back_periods;
	snd_pcm_format_t back_format;
	unsigned int back_rate;
	unsigned int back_channels;
}diretta_alsa_node_st;

typedef struct _diretta_alsa_st{
	int major;
	struct class *class;
	diretta_alsa_node_st target[MAX_TARGET];
	int exit;
	dev_t devn;
	
}diretta_alsa_st;

//
static diretta_alsa_st diretta_alsa;


static void diretta_alsa_wakeup(diretta_alsa_node_st* node){
	node->sync_wakeup = 1;
	wake_up_interruptible(&node->sync);
}

static int diretta_sleep(int ms){
	signed long timeout;
	signed long s,m;
	m = ms+1000/HZ-1;
	s = m/1000;
	m = m-s*1000;
	timeout = s*HZ + (m*HZ)/1000 + 1;
	set_current_state(TASK_INTERRUPTIBLE);
	if(signal_pending(current)){
		return -EINTR;
	}
	timeout = schedule_timeout(timeout);
	if(signal_pending(current)){
		return -EINTR;
	}
	return 0;
}
static int diretta_alsa_waite(diretta_alsa_node_st* node,int time,signed int wtflag){
	unsigned long start = jiffies;
	while(1){
		if (signal_pending(current)) {
			printk("stat pend\n");
			return -1;
	    }
		if(time <= (jiffies - start)*1000/HZ){
			//time out
			printk("stat timeourr\n");
			return 0;
		}
		if(node->info->statusTarget&wtflag){
			if(node->info->statusTarget&DA_STATUS_ERROR){
				printk("stat err\n");
				return 2;
			}else{
			//	printk("stat ok\n");
				return 1;
			}
		}
		if(node->info->statusTarget == 0){
			printk("stat err\n");
			return 3;
		}
		wait_event_interruptible_timeout(node->sync,node->sync_wakeup!=0,HZ/10);//100 msec
		node->sync_wakeup = 0;
		if(node->close)
			return 0;
	}
}
static int diretta_alsa_waite_clear(diretta_alsa_node_st* node,int time,signed int wtflag){
	unsigned long start = jiffies;
	while(1){
		if (signal_pending(current)) {
			printk("stat pend\n");
			return -1;
	    }
		if(time <= (jiffies - start)*1000/HZ){
			//time out
			printk("stat timeourr\n");
			return 0;
		}
		if((node->info->statusTarget&wtflag) == 0){
			if(node->info->statusTarget&DA_STATUS_ERROR){
				printk("stat err\n");
				return 2;
			}else{
			//	printk("stat clear\n");
				return 1;
			}
		}
		if(node->info->statusTarget == 0){
			printk("stat err\n");
			return 3;
		}
		wait_event_interruptible_timeout(node->sync,node->sync_wakeup!=0,HZ/10);//100 msec
		node->sync_wakeup = 0;
		if(node->close)
			return 0;
	}
}
static void diretta_alsa_notify(diretta_alsa_node_st* node){
	if(!node->playbackOpen)
		return;
	snd_pcm_period_elapsed(node->substream);
}
static int snd_card_diretta_playback_open(struct snd_pcm_substream *substream){
	
	diretta_alsa_node_st* node = substream->pcm->private_data;
	int a;
	signed long long fntHz,minHz,maxHz;
#ifdef __BIT32__
	signed long temp;
#endif
	signed long long fntCh,minCh,maxCh;
	if(node->close)
		return -EFAULT;
	
	printkd("snd_card_diretta_playback_open \n");
	
	
	memset(&substream->runtime->hw,0,sizeof(substream->runtime->hw));
	
	substream->runtime->hw.info = SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_PAUSE ;
	substream->runtime->hw.formats = 0;
	if(node->info->supportTYPE&DA_FORMAT_TYPE_PCM_8)
		substream->runtime->hw.formats |= SNDRV_PCM_FMTBIT_S8;
	if(node->info->supportTYPE&DA_FORMAT_TYPE_PCM_16_LE)
		substream->runtime->hw.formats |= SNDRV_PCM_FMTBIT_S16_LE;
	if(node->info->supportTYPE&DA_FORMAT_TYPE_PCM_24_LE)
		substream->runtime->hw.formats |= SNDRV_PCM_FMTBIT_S24_3LE;
	if(node->info->supportTYPE&DA_FORMAT_TYPE_PCM_32_LE)
		substream->runtime->hw.formats |= SNDRV_PCM_FMTBIT_S32_LE;
	if(node->info->supportTYPE&DA_FORMAT_TYPE_PCM_FLOAT)
		substream->runtime->hw.formats |= SNDRV_PCM_FMTBIT_FLOAT_LE;
	if(node->info->supportTYPE&DA_FORMAT_TYPE_PCM_DOUBLE)
		substream->runtime->hw.formats |= SNDRV_PCM_FMTBIT_FLOAT64_LE;
#ifdef SNDRV_PCM_FMTBIT_DSD_U32_LE
	if(node->info->supportTYPE&DA_FORMAT_TYPE_DSD_32_MSB_LE)
		substream->runtime->hw.formats |= SNDRV_PCM_FMTBIT_DSD_U32_LE;
#endif
#ifdef SNDRV_PCM_FMTBIT_DSD_U32_BE
	if(node->info->supportTYPE&DA_FORMAT_TYPE_DSD_32_MSB_BE)
		substream->runtime->hw.formats |= SNDRV_PCM_FMTBIT_DSD_U32_BE;
#endif
	
//	printk("snd_card_diretta_playback_open formats=%lld\n",substream->runtime->hw.formats);
	
	
	substream->runtime->hw.rates = 0;
	if(node->info->supportHz&DA_FORMAT_HZ_8000)
		substream->runtime->hw.rates |= SNDRV_PCM_RATE_8000;
	if(node->info->supportHz&DA_FORMAT_HZ_44100)
		substream->runtime->hw.rates |= SNDRV_PCM_RATE_44100;
	if(node->info->supportHz&DA_FORMAT_HZ_48000)
		substream->runtime->hw.rates |= SNDRV_PCM_RATE_48000;
	if(node->info->supportHz&DA_FORMAT_HZ_88200)
		substream->runtime->hw.rates |= SNDRV_PCM_RATE_88200;
	if(node->info->supportHz&DA_FORMAT_HZ_96000)
		substream->runtime->hw.rates |= SNDRV_PCM_RATE_96000;
	if(node->info->supportHz&DA_FORMAT_HZ_176400)
		substream->runtime->hw.rates |= SNDRV_PCM_RATE_176400;
	if(node->info->supportHz&DA_FORMAT_HZ_192000)
		substream->runtime->hw.rates |= SNDRV_PCM_RATE_192000;
	
	substream->runtime->hw.rates |= SNDRV_PCM_RATE_CONTINUOUS;
	
//	printk("snd_card_diretta_playback_open rates=%d\n",substream->runtime->hw.rates);
	
	
	minHz=-1;
	maxHz=-1;
	
	
	fntHz = node->info->supportHz;
	if(fntHz&DA_FORMAT_HZ_8000){
		minHz = 8000;
	}else{
		fntHz>>=1;
		for(a=0;a<=30;++a){
			if(fntHz&3){
				if(fntHz&1)
					minHz = 44100*(1<<a);
				else if(fntHz&2)
					minHz = 48000*(1<<a);
				break;
			}
			fntHz>>=2;
		}
	}
	fntHz = node->info->supportHz>>1;
	for(a=0;a<=30;++a){
		if(fntHz&(3ll<<(30*2))){
			if(fntHz&(2ll<<(30*2)))
				maxHz = 48000*(1<<(30-a));
			else if(fntHz&(1ll<<(30*2)))
				maxHz = 44100*(1<<(30-a));
			
			
			break;
		}
		fntHz<<=2;
	}
	if(maxHz==-1){
		if(fntHz&DA_FORMAT_HZ_8000)
			maxHz = 8000;
	}
	
	
//	printk("snd_card_diretta_playback_open rate_min=%lld\n",minHz);
//	printk("snd_card_diretta_playback_open rate_max=%lld\n",maxHz);
	
	if(maxHz == -1 ||  minHz == -1)
		return -1;

	substream->runtime->hw.rate_min = minHz;
	substream->runtime->hw.rate_max = maxHz;
	
	
	minCh=-1;
	maxCh=-1;
	
	fntCh = node->info->supportCH;
	for(a=0;a<30;++a){
		if(fntCh&1){
			minCh = a+1;
			break;
		}
		fntCh>>=1;
	}
	fntCh = node->info->supportCH;
	for(a=0;a<30;++a){
		if(fntCh&(1ll<<30)){
			maxCh = 30-a+1;
			break;
		}
		fntCh<<=1;
	}
	
//	printk("snd_card_diretta_playback_open channels_min=%lld\n",minCh);
//	printk("snd_card_diretta_playback_open channels_max=%lld\n",maxCh);

	if(maxCh == -1 ||  minCh == -1)
		return -1;
	
	substream->runtime->hw.channels_min = minCh;
	substream->runtime->hw.channels_max = maxCh;
	
	
	
	substream->runtime->hw.buffer_bytes_max =	node->info->periodSizeMax*node->info->periodMax;
	
	substream->runtime->hw.period_bytes_min = node->info->periodSizeMin;
	substream->runtime->hw.period_bytes_max = node->info->periodSizeMax;
	substream->runtime->hw.periods_min =		node->info->periodMin;
	substream->runtime->hw.periods_max =		node->info->periodMax;
	substream->runtime->hw.fifo_size =		0;
	
	printkd("snd_card_diretta_playback_open period_bytes_min=%d\n",(int)substream->runtime->hw.period_bytes_min);
	printkd("snd_card_diretta_playback_open period_bytes_max=%d\n",(int)substream->runtime->hw.period_bytes_max);
	printkd("snd_card_diretta_playback_open periods_min=%d\n",substream->runtime->hw.periods_min);
	printkd("snd_card_diretta_playback_open periods_max=%d\n",substream->runtime->hw.periods_max);
	
	
	node->info->statusPlayer =  DA_STATUS_OPEN;
	node->substream = substream;
	 
	node->playbackOpen = true;
	node->doStart = false;
	node->doPrStart = false;
//	printk("snd_card_diretta_playback_open done\n");
	
	return 0;
	
	
}
static int snd_card_diretta_playback_close(struct snd_pcm_substream *substream){
	diretta_alsa_node_st* node = substream->pcm->private_data;
	printkd("snd_card_diretta_playback_close \n");
	node->playbackOpen = false;
	
	node->info->statusPlayer &=  ~DA_STATUS_CONNECT;
	diretta_alsa_waite_clear(node,2000,DA_STATUS_CONNECT);
	
	node->info->statusPlayer = 0;
	node->doStart = false;
	node->doPrStart = false;
	return 0;

}
static int snd_card_diretta_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params){
	diretta_alsa_node_st* node = substream->pcm->private_data;
	if(node->close)
		return -EFAULT;
	//printk("snd_card_diretta_hw_params \n");
	return 0;
}
static int snd_card_diretta_pcm_prepare(struct snd_pcm_substream *substream){
	diretta_alsa_node_st* node = substream->pcm->private_data;
	struct snd_pcm_runtime *runtime= substream->runtime;
	unsigned long rate = runtime->rate;
	if(node->close)
		return -EFAULT;
	if(node->doPrStart || node->doStart){
		if(	node->back_buffer_size == runtime->buffer_size &&
			node->back_periods == runtime->periods  &&
			node->back_format == runtime->format &&
			node->back_rate == runtime->rate &&
			node->back_channels == runtime->channels ){
			printk("snd_card_diretta_pcm_prepare reject\n");
			return 0;
		}
	}
	
	node->back_buffer_size = runtime->buffer_size;
	node->back_periods = runtime->periods;
	node->back_format = runtime->format;
	node->back_rate = runtime->rate;
	node->back_channels = runtime->channels;
	printkd("snd_card_diretta_pcm_prepare buffer_size=%d / periods=%d  format=%d  rate=%d  ch=%d \n",(int)runtime->buffer_size,runtime->periods,runtime->format,runtime->rate,runtime->channels);
	
	node->info->statusPlayer &=  ~DA_STATUS_CONNECT;
	diretta_alsa_waite_clear(node,2000,DA_STATUS_CONNECT);
	
	switch(runtime->format){
	case SNDRV_PCM_FORMAT_S8:
		node->info->playTYPE = DA_FORMAT_TYPE_PCM_8;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		node->info->playTYPE = DA_FORMAT_TYPE_PCM_16_LE;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		node->info->playTYPE = DA_FORMAT_TYPE_PCM_24_LE;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		node->info->playTYPE =  DA_FORMAT_TYPE_PCM_32_LE;
		break;
	case SNDRV_PCM_FORMAT_FLOAT_LE:
		node->info->playTYPE =  DA_FORMAT_TYPE_PCM_FLOAT;
		break;
#ifdef SNDRV_PCM_FORMAT_DSD_U32_LE
	case SNDRV_PCM_FORMAT_DSD_U32_LE:
		node->info->playTYPE =  DA_FORMAT_TYPE_DSD_32_MSB_LE;
		rate *= 32;
		break;
#endif
#ifdef SNDRV_PCM_FORMAT_DSD_U32_BE
	case SNDRV_PCM_FORMAT_DSD_U32_BE:
		node->info->playTYPE =  DA_FORMAT_TYPE_DSD_32_MSB_BE;
		rate *= 32;
		break;
#endif
	default:
		return -1;
	}
	if((rate%44100) == 0){
		node->info->playHz = (rate/44100)*(rate/44100)*2;
	}else if((rate%48000)  == 0){
		node->info->playHz = (rate/48000)*(rate/48000)*4;
	}else if((rate%8000)  == 0){
		node->info->playHz = DA_FORMAT_HZ_8000;
	}else{
		return -1;
	}
	node->info->playCH = 1<<(runtime->channels-1);
	
	node->wd = node->info->wd=0;
	node->info->cd =0;
	node->info->sd =0;
	memset(node->info->sds,0,sizeof(node->info->sds));
	
	node->bufuse = node->info->bufuse = frames_to_bytes(runtime, runtime->buffer_size ) ;
	node->cntuse = node->info->cntuse = runtime->periods;
	node->info->statusPlayer |=  DA_STATUS_CONNECT;
	
	if(diretta_alsa_waite(node,10000,DA_STATUS_CONNECT)!=1)
		return -1;

	node->doPrStart = true;
	return 0;

}
static int snd_card_diretta_pcm_trigger(struct snd_pcm_substream *substream, int cmd){
	diretta_alsa_node_st* node = substream->pcm->private_data;
	printkd("snd_card_diretta_pcm_trigger %d\n",cmd);
	if(node->close)
		return -EFAULT;
	if(cmd){
		if(cmd!=2){
			node->info->statusPlayer |=  DA_STATUS_PLAY;
			node->doStart = true;
			node->doPrStart = false;
		}else{//pauser
			node->info->statusPlayer &= ~DA_STATUS_PLAY;
		}
	}else{
		node->info->statusPlayer &= ~DA_STATUS_PLAY;
		node->doPrStart = false;
	}
	return 0;

}
static snd_pcm_uframes_t snd_card_diretta_pcm_pointer(struct snd_pcm_substream *substream){
	struct snd_pcm_runtime *runtime = substream->runtime;
	diretta_alsa_node_st* node = substream->pcm->private_data;
	//if(node->close)
	//	return 0;
//	printk("snd_card_diretta_pcm_pointer %d %d\n",node->info->rd,node->info->wd);
	
	
	return bytes_to_frames(runtime,node->info->rd);

}
static int snd_card_diretta_pcm_mmap(struct snd_pcm_substream *substream, struct vm_area_struct *vma){
	printk("diretta_map \n");
	return 0;

}
		    
static int snd_card_diretta_pcm_copy(struct snd_pcm_substream *substream,
			  int channel, unsigned long pos,
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0) && !defined(RHEL_RELEASE_CODE)
			  void __user *src, unsigned long bytes)
#else
			  struct iov_iter *iter, unsigned long bytes)
#endif
{
//	struct snd_pcm_runtime *runtime = substream->runtime;
	diretta_alsa_node_st* node = substream->pcm->private_data;
	size_t size = bytes;
	unsigned long wd = node->wd;
	unsigned long bufuse = node->bufuse;
	if(node->close)
		return -EFAULT;
	while(size){
		size_t can = bufuse - wd;
		size_t cpsize = size;
		if(cpsize>can)
			cpsize = can;
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0) && !defined(RHEL_RELEASE_CODE)
		if(copy_from_user(node->buffer+wd  ,src,cpsize)!=0){
			printk("snd_card_diretta_pcm_copy bad\n");
			return -EFAULT;
		}
#else
		if (copy_from_iter(node->buffer+wd, cpsize, iter) != cpsize){
			printk("snd_card_diretta_pcm_copy bad\n");
			return -EFAULT;
		}
#endif
		wd +=cpsize;
		size-=cpsize;
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0) && !defined(RHEL_RELEASE_CODE)
		src+=cpsize;
#endif
		if(wd >= bufuse)
			wd=0;
	}
	node->info->wd = node->wd = wd;
	node->info->sd+=bytes;
	node->info->sds[node->info->cd&((sizeof(node->info->sds)/sizeof(node->info->sds[0]))-1)]=bytes;
	node->info->cd++;
	return 0;
}
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0) && !defined(RHEL_RELEASE_CODE)
static int snd_card_diretta_pcm_copy_kernel(struct snd_pcm_substream *substream,
				 int channel, unsigned long pos,
				 void *src, unsigned long count)
{
	printk("copy kernel\n");
	return 0; /* do nothing */
}
#endif
static int snd_card_diretta_fill_silence(struct snd_pcm_substream *substream, int channel,unsigned long pos, unsigned long count){
//	struct snd_pcm_runtime *runtime = substream->runtime;
	diretta_alsa_node_st* node = substream->pcm->private_data;
	size_t size = count;
	unsigned long wd = node->wd;
	unsigned long bufuse = node->bufuse;
	unsigned char mute = 0;
	if(node->close)
		return -EFAULT;
	
	if(node->info->playTYPE == DA_FORMAT_TYPE_DSD_32_MSB_LE || node->info->playTYPE == DA_FORMAT_TYPE_DSD_32_MSB_BE)
		mute=0xA5;
	
	while(size){
		size_t can = bufuse - wd;
		size_t cpsize = size;
		if(cpsize>can)
			cpsize = can;
		memset(node->buffer+wd,mute,cpsize);
		wd +=cpsize;
		size-=cpsize;
		if(wd >= bufuse)
			wd=0;
	}
	node->info->wd = node->wd = wd;
	node->info->cd++;
	return 0;

}
static struct snd_pcm_ops snd_card_diretta_alsa_playback_ops = {
	.open =			snd_card_diretta_playback_open,
	.close =		snd_card_diretta_playback_close,
	.ioctl =		snd_pcm_lib_ioctl,
	.hw_params =	snd_card_diretta_hw_params,
	.prepare =		snd_card_diretta_pcm_prepare,
	.trigger =		snd_card_diretta_pcm_trigger,
	.pointer =		snd_card_diretta_pcm_pointer,
	.mmap  =		snd_card_diretta_pcm_mmap,
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0) && !defined(RHEL_RELEASE_CODE)
	.copy_user =	snd_card_diretta_pcm_copy,
	.copy_kernel =	snd_card_diretta_pcm_copy_kernel,
#else
	.copy =	snd_card_diretta_pcm_copy,
#endif

	
	.fill_silence = snd_card_diretta_fill_silence,
};


static int snd_card_diretta_alsa_volume_info(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_info *uinfo){
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 10000;
	return 0;
}

static int aica_pcmvolume_get(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value *ucontrol){
	diretta_alsa_node_st* node = kcontrol->private_data;
	ucontrol->value.integer.value[0] = node->info->vol-1;
	return 0;
}

static int aica_pcmvolume_put(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value *ucontrol){
	diretta_alsa_node_st* node = kcontrol->private_data;
	unsigned int vol = ucontrol->value.integer.value[0];
	if (vol > 10000)
		return -EINVAL;
	node->info->vol = vol+1;
	return 1;
}

static const struct snd_kcontrol_new snd_card_diretta_alsa_volume_ops = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Volume",
	.index = 0,
	.info = snd_card_diretta_alsa_volume_info,
	.get = aica_pcmvolume_get,
	.put = aica_pcmvolume_put
};

static int diretta_alsa_add_sound(diretta_alsa_node_st* node){
	int ret;
	struct snd_pcm *pcm;
	size_t logLen = 0;
	
	
	char lname[sizeof(node->card->longname)];
	char dname[sizeof(node->card->longname)];
	u16 crc;
	if(node->info->sinkName[0]=='\0'){//no sink name
		strscpy(lname , node->info->targetName,sizeof(lname ));
	}else{
		size_t s1 = strscpy(lname, node->info->targetName,sizeof(lname));
		logLen = s1;
		if(sizeof(lname) > s1+1){
			size_t s2 = strscpy(lname+s1 , ":",sizeof(lname)-s1);
			logLen = strscpy(lname+s1+s2 , node->info->sinkName,sizeof(lname)-s1-s2)+s1+s2;
		}
	}
	crc = crc16(0xffff,lname,strlen(lname));
	dname[ 0]='D';
	dname[ 1]='i';
	dname[ 2]='r';
	dname[ 3]='e';
	dname[ 4]='t';
	dname[ 5]='t';
	dname[ 6]='a';
	dname[ 7]=((crc>> 0)&0xF)<10?((crc>> 0)&0xF)+'0':((crc>> 0)&0xF)-10+'A';
	dname[ 8]=((crc>> 4)&0xF)<10?((crc>> 4)&0xF)+'0':((crc>> 4)&0xF)-10+'A';
	dname[ 9]=((crc>> 8)&0xF)<10?((crc>> 8)&0xF)+'0':((crc>> 8)&0xF)-10+'A';
	dname[10]=((crc>>12)&0xF)<10?((crc>>12)&0xF)+'0':((crc>>12)&0xF)-10+'A';
	dname[11]='\0';

	
	
	ret = snd_card_new(node->dev , -1, dname, THIS_MODULE, 0  ,&node->card);
	printkd("snd_card_new %d [%s]\n",ret,dname);
	if(ret<0) {
		printk("diretta_alsa_add_sound: snd_card_new FAILED ret=%d dname=%s lname=%s targetName=%s sinkName=%s\n",
		       ret, dname, lname, node->info->targetName, node->info->sinkName);
		return -ENOSYS;
	}
	
	ret = snd_pcm_new(node->card, lname, 0, 1, 0, &pcm);
	printkd("snd_pcm_new %d [%s]\n",ret,lname);
	if(ret){
		return ret;
	}
	
	strscpy(node->card->driver, "DAlsaBridge",sizeof(node->card->driver));

	if(node->info->sinkName[0]=='\0'){//no sink name
		strscpy(pcm->name            , node->info->targetName,sizeof(pcm->name            ));
		strscpy(node->card->longname , node->info->targetName,sizeof(node->card->longname ));
		strscpy(node->card->shortname, node->info->targetName,sizeof(node->card->shortname));
	}else{//had sink name
		strscpy(pcm->name, node->info->sinkName,sizeof(pcm->name));
		strscpy(node->card->longname, lname,sizeof(node->card->longname));
		if(logLen == 0 || logLen >  sizeof(node->card->shortname)-1){
			strscpy(node->card->shortname, node->info->targetName,sizeof(node->card->shortname));
		}else{
			strscpy(node->card->shortname, lname,sizeof(node->card->shortname));
		}
	}
	

	printkd("snd_pcm_new %s\n",node->card->longname);
	
	
	pcm->private_data = node;
	
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK,&snd_card_diretta_alsa_playback_ops);
	
	if(node->info->vol != 0){
		snd_ctl_add(node->card, snd_ctl_new1(&snd_card_diretta_alsa_volume_ops, node));
	}
	
	node->close = false;
	
	ret = snd_card_register(node->card);
	printkd("snd_card_register %d\n",ret);
	if(ret){
		return ret;
	}
	return 0;
}
static int diretta_alsa_del_sound(diretta_alsa_node_st* node){
	node->close = true;
	if(node->card !=NULL){
		struct snd_card *card = node->card;
		int ret = diretta_sleep(500);
		if(ret){
			return ret;
		}
		node->card = NULL;
		snd_card_free(card);
	}
	return 0;
}





static ssize_t diretta_alsa_show_adapter_name(struct device *dev, struct device_attribute *attr, char *buf)
{
	int minor = MINOR(dev->devt);
	if(minor ==0)
		return sprintf(buf, "diretta-alsa\n");
	else
		return sprintf(buf, "diretta-alsa_%d\n",minor);
}
static DEVICE_ATTR(name, S_IRUGO, diretta_alsa_show_adapter_name, NULL);

static const struct file_operations diretta_alsa_fops;
static int diretta_alsa_add_dev(diretta_alsa_node_st* node,size_t size){
	int minor = node - diretta_alsa.target;
	int ret;

	if(size){
		int order = get_order(size +sizeof(da_sync_mem));
		
		if(node->pageinfo != NULL){
			if(order != node->order){
				//size_t a;
				//printk("free=%d\n",node->order);
				//for(a=0;a<(PAGE_SIZE<<node->order);a+=PAGE_SIZE){
				//	put_page(virt_to_page(node->buffer+a));
				//}
				__free_pages(node->pageinfo,node->order);
				node->pageinfo = NULL;
			}
		}
		if(node->pageinfo == NULL){
			//size_t a;
			node->order = order;
			node->pageinfo = alloc_pages(GFP_KERNEL ,order);
			//printk("alloc=%d %llx\n",node->order , (long long unsigned int)node->pageinfo);
			node->buffer = page_address(node->pageinfo);
			//for(a=0;a<(PAGE_SIZE<<node->order);a+=PAGE_SIZE){
			//	get_page(virt_to_page(node->buffer+a));
			//}
		}
		
		memset(node->buffer,0,PAGE_SIZE<<node->order);
		node->info = (da_sync_mem*)(node->buffer+size);
	}
	
	init_waitqueue_head(&node->sync);
	
	/* register_chrdev handles cdev registration for minor 0;
	 * only do per-target cdev_init+cdev_add for dynamically-added targets (minor>0) */
	if(minor > 0){
		cdev_init(&node->cdev, &diretta_alsa_fops);
		node->cdev.owner = THIS_MODULE;
		ret = cdev_add(&node->cdev, MKDEV(diretta_alsa.major, minor), 1);
		if (ret) {
			printk("diretta_alsa_add_dev: cdev_add failed for minor %d ret=%d\n", minor, ret);
			return ret;
		}
	}
	
	if(minor ==0)
		node->dev = device_create(diretta_alsa.class, NULL, MKDEV(diretta_alsa.major, minor),NULL, "diretta-alsa");
	else
		node->dev = device_create(diretta_alsa.class, NULL, MKDEV(diretta_alsa.major, minor),NULL, "diretta-alsa_%d",minor);
	if (IS_ERR(node->dev)) {
		if(minor > 0) cdev_del(&node->cdev);
		ret = PTR_ERR(node->dev);
		node->dev = 0;
		return ret;
	}
	ret = device_create_file(node->dev, &dev_attr_name);
	if (ret) {
		if(minor > 0) cdev_del(&node->cdev);
		device_destroy(diretta_alsa.class, MKDEV(diretta_alsa.major, minor));
		node->dev = 0;
		return ret;
	}
	
	return 0;
}
static void diretta_alsa_del_dev(diretta_alsa_node_st* node ,int hard){
	int minor = node - diretta_alsa.target;
	/* register_chrdev handles cdev for minor 0; only cdev_del for per-target cdev_add */
	if(minor > 0 && node->cdev.dev)
		cdev_del(&node->cdev);
	if(node->dev){
		device_remove_file(node->dev, &dev_attr_name);
		device_destroy(diretta_alsa.class, MKDEV(diretta_alsa.major, minor));
		node->dev = 0;
	}
	if(hard){
		if(node->pageinfo != NULL){
			//size_t a;
			//printk("free=%d %llx\n",node->order , (long long unsigned int)node->pageinfo);
			
			//for(a=0;a<(PAGE_SIZE<<node->order);a+=PAGE_SIZE){
			//	put_page(virt_to_page(node->buffer+a));
			//	printk("put_page=%d \n",a);
			//}
			__free_pages(node->pageinfo,node->order);
			node->pageinfo = NULL;
			node->order = 0;
			node->buffer = 0;
		}
	}
}

static long diretta_alsa_ioctl(struct file *file, unsigned int cmd, unsigned long arg){
	
	int ret,a;
//	printk("diretta_alsa_ioctl %d %d\n",cmd,(int)arg);
	if(diretta_alsa.exit){
		return -ENOSYS;
	}
	cmd &= 0xFF;
	if(cmd==0){
		// create vridge dev
		if(arg){
			for(a=1;a<MAX_TARGET;++a){
				if(diretta_alsa.target[a].dev!=NULL)
					continue;
				ret = diretta_alsa_add_dev(&diretta_alsa.target[a], arg);
				if(ret)
					return ret;
				return a;
			}
			return -ENOSYS;
		}else{
			//all delete
			printkd("diretta_alsa_ioctl DELETE ALL\n");
			for(a=MAX_TARGET-1;a>=1;--a){
				ret = diretta_alsa_del_sound(&diretta_alsa.target[a]);
				if(ret)
					return -ERESTARTSYS;
				diretta_alsa_del_dev(&diretta_alsa.target[a],0);
			}
			return 0;
		}
	}
	if(cmd){
		if(arg == DIRETTA_BR_DELETE){
			printkd("diretta_alsa_ioctl DELETE %d\n",cmd);
			if(cmd>=MAX_TARGET)
				return -ENOSYS;
			ret = diretta_alsa_del_sound(&diretta_alsa.target[cmd]);
			if(ret)
				return -ERESTARTSYS;
			diretta_alsa_del_dev(&diretta_alsa.target[cmd],0);
			return 0;
		}
		if(arg == DIRETTA_BR_ATTACH){
			printkd("diretta_alsa_ioctl ATTACH %d\n",cmd);
			return diretta_alsa_add_sound(&diretta_alsa.target[cmd]);
		}
		if(arg == DIRETTA_BR_DETACH){
			printkd("diretta_alsa_ioctl DETACH %d\n",cmd);
			ret = diretta_alsa_del_sound(&diretta_alsa.target[cmd]);
			if(ret)
				return -ERESTARTSYS;
			return 0;
		}
		if(arg == DIRETTA_BR_WAKEUP){
			//printk("diretta_alsa_ioctl WAKEUP %d\n",cmd);
			if(diretta_alsa.target[cmd].close)
				return 0;
			diretta_alsa_wakeup(&diretta_alsa.target[cmd]);
			return 0;
		}
		if(arg == DIRETTA_BR_NOTIFY){
			//printk("diretta_alsa_ioctl NOTIFY %d\n",cmd);
			if(diretta_alsa.target[cmd].close)
				return -EFAULT;
			diretta_alsa_notify(&diretta_alsa.target[cmd]);
			return 0;
		}
		if(arg == DIRETTA_BR_EXIT){
			printkd("diretta_alsa EXIT SET\n");
			diretta_alsa.exit=1;
		}
		return -ENOSYS;
	}
	
	return -ENOSYS;
}
static int diretta_alsa_open(struct inode *inode, struct file *file){
	/* Parse the device node filename to determine minor number.
	 * Daphile kernel's struct inode layout differs from our CI build,
	 * so iminor()/i_rdev return garbage.  File name is always correct
	 * regardless of inode layout.
	 *
	 * Device naming convention from register_chrdev():
	 *   "diretta-alsa"        → minor 0
	 *   "diretta-alsa_<N>"    → minor N (N = 1, 2, 3, ...)
	 */
	const unsigned char *name = file->f_path.dentry->d_name.name;
	const char *devname = DEVICE_NAME;
	size_t prefix_len = strlen(devname);
	unsigned int minor = 0;

	if (strncmp((const char *)name, devname, prefix_len) != 0)
		return -ENXIO;

	name += prefix_len;
	if (*name == '_') {
		int ret;
		unsigned int m;
		ret = kstrtouint((const char *)(name + 1), 10, &m);
		if (ret || m >= MAX_TARGET)
			return -ENXIO;
		minor = m;
	} else if (*name != '\0') {
		return -ENXIO;
	}

	file->private_data = &diretta_alsa.target[minor];
	diretta_alsa.exit = 0;
	printk("diretta_alsa_open: target[%u] private_data=%px\n",
	       minor, &diretta_alsa.target[minor]);

	return 0;
}
static int diretta_alsa_release(struct inode *inode, struct file *file){
	diretta_alsa_node_st*  node = file->private_data;
	
	if(&diretta_alsa.target[0] == node)
		return 0;
	
	if(diretta_alsa.exit){
		return 0;
	}
	
	printkd("diretta_alsa_release DELETE\n");
	
	/* ignore signal_pending — release path must proceed */
	diretta_alsa_del_sound(node);
	diretta_alsa_del_dev(node,0);
	
	return 0;
}
static int diretta_alsa_mmap(struct file *file, struct vm_area_struct *vma){
	diretta_alsa_node_st* node = file->private_data;
	
	/* remap_pfn_range internally sets VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP
	 * so we don't need to set vm_ops or vm_flags ourselves.
	 * Using PAGE_SHARED instead of vma->vm_page_prot to avoid struct layout dependency
	 * (struct vm_area_struct offsets differ in Daphile's custom kernel). */
	if(node->buffer == NULL)
		return -ENOMEM;
	
	return remap_pfn_range(vma, vma->vm_start,
			       virt_to_phys(node->buffer) >> PAGE_SHIFT,
			       vma->vm_end - vma->vm_start,
			       PAGE_SHARED);
}


static const struct file_operations diretta_alsa_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= diretta_alsa_ioctl,
	.open		= diretta_alsa_open,
	.release	= diretta_alsa_release,
	.mmap = diretta_alsa_mmap,
};



static void diretta_alsa_clean(void){

	int a;
	
	for(a=MAX_TARGET-1;a>=1;--a){
		/* ignore signal_pending — module cleanup */
		diretta_alsa_del_sound(&diretta_alsa.target[a]);
		diretta_alsa_del_dev(&diretta_alsa.target[a],1);
	}
	
	diretta_alsa_del_dev(&diretta_alsa.target[0],0);
	
	if(diretta_alsa.class)
		class_destroy(diretta_alsa.class);
	
	if(diretta_alsa.major)
		unregister_chrdev(diretta_alsa.major, DEVICE_NAME);
	diretta_alsa.major=0;
	
}
static int __init diretta_alsa_init(void){
	
	int ret;
	printk("diretta_alsa_init\n");
	
	memset(&diretta_alsa,0,sizeof(diretta_alsa));
	
	diretta_alsa.major = register_chrdev(0, DEVICE_NAME, &diretta_alsa_fops);
	if(diretta_alsa.major <= 0){
		if(diretta_alsa.major==0)
			diretta_alsa.major = -EINVAL;
		diretta_alsa_clean();
		return diretta_alsa.major;
	}
	
	
	#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 4, 0) && !defined(RHEL_RELEASE_CODE)
	  diretta_alsa.class = class_create(THIS_MODULE, "diretta-alsa");
	#else
	  diretta_alsa.class = class_create("diretta-alsa");
	#endif	
	
	if (IS_ERR(diretta_alsa.class)) {
		ret = PTR_ERR(diretta_alsa.class);
		diretta_alsa.class = NULL;
		diretta_alsa_clean();
		return ret;
	}
	
	ret = diretta_alsa_add_dev(&diretta_alsa.target[0],0);
	if (ret<0){
		diretta_alsa_clean();
		return ret;
	}

	
	return 0;
}
static void __exit diretta_alsa_exit(void){

	diretta_alsa_clean();
	printkd("diretta_alsa_exit\n");

}

module_init(diretta_alsa_init);
module_exit(diretta_alsa_exit);

MODULE_AUTHOR("yu@diretta.link");
MODULE_DESCRIPTION("Diretta ALSA driver");
MODULE_LICENSE("Dual BSD/GPL");
