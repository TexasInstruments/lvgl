#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <linux/input.h>
#include <fcntl.h>

#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#include <linux/fb.h>
#include <sys/mman.h>
#include "lv_demo_high_res_private.h"


const char *ev_type[EV_CNT] = {
	[EV_SYN]       = "EV_SYN",
	[EV_KEY]       = "EV_KEY",
	[EV_REL]       = "EV_REL",
	[EV_ABS]       = "EV_ABS",
	[EV_MSC]       = "EV_MSC",
	[EV_SW]        = "EV_SW",
	[EV_LED]       = "EV_LED",
	[EV_SND]       = "EV_SND",
	[EV_REP]       = "EV_REP",
	[EV_FF]        = "EV_FF",
	[EV_PWR]       = "EV_PWR",
	[EV_FF_STATUS] = "EV_FF_STATUS",
	[EV_MAX]       = "EV_MAX",
};

const char *ev_code_syn[SYN_CNT] = {
	[SYN_REPORT]    = "SYN_REPORT",
	[SYN_CONFIG]    = "SYN_CONFIG",
	[SYN_MT_REPORT] = "SYN_MT_REPORT",
	[SYN_DROPPED]   = "SYN_DROPPED",
	[SYN_MAX]       = "SYN_MAX",
};

void *button_init(lv_demo_high_res_api_t * api)
{
        int fd;
        char dev[] = "/dev/input/eventX";
        struct input_event ie;

        char buffer[128];
        FILE *fp = popen("cat /proc/bus/input/devices | grep Phys=gpio-keys/input", "r");

        if (fp == NULL) {
            perror("popen failed");
            return 1;
        }

        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            int len_dev = strlen(dev);
            int len_buffer = strlen(buffer);
            dev[len_dev - 1] = buffer[len_buffer - 2];
        }
        else{
            printf("Button not Configured\n");
            return 0;
        }

        if ((fd = open(dev, O_RDONLY)) == -1) {
                perror("opening device");
                exit(EXIT_FAILURE);
        }

        int last_val=-1;
        int curr_val=-1;
        while (read(fd, &ie, sizeof(struct input_event))) {
                if(ie.type != 1)
                    continue;
                curr_val = ie.value;
                if(curr_val==0 && last_val > 0){
                    fprintf(stderr, "Button Released!!\n");
                    lv_lock();
                    int lock_status = lv_subject_get_int(&api->subjects.locked);
                    lv_subject_set_int(&api->subjects.locked, !lock_status);
                    lv_unlock();
                }
                last_val = curr_val;
        }

        close(fd);

        return EXIT_SUCCESS;
}
