/*
 * Simple sound playback using ALSA API and libasound.
 *
 * Compile:
 * $ cc -o play sound_playback.c -lasound
 * 
 * Usage:
 * $ ./play <sample_rate> <channels> <seconds> < <file>
 * 
 * Examples:
 * $ ./play 44100 2 5 < /dev/urandom
 * $ ./play 22050 1 8 < /path/to/file.wav
 *
 * Copyright (C) 2009 Alessandro Ghedini <alessandro@ghedini.me>
 * --------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Alessandro Ghedini wrote this file. As long as you retain this
 * notice you can do whatever you want with this stuff. If we
 * meet some day, and you think this stuff is worth it, you can
 * buy me a beer in return.
 * --------------------------------------------------------------
 */

#include <alsa/asoundlib.h>
#include <stdio.h>
#include <pthread.h>

#define PCM_DEVICE "default"
extern pthread_mutex_t playing_now_lock;
extern int playing_now;

void *audio_play() {
        unsigned int pcm, tmp, dir;
        int rate, channels, seconds;
        snd_pcm_t *pcm_handle;
        snd_pcm_hw_params_t *params;
        snd_pcm_uframes_t frames;
        char *buff;
        int buff_size, loops;

        rate     = 44100;
        channels = 2;
        seconds  = 30;

        /* Open the PCM device in playback mode */
        if (pcm = snd_pcm_open(&pcm_handle, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0) < 0){ 
                printf("ERROR: Can't open \"%s\" PCM device. %s\n", PCM_DEVICE, snd_strerror(pcm));
                return;
        }

        /* Allocate parameters object and fill it with default values*/
        snd_pcm_hw_params_alloca(&params);

        snd_pcm_hw_params_any(pcm_handle, params);

        /* Set parameters */
        if (pcm = snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED) < 0){ 
                printf("ERROR: Can't set interleaved mode. %s\n", snd_strerror(pcm));
                return;
        }

        if (pcm = snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE) < 0){
                printf("ERROR: Can't set format. %s\n", snd_strerror(pcm));
                return;
        }

        if (pcm = snd_pcm_hw_params_set_channels(pcm_handle, params, channels) < 0){ 
                printf("ERROR: Can't set channels number. %s\n", snd_strerror(pcm));
                return;
        }

        if (pcm = snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, 0) < 0){ 
                printf("ERROR: Can't set rate. %s\n", snd_strerror(pcm));
                return;
        }

        /* Write parameters */
        if (pcm = snd_pcm_hw_params(pcm_handle, params) < 0){
                printf("ERROR: Can't set harware parameters. %s\n", snd_strerror(pcm));
                return;
        }

        /* Resume information */
        printf("PCM name: '%s'\n", snd_pcm_name(pcm_handle));

        printf("PCM state: %s\n", snd_pcm_state_name(snd_pcm_state(pcm_handle)));

        snd_pcm_hw_params_get_channels(params, &tmp);
        printf("channels: %i ", tmp);

        if (tmp == 1)
                printf("(mono)\n");
        else if (tmp == 2)
                printf("(stereo)\n");

        snd_pcm_hw_params_get_rate(params, &tmp, 0);
        printf("rate: %d bps\n", tmp);

        printf("seconds: %d\n", seconds);

        /* Allocate buffer to hold single period */
        snd_pcm_hw_params_get_period_size(params, &frames, 0);

        buff_size = frames * channels * 2 /* 2 -> sample size */;
        buff = (char *) malloc(buff_size);

        snd_pcm_hw_params_get_period_time(params, &tmp, NULL);

        while(1){
            int fd = open("/usr/share/ti-lvgl-demo/audio/mixkit-game-show-suspense-waiting-667.wav", O_RDONLY);
            if (fd == -1) {
                perror("Error opening audio file"); 
                return;
            }      
            for (loops = (seconds * 1000000) / tmp; loops > 0; loops--) {
                    while(1){
                        pthread_mutex_lock(&playing_now_lock);
                        if(playing_now){
                            pcm = snd_pcm_pause(pcm_handle, 0);
                            pthread_mutex_unlock(&playing_now_lock);
                            break;
                        }else{
                            pcm = snd_pcm_pause(pcm_handle, 1);
                        }
                        pthread_mutex_unlock(&playing_now_lock);
                    }

                    if (pcm = read(fd, buff, buff_size) == 0) {
                            printf("Early end of file.\n");
                            return 0;
                    }

                    if (pcm = snd_pcm_writei(pcm_handle, buff, frames) == -EPIPE) {
                            printf("XRUN.\n");
                            snd_pcm_prepare(pcm_handle);
                    } else if (pcm < 0) {
                            printf("ERROR. Can't write to PCM device. %s\n", snd_strerror(pcm));
                    }
            }
            close(fd);
        }
}