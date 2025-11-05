/**
 * @file lv_demo_high_res_app_edgeai.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_demo_high_res_private.h"

#if LV_USE_DEMO_HIGH_RES && LV_BUILD_EXAMPLES && LV_USE_LABEL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "lvgl/lvgl.h"
#include "../../examples/lv_examples.h"
#include "../../src/widgets/image/lv_image.h"
#include "../../src/widgets/label/lv_label.h"

/*********************
 *      DEFINES
 *********************/
#define MAX_DEVICES 10

lv_obj_t *label;
lv_obj_t *btn;
pthread_t gst_thread;
volatile int running = 0;
const char* fifo_path = "/tmp/gst_output_fifo";
char *g_selected_device = NULL;
static bool g_no_device_ui = false;

typedef struct {
    char display_name[256];
    char alsa_device[64];
} audio_device_t;

audio_device_t audio_devices[MAX_DEVICES];
int device_count = 0;

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void back_clicked_cb(lv_event_t * e);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void update_label_text(const char *text) {
    lv_label_set_text(label, text);
}

// Get list of audio recording devices
char* get_arecord_devices() {
    FILE *fp;
    char path[1035];
    char *device_list = malloc(4096);
    
    if (!device_list) return NULL;
    device_list[0] = '\0';
    
    fp = popen("arecord -l", "r");
    if (fp == NULL) {
        printf("Failed to run command\n");
        free(device_list);
        return NULL;
    }
    
    char current_card_name[256] = {0};
    int card_num = -1;
    device_count = 0;
    
    while (fgets(path, sizeof(path), fp) != NULL) {
        if (strncmp(path, "card", 4) == 0) {
            char *card_str = strstr(path, "card ");
            if (card_str) {
                sscanf(card_str, "card %d:", &card_num);
            }
            
            char *name_start = strchr(path, '[');
            char *name_end = strchr(path, ']');
            if (name_start && name_end && name_end > name_start && device_count < MAX_DEVICES) {
                int name_len = name_end - name_start - 1;
                if (name_len > 0 && name_len < sizeof(current_card_name)) {
                    strncpy(current_card_name, name_start + 1, name_len);
                    current_card_name[name_len] = '\0';
                    
                    // Skip HDMI/playback-only devices
                    if (strstr(current_card_name, "HDMI") != NULL || strstr(current_card_name, "hdmi") != NULL || strstr(current_card_name, "cape") != NULL) {
                        printf("Skipping playback-only device: %s (card %d)\n", current_card_name, card_num);
                        continue;
                    }
                    
                    strncpy(audio_devices[device_count].display_name, current_card_name, 
                            sizeof(audio_devices[device_count].display_name) - 1);
                    audio_devices[device_count].display_name[sizeof(audio_devices[device_count].display_name) - 1] = '\0';
                    
                    snprintf(audio_devices[device_count].alsa_device, 
                             sizeof(audio_devices[device_count].alsa_device),
                             "plughw:%d,0", card_num);
                    
                    if (strlen(device_list) + strlen(current_card_name) + 2 < 4095) {
                        if (device_count > 0) {
                            strcat(device_list, "\n");
                        }
                        strcat(device_list, current_card_name);
                    }
                    
                    printf("Found capture device: %s -> %s\n", 
                           current_card_name, audio_devices[device_count].alsa_device);
                    
                    device_count++;
                }
            }
        }
    }
    
    pclose(fp);
    return device_list;
}

void* gst_launch_thread(void *arg) {
    char buffer[128];
    
    unlink(fifo_path);
    mkfifo(fifo_path, 0666);
    
    char gst_command[1024];
    snprintf(gst_command, sizeof(gst_command),
             "gst-launch-1.0 alsasrc device=%s ! audioconvert ! audio/x-raw,format=S16LE,channels=1,rate=16000,layout=interleaved ! "
             "tensor_converter frames-per-tensor=3900 ! "
             "tensor_aggregator frames-in=3900 frames-out=15600 frames-flush=3900 frames-dim=1 ! "
             "tensor_transform mode=arithmetic option=typecast:float32,add:0.5,div:32767.5 ! "
             "tensor_transform mode=transpose option=1:0:2:3 ! "
             "queue leaky=2 max-size-buffers=10 ! "
             "tensor_filter framework=tensorflow2-lite model=/usr/share/oob-demo-assets/models/yamnet_audio_classification.tflite custom=Delegate:XNNPACK,NumThreads:2 ! "
             "tensor_decoder mode=image_labeling option1=/usr/share/oob-demo-assets/labels/yamnet_label_list.txt ! "
             "filesink buffer-mode=2 location=%s 1> /dev/null", g_selected_device, fifo_path);

    printf("Starting GStreamer with device: %s\n", g_selected_device);

    FILE *pipe = popen(gst_command, "r");

    if (!pipe) {
        perror("popen failed!");
        return NULL;
    }

    int fifo_fd = open(fifo_path, O_RDONLY);
    if (fifo_fd == -1) {
        perror("open fifo failed!");
        pclose(pipe);
        return NULL;
    }

    // Read from the named pipe
    while (running && fgets(buffer, sizeof(buffer), fdopen(fifo_fd, "r")) != NULL) {
        buffer[strcspn(buffer, "$")] = 0;
        update_label_text(buffer);
    }

    close(fifo_fd);
    pclose(pipe);
    unlink(fifo_path); // Remove the named pipe
    return NULL;
}

 // Define the callback function for the button click event
static void btn_click_event(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = lv_event_get_target(e);

    if (code == LV_EVENT_CLICKED) {
        if (!running) {
            char *devices = get_arecord_devices();
            if (devices) free(devices);

            if (g_no_device_ui && device_count > 0) {
                update_label_text("Device detected. Please go back and re-open this page to select it.");
                return;
            }

            if (device_count == 0) {
                update_label_text("No capture device found!");
                if (g_selected_device) {
                    free(g_selected_device);
                    g_selected_device = NULL;
                }
                return;
            }

            bool is_valid = false;
            if (g_selected_device) {
                for (int i = 0; i < device_count; i++) {
                    if (strcmp(g_selected_device, audio_devices[i].alsa_device) == 0) {
                        is_valid = true;
                        break;
                    }
                }
            }

            if (!is_valid) {
                if (g_selected_device) {
                    free(g_selected_device);
                }
                g_selected_device = strdup(audio_devices[0].alsa_device);
            }
            running = 1;
            lv_label_set_text(lv_obj_get_child(btn, 0), "Stop");
            update_label_text("Starting audio classification...");
            pthread_create(&gst_thread, NULL, gst_launch_thread, NULL);
        } else {
            running = 0;
            pthread_join(gst_thread, NULL);
            lv_label_set_text(lv_obj_get_child(btn, 0), "Play");
            update_label_text("Connect your microphone and press Play");
        }
    }
}

// Dropdown event handler
static void dropdown_event_handler(lv_event_t * e)
{
    lv_obj_t * dropdown = lv_event_get_target(e);
    uint16_t selected = lv_dropdown_get_selected(dropdown);
    
    if (selected < device_count) {
        if (g_selected_device) {
            free(g_selected_device);
        }
        
        g_selected_device = strdup(audio_devices[selected].alsa_device);
        
        printf("Selected device: %s (ALSA: %s)\n", 
               audio_devices[selected].display_name, 
               audio_devices[selected].alsa_device);
        
        char info_msg[512];
        snprintf(info_msg, sizeof(info_msg), 
                 "Selected: %s\nPress Play to start", 
                 audio_devices[selected].display_name);
        update_label_text(info_msg);
        
        if (running) {
            running = 0;
            pthread_join(gst_thread, NULL);
            running = 1;
            pthread_create(&gst_thread, NULL, gst_launch_thread, NULL);
        }
    }
}

void lv_demo_high_res_app_edgeai(lv_obj_t * base_obj)
{
    lv_demo_high_res_ctx_t * c = lv_obj_get_user_data(base_obj);

    /* background */

    lv_obj_t * bg = base_obj;
    lv_obj_remove_style_all(bg);
    lv_obj_set_size(bg, LV_PCT(100), LV_PCT(100));

    lv_obj_t * bg_img = lv_image_create(bg);
    lv_subject_add_observer_obj(&c->th, lv_demo_high_res_theme_observer_image_src_cb, bg_img,
                                (void *)&c->imgs[IMG_LIGHT_BG_ABOUT]);

    lv_obj_t * bg_cont = lv_obj_create(bg);
    lv_obj_remove_style_all(bg_cont);
    lv_obj_set_size(bg_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_top(bg_cont, c->sz->gap[7], 0);
    lv_obj_set_style_pad_bottom(bg_cont, c->sz->gap[10], 0);
    lv_obj_set_style_pad_hor(bg_cont, c->sz->gap[10], 0);

    /* top margin */

    lv_obj_t * top_margin = lv_demo_high_res_top_margin_create(bg_cont, 0, true, c);

    /* app info */

    lv_obj_t * app_info = lv_demo_high_res_simple_container_create(bg_cont, true, c->sz->gap[4], LV_FLEX_ALIGN_START);
    lv_obj_align_to(app_info, top_margin, LV_ALIGN_OUT_BOTTOM_LEFT, 0, c->sz->gap[7]);

    lv_obj_t * back = lv_demo_high_res_simple_container_create(app_info, false, c->sz->gap[2], LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(back, back_clicked_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * back_icon = lv_image_create(back);
    lv_image_set_src(back_icon, c->imgs[IMG_ARROW_LEFT]);
    lv_obj_add_style(back_icon, &c->styles[STYLE_COLOR_BASE][STYLE_TYPE_A8_IMG], 0);
    lv_obj_add_flag(back_icon, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_t * back_label = lv_label_create(back);
    lv_label_set_text_static(back_label, "Back");
    lv_obj_set_style_text_opa(back_label, LV_OPA_60, 0);
    lv_obj_add_style(back_label, &c->styles[STYLE_COLOR_BASE][STYLE_TYPE_TEXT], 0);
    lv_obj_add_style(back_label, &c->fonts[FONT_HEADING_MD], 0);
    lv_obj_add_flag(back_label, LV_OBJ_FLAG_EVENT_BUBBLE);  // NEW: Added event bubble

    lv_obj_t * app_label = lv_label_create(app_info);
    lv_label_set_text_static(app_label, "Audio Classification");
    lv_obj_add_style(app_label, &c->styles[STYLE_COLOR_BASE][STYLE_TYPE_TEXT], 0);
    lv_obj_add_style(app_label, &c->fonts[FONT_HEADING_LG], 0);

    // Create dropdown label
    lv_obj_t * dropdown_label = lv_label_create(bg_cont);
    lv_label_set_text(dropdown_label, "Select Audio Input Device:");
    lv_obj_add_style(dropdown_label, &c->styles[STYLE_COLOR_BASE][STYLE_TYPE_TEXT], 0);
    lv_obj_add_style(dropdown_label, &c->fonts[FONT_HEADING_MD], 0);
    lv_obj_set_style_text_opa(dropdown_label, LV_OPA_80, 0);
    lv_obj_align(dropdown_label, LV_ALIGN_CENTER, 0, -130);

    // Create dropdown menu
    char *devices = get_arecord_devices();
    if (devices && device_count > 0) {
        g_no_device_ui = false;
        lv_obj_t * dropdown = lv_dropdown_create(bg_cont);
        lv_dropdown_set_options(dropdown, devices);
        
        lv_obj_set_width(dropdown, 450);
        lv_obj_set_height(dropdown, 45);
        lv_obj_align(dropdown, LV_ALIGN_CENTER, 0, -85);
        
        lv_obj_set_style_bg_color(dropdown, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dropdown, LV_OPA_90, LV_PART_MAIN);
        lv_obj_set_style_border_color(dropdown, lv_color_hex(0x00A2E8), LV_PART_MAIN);
        lv_obj_set_style_border_width(dropdown, 2, LV_PART_MAIN);
        lv_obj_set_style_border_opa(dropdown, LV_OPA_50, LV_PART_MAIN);
        lv_obj_set_style_radius(dropdown, 8, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(dropdown, 10, LV_PART_MAIN);
        lv_obj_set_style_shadow_color(dropdown, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(dropdown, LV_OPA_20, LV_PART_MAIN);
        lv_obj_set_style_pad_left(dropdown, 15, LV_PART_MAIN);
        lv_obj_set_style_pad_right(dropdown, 15, LV_PART_MAIN);
        lv_obj_set_style_pad_top(dropdown, 10, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(dropdown, 10, LV_PART_MAIN);
        lv_obj_set_style_text_color(dropdown, lv_color_hex(0x333333), LV_PART_MAIN);
        lv_obj_set_style_text_font(dropdown, &lv_font_montserrat_14, LV_PART_MAIN);
        
        lv_obj_set_style_bg_color(dropdown, lv_color_hex(0xFFFFFF), LV_PART_SELECTED);
        lv_obj_set_style_bg_opa(dropdown, LV_OPA_COVER, LV_PART_SELECTED);
        lv_obj_set_style_text_color(dropdown, lv_color_hex(0x00A2E8), LV_PART_SELECTED);
        
        lv_obj_t * list = lv_dropdown_get_list(dropdown);
        if (list) {
            lv_obj_set_style_bg_color(list, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_bg_opa(list, LV_OPA_90, 0);
            lv_obj_set_style_border_color(list, lv_color_hex(0x00A2E8), 0);
            lv_obj_set_style_border_width(list, 2, 0);
            lv_obj_set_style_radius(list, 8, 0);
            lv_obj_set_style_shadow_width(list, 15, 0);
            lv_obj_set_style_shadow_color(list, lv_color_hex(0x000000), 0);
            lv_obj_set_style_shadow_opa(list, LV_OPA_30, 0);
            lv_obj_set_style_pad_top(list, 8, 0);
            lv_obj_set_style_pad_bottom(list, 8, 0);
            lv_obj_set_style_pad_left(list, 5, 0);
            lv_obj_set_style_pad_right(list, 5, 0);
            lv_obj_set_style_max_height(list, 200, 0);
            lv_obj_set_style_text_font(list, &lv_font_montserrat_14, 0);
        }
        
        lv_dropdown_set_dir(dropdown, LV_DIR_BOTTOM);
        lv_dropdown_set_symbol(dropdown, LV_SYMBOL_DOWN);
        lv_obj_add_event_cb(dropdown, dropdown_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
        
        g_selected_device = strdup(audio_devices[0].alsa_device);
        printf("Default device: %s (ALSA: %s)\n", 
               audio_devices[0].display_name, 
               audio_devices[0].alsa_device);
        
        free(devices);
    } else {
        g_no_device_ui = true;
        lv_obj_t * error_label = lv_label_create(bg_cont);
        lv_label_set_text(error_label, "No capture devices found !!");
        lv_obj_add_style(error_label, &c->styles[STYLE_COLOR_BASE][STYLE_TYPE_TEXT], 0);
        lv_obj_add_style(error_label, &c->fonts[FONT_HEADING_MD], 0);
        lv_obj_set_style_text_color(error_label, lv_color_hex(0xFF6B6B), 0);
        lv_obj_align(error_label, LV_ALIGN_CENTER, 0, -80);
        lv_obj_set_style_text_align(error_label, LV_TEXT_ALIGN_CENTER, 0);
        
        if (devices) free(devices);
    }

    /* Create a label */
    label = lv_label_create(bg_cont);
    lv_label_set_text(label, "Connect your microphone and press Play");
    lv_obj_set_style_text_opa(label, LV_OPA_70, 0);  // MODIFIED: Changed from 60 to 70
    lv_obj_add_style(label, &c->styles[STYLE_COLOR_BASE][STYLE_TYPE_TEXT], 0);
    lv_obj_add_style(label, &c->fonts[FONT_HEADING_MD], 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 10);  // MODIFIED: Changed from -50 to 10
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);  // NEW: Added wrap
    lv_obj_set_width(label, 500);  // NEW: Set width
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);  // NEW: Center align

    /* Create a button */
    btn = lv_btn_create(bg_cont);
    lv_obj_set_size(btn, 140, 55);  // MODIFIED: Changed from 120x50 to 140x55
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 100);
    lv_obj_add_event_cb(btn, btn_click_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Play");
    lv_obj_center(btn_label);  // NEW: Center the label
    lv_obj_add_style(btn_label, &c->fonts[FONT_HEADING_MD], 0);  // NEW: Add font style
    lv_obj_set_style_text_color(btn_label, lv_color_hex(0xFFFFFF), 0);  // NEW: White text

    static lv_style_t style_btn;
    lv_style_init(&style_btn);
    lv_style_set_radius(&style_btn, 50);
    lv_style_set_bg_color(&style_btn, lv_color_hex(0x00A2E8));
    lv_style_set_bg_grad_color(&style_btn, lv_color_hex(0x005F99));
    lv_style_set_bg_grad_dir(&style_btn, LV_GRAD_DIR_VER);
    lv_style_set_border_color(&style_btn, lv_color_hex(0xFFFFFF));
    lv_style_set_border_width(&style_btn, 2);
    lv_style_set_border_opa(&style_btn, LV_OPA_30);  // NEW: Added border opacity
    lv_style_set_shadow_width(&style_btn, 15);  // MODIFIED: Changed from 10 to 15
    lv_style_set_shadow_color(&style_btn, lv_color_hex(0x000000));
    lv_style_set_shadow_opa(&style_btn, LV_OPA_30);  // NEW: Added shadow opacity
    lv_style_set_shadow_ofs_x(&style_btn, 0);  // MODIFIED: Changed from 5 to 0
    lv_style_set_shadow_ofs_y(&style_btn, 5);
    lv_obj_add_style(btn, &style_btn, 0);
    
    // NEW: Button pressed state
    static lv_style_t style_btn_pressed;
    lv_style_init(&style_btn_pressed);
    lv_style_set_bg_color(&style_btn_pressed, lv_color_hex(0x005F99));
    lv_style_set_shadow_ofs_y(&style_btn_pressed, 2);
    lv_obj_add_style(btn, &style_btn_pressed, LV_STATE_PRESSED);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void back_clicked_cb(lv_event_t * e)
{
    // NEW: Stop recording if running
    if (running) {
        running = 0;
        pthread_join(gst_thread, NULL);
    }

    // NEW: Free selected device
    if (g_selected_device) {
        free(g_selected_device);
        g_selected_device = NULL;
    }

    lv_obj_t * back = lv_event_get_target_obj(e);
    lv_obj_t * base_obj = lv_obj_get_parent(lv_obj_get_parent(lv_obj_get_parent(back)));
    lv_obj_clean(base_obj);
    lv_demo_high_res_home(base_obj);
}

#endif
