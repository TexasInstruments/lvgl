#include <stdio.h>
#include <fcntl.h>
#include <pthread.h>

extern int delay_millis;
extern pthread_mutex_t delay_lock;

void *led_blink() {
    char on = '1';
    char off = '0';
    int fd = open("/sys/devices/platform/leds/leds/am62-sk\:green\:heartbeat/brightness", O_WRONLY);
    if (fd == -1) {
        perror("Error opening LED sysfs"); 
        return;
    }
    write (fd, &off, 1);
    int delay_local;
    while(1){
        pthread_mutex_lock(&delay_lock);
        delay_local = delay_millis;
        pthread_mutex_unlock(&delay_lock);
        write (fd, &on, 1);
        usleep(delay_local*1000);
        write (fd, &off, 1);
        usleep(delay_local*1000);
    }  
    close(fd);
}