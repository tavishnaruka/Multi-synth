#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <math.h>
#include <alsa/asoundlib.h>

#define BUFSIZE 1024

int16_t buf[BUFSIZE];

snd_output_t *output = NULL;
snd_pcm_t *handle;
snd_pcm_sframes_t frames;
static char *device = "default";


int sync_fd1, sync_fd2;//see main()

void fill_buffer()
{	
	while(1)
	{
		uint64_t temp;		
		read(sync_fd1, &temp, 8);
		
		double theta=0.0;
		int loc;
		for(loc=0; loc<BUFSIZE; loc++)
		{
			buf[loc] = sin(theta)*(INT16_MAX/4);
			theta+=440*0.000142476;
			if(theta>2.*M_PI) 
				theta=0.0;
		}
		
		uint64_t unblock=(unsigned char)1;
		
		//unblock fill_buffer() thread
		again:
			if(write(sync_fd2, &unblock, 8) < 8)
			{
				goto again;
				printf("again lol\n");
			}
		
		
		printf("		filled\n");
	}
}


void write_buffer_audio()
{
	while(1)
	{
		uint64_t temp;		
		read(sync_fd2, &temp, 8);
		
		
		uint64_t unblock=(unsigned char)1;
		
		//unblock fill_buffer() thread
		again:
			if(write(sync_fd1, &unblock, 8) < 8)
			{
				goto again;
				printf("again lol\n");
			}
			
		frames = snd_pcm_writei(handle, buf, BUFSIZE);
		if (frames < 0)
			frames = snd_pcm_recover(handle, frames, 0);
		if (frames < 0) 
		{
			printf("snd_pcm_writei failed: %s\n", snd_strerror(frames));
			break;
		}
		if (frames > 0 && frames < (long)BUFSIZE)
			printf("Short write (expected %li, wrote %li)\n", (long)BUFSIZE, frames);
		printf("written\n");
	}
}

void sched_realtime()
{
	struct sched_param sched;

	sched.sched_priority = 50;
	sched_setscheduler(0, SCHED_FIFO, &sched);
	sched_getparam(0, &sched);
}

int main(int argc, char *argv[])
{
	//sched_realtime();
	
		
	/*
	 * The eventfd(2) file descriptor for fill_buffer() to wait upon.
	 */
	sync_fd1 = eventfd(0, 0);
	sync_fd2 = eventfd(1, 0);
	
	if(sync_fd1 == -1 || sync_fd2 == -1)
	{
		fprintf(stderr, __FILE__ ": failed to open sync_fd\n");
		exit(0);
	}
	
	int err;


	/* 
	 * Create a new playback stream 
	 */
	if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0) < 0))
	{
		fprintf(stderr, __FILE__ "Playback open error: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}
	if ((err = snd_pcm_set_params(handle,
				SND_PCM_FORMAT_S16,
				SND_PCM_ACCESS_RW_INTERLEAVED,
				1,
				44100,
				1,
				500000)) < 0)
    {
		/* 0.5sec latency */
		printf("Playback open error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }
	
	
	pthread_t thread_fill, thread_write;
	
	pthread_create(&thread_fill, NULL, (void*) &fill_buffer, (void *)NULL);
	pthread_create(&thread_write, NULL, (void*) &write_buffer_audio, (void *)NULL);
	

	while(1){}

	return 0;
}