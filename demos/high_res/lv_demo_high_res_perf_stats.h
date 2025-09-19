/**
 * @file lv_demo_high_res.h
 *
 */

#ifndef LV_DEMO_HIGH_RES_PERF_STATS_H
#define LV_DEMO_HIGH_RES_PERF_STATS_H

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Read /proc/stats file, and load values of all fields into stat_values[] array.
 * @param stat_values[]         Values extracted from /proc/stats
 * @param len                   Length of stat_values[] array
 * @return                      0 if opening the file failed, 1 if everything went successfully
 */
uint8_t ti_read_proc_stats(uint32_t stat_values[], uint32_t len);

/**
 * Get CPU Utilization value in percentage. The function computes the busy time and the total time
 * of the CPU between current invocation and the previous invocation. It then finds the percentage
 * of busy time with respect to the total time and returns the result as utilization percent.
 * @return          CPU Utilization percentage
 */
uint32_t ti_get_cpu_load();

#endif /*LV_DEMO_HIGH_RES_PERF_STATS_H*/
