#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include "lv_demo_high_res_private.h"


static void slice(const char* str, char* result, size_t start, size_t end) {
    strncpy(result, str + start, end - start);
}

void *clock_init(lv_demo_high_res_api_t * api) {
    int last_minute = -1;
    while(1){
        char buffer[128];
        FILE *fp = popen("date", "r");

        if (fp == NULL) {
            perror("popen failed");
            return 1;
        }

        if (fgets(buffer, sizeof(buffer), fp) == NULL) {
            continue;
        }
        
        char day[10];
        char sliced_day[5];
        slice(buffer, sliced_day, 0, 0 + 3);
        if(strcmp(sliced_day, "Mon")==0){
            strcpy(day, "Monday");
        }
        else if(strcmp(sliced_day, "Tue")==0){
            strcpy(day, "Tuesday");
        }
        else if(strcmp(sliced_day, "Wed")==0){
            strcpy(day, "Wednesday");
        }
        else if(strcmp(sliced_day, "Thu")==0){
            strcpy(day, "Thursday");
        }
        else if(strcmp(sliced_day, "Fri")==0){
            strcpy(day, "Friday");
        }
        else if(strcmp(sliced_day, "Sat")==0){
            strcpy(day, "Saturday");
        }
        else if(strcmp(sliced_day, "Sun")==0){
            strcpy(day, "Sunday");
        }
        
        char month[10];
        char sliced_month[5];
        slice(buffer, sliced_month, 4, 4 + 3);
        if(strcmp(sliced_month, "Jan")==0){
            strcpy(month, "January");
        }
        else if(strcmp(sliced_month, "Feb")==0){
            strcpy(month, "February");
        }
        else if(strcmp(sliced_month, "Mar")==0){
            strcpy(month, "March");
        }
        else if(strcmp(sliced_month, "Apr")==0){
            strcpy(month, "April");
        }
        else if(strcmp(sliced_month, "May")==0){
            strcpy(month, "May");
        }
        else if(strcmp(sliced_month, "Jun")==0){
            strcpy(month, "June");
        }
        else if(strcmp(sliced_month, "Jul")==0){
            strcpy(month, "July");
        }
        else if(strcmp(sliced_month, "Aug")==0){
            strcpy(month, "August");
        }
        else if(strcmp(sliced_month, "Sep")==0){
            strcpy(month, "September");
        }
        else if(strcmp(sliced_month, "Oct")==0){
            strcpy(month, "October");
        }
        else if(strcmp(sliced_month, "Nov")==0){
            strcpy(month, "November");
        }
        else if(strcmp(sliced_month, "Dec")==0){
            strcpy(month, "December");
        }
        
        char sliced_date[4];
        slice(buffer, sliced_date, 8, 8 + 2);
        int date = atoi(sliced_date);
        
        char sliced_hour[4];
        slice(buffer, sliced_hour, 11, 11 + 2);
        int hour = atoi(sliced_hour);
        
        char sliced_minute[4];
        slice(buffer, sliced_minute, 14, 14 + 2);
        int minute = atoi(sliced_minute);
        if(last_minute==-1){
            lv_lock();
            lv_subject_set_pointer(&api->subjects.month_name, month);
            lv_subject_set_int(&api->subjects.month_day, date);
            lv_subject_set_pointer(&api->subjects.week_day_name, day);
            lv_subject_set_int(&api->subjects.hour, hour);
            lv_subject_set_int(&api->subjects.minute, minute);
            lv_unlock();
            last_minute = minute;
        }

        char sliced_year[4];
        slice(buffer, sliced_year, 24, 24 + 4);
        int year = atoi(sliced_year);
        
        pclose(fp);

        if(last_minute != minute){
            lv_lock();
            lv_subject_set_pointer(&api->subjects.month_name, month);
            lv_subject_set_int(&api->subjects.month_day, date);
            lv_subject_set_pointer(&api->subjects.week_day_name, day);
            lv_subject_set_int(&api->subjects.hour, hour);
            lv_subject_set_int(&api->subjects.minute, minute);
            lv_unlock();
        }
        last_minute = minute;
        usleep(1000000);
    }
}