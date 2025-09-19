/**
 * @file lv_demo_high_res_perf_stats.c
 *
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "../../src/misc/lv_fs.h"

/* The number of fields in /proc/stat entries. Currently, as of kernel v6.12, it is 10. */
#define TI_NUM_PROC_STAT_FIELDS 10
/* The index (from 0) to field that shows idle stat. */
#define TI_PROC_STAT_IDLE_FIELD 3
/* The index (from 0) to field that shows IO-wait stat. */
#define TI_PROC_STAT_IOWAIT_FIELD 4

/* Variable to indicate that this is the first time a CPU load is being calculated. */
static uint8_t ti_is_first_time = 1;

/* Variable to hold cpu_util from last computation. This is useful when /proc/stat fails to open,
 * leaving the function with no value to return. In such cases, the previous utilization can be
 * returned.
 */
static uint32_t ti_cpu_utilization = 0;

/* Array to hold stats from previous function call. Initialized to 0 since there have been 0 calls
 * before the 1st one.
 */
uint32_t ti_glob_prev_stats[TI_NUM_PROC_STAT_FIELDS] = {0};

uint8_t ti_read_proc_stats(uint32_t stat_values[], uint32_t len)
{
    lv_fs_file_t f;
    lv_fs_res_t res;
    res = lv_fs_open(&f, "A:/proc/stat", LV_FS_MODE_RD);
    if (res != LV_FS_RES_OK) {
        printf("fs_open FAIL: res=%d\n", res);
        return 0;
    }

    uint8_t str[256];
    char delimiter[] = " ";

    lv_fs_read(&f, str, 256, NULL);
    res = lv_fs_close(&f);
    if (res != LV_FS_RES_OK) {
        printf("fs_close FAIL: res=%d\n", res);
    }

    /* Trim this string at \n */
    int j;
    for (j = 0; str[j] != '\n' && str[j] != '\0'; ++j)
        ;
    str[j] = '\0';
    
    char *token = strtok(str, delimiter);
    for (int i = 0; i < len; ++i)
        if ((token = strtok(NULL, delimiter)))
            stat_values[i] = atoi(token);
    return 1;
}

uint32_t ti_get_cpu_load()
{
    /**
     * Implementation:
     * ---------------
     *
     * /proc/stat aggregates information on the amount of time the CPU spent on different types of
     * activities since boot. The "types" here are "user processes", "niced processes" etc.
     * The format of output is:
     * 
     * cpu <user> <nice> ...
     *
     * The code below reads this, then finds the difference from the values obtained in the previous
     * read. It sums all the differences to determine the total_time taken, and finds busy_time by
     * subtracting the idle and io-wait time's from total_time.
     * It returns 100 - (percentage of busy_time), since LVGL expects idle %, instead of
     * utilization %.
     */

    if (ti_is_first_time) {
        /* if there was a problem in opening file, return previous utilization */
        if(!ti_read_proc_stats(ti_glob_prev_stats, TI_NUM_PROC_STAT_FIELDS))
            return ti_cpu_utilization;
        ti_is_first_time = 0;
    }

    uint32_t cur_stats[TI_NUM_PROC_STAT_FIELDS] = {0};
    ti_read_proc_stats(cur_stats, TI_NUM_PROC_STAT_FIELDS);

    uint32_t total_time = 0;

    uint32_t diff_stats[TI_NUM_PROC_STAT_FIELDS] = {0};
    for (int i = 0; i < TI_NUM_PROC_STAT_FIELDS; ++i) {
        diff_stats[i] = cur_stats[i] - ti_glob_prev_stats[i];
        total_time += diff_stats[i];
    }

    uint32_t busy_time = total_time - (diff_stats[TI_PROC_STAT_IDLE_FIELD] + diff_stats[TI_PROC_STAT_IOWAIT_FIELD]);

    for (int i = 0; i < TI_NUM_PROC_STAT_FIELDS; ++i)
        ti_glob_prev_stats[i] = cur_stats[i];

    ti_cpu_utilization = 100 - ((busy_time * 100) / total_time);

    return ti_cpu_utilization;
}

