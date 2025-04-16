// SPDX-License-Identifier: GPL-2.0-only
/* Industrialio buffer test code.
 *
 * Copyright (c) 2008 Jonathan Cameron
 *
 * This program is primarily intended as an example application.
 * Reads the current buffer setup from sysfs and starts a short capture
 * from the specified device.
 */

#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <linux/types.h>
#include <string.h>
#include <poll.h>
#include <endian.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <linux/iio/buffer.h>
#include <mosquitto.h>
#include <pthread.h>
#include "iio_utils.h"
#include "lv_demo_high_res_private.h"
#include "../../src/osal/lv_os.h"

static lv_demo_high_res_api_t * api_hmi;
static struct mosquitto *mosq;
extern int back_pressed_adc;
int flag_is_connected = 0;
void publish_sensor_data(float);

/* Callback called when the client receives a CONNACK message from the broker. */
void on_connect_temp_pub(struct mosquitto *mosq, void *obj, int reason_code)
{
    /* Print out the connection result. mosquitto_connack_string() produces an
        * appropriate string for MQTT v3.x clients, the equivalent for MQTT v5.0
        * clients is mosquitto_reason_string().
        */
    printf("on_connect: %s\n", mosquitto_connack_string(reason_code));
    if(reason_code != 0){
            /* If the connection fails for any reason, we don't want to keep on
                * retrying in this example, so disconnect. Without this, the client
                * will attempt to reconnect. */
            mosquitto_disconnect(mosq);
    }

    /* You may wish to set a flag here to indicate to your application that the
        * client is now connected. */
}

/* Callback called when the client knows to the best of its abilities that a
 * PUBLISH has been successfully sent. For QoS 0 this means the message has
 * been completely written to the operating system. For QoS 1 this means we
 * have received a PUBACK from the broker. For QoS 2 this means we have
 * received a PUBCOMP from the broker. */
void on_publish_temp_pub(struct mosquitto *mosq, void *obj, int mid)
{
    // printf("Message with mid %d has been published.\n", mid);
}

/* This function pretends to read some data from a sensor and publish it.*/
void publish_sensor_data(float temp)                            
{                                                                                     
    if(!flag_is_connected){
		return;
	}
	char payload[20];                                                   
    int rc;                 
                                                                                                                         
    /* Print it to a string for easy human reading - payload format is highly
        * application dependent. */                                                  
    snprintf(payload, sizeof(payload), "%1f", temp);    
                                                                        
    /* Publish the message                  
	* mosq - our client instance                                                 
	* *mid = NULL - we don't want to know what the message id for this message is
	* topic = "example/temperature" - the topic on which this message will be published
	* payloadlen = strlen(payload) - the length of our payload in bytes
	* payload - the actual payload                                    
	* qos = 1 - publish with QoS 1 for this example
	* retain = false - do not use the retained message feature for this message  
	*/   
    rc = mosquitto_publish(mosq, NULL, "SmartHome/temp", strlen(payload), payload, 1, false);
    if(rc != MOSQ_ERR_SUCCESS){                                        
		fprintf(stderr, "Error publishing: %s\n", mosquitto_strerror(rc));
    }
}

void publish_evCharge_data(int ev_charge)                            
{                                                                                     
    if(!flag_is_connected){
		return;
	}
	char payload[20];                                                   
    int rc;                 
                                                                                                                         
    /* Print it to a string for easy human reading - payload format is highly
        * application dependent. */                                                  
    snprintf(payload, sizeof(payload), "%d", ev_charge);    
                                                                        
    /* Publish the message                  
	* mosq - our client instance                                                 
	* *mid = NULL - we don't want to know what the message id for this message is
	* topic = "example/temperature" - the topic on which this message will be published
	* payloadlen = strlen(payload) - the length of our payload in bytes
	* payload - the actual payload                                    
	* qos = 1 - publish with QoS 1 for this example
	* retain = false - do not use the retained message feature for this message  
	*/  
	rc = mosquitto_publish(mosq, NULL, "SmartHome/evCharge", strlen(payload), payload, 1, false);
    if(rc != MOSQ_ERR_SUCCESS){                                        
		fprintf(stderr, "Error publishing: %s\n", mosquitto_strerror(rc));
    }                                    
}

int mqtt_temp_pub_init(){
  
  int rc;

  /* Required before calling other mosquitto functions */
  mosquitto_lib_init();

  /* Create a new client instance.
   * id = NULL -> ask the broker to generate a client id for us
   * clean session = true -> the broker should remove old sessions when we connect
   * obj = NULL -> we aren't passing any of our private data for callbacks
   */
  mosq = mosquitto_new(NULL, true, NULL);
  if(mosq == NULL){
        fprintf(stderr, "Error: Out of memory.\n");
        return 1;
  }

  /* Configure callbacks. This should be done before connecting ideally. */
  mosquitto_connect_callback_set(mosq, on_connect_temp_pub);
  mosquitto_publish_callback_set(mosq, on_publish_temp_pub);

  /* Connect to test.mosquitto.org on port 1883, with a keepalive of 60 seconds.
   * This call makes the socket connection only, it does not complete the MQTT
   * CONNECT/CONNACK flow, you should use mosquitto_loop_start() or
   * mosquitto_loop_forever() for processing net traffic. */
  //rc = mosquitto_connect(mosq, "test.mosquitto.org", 1883, 60);

  int err = mosquitto_tls_set(mosq, "/usr/share/ti-lvgl-demo/cert/AmazonRootCA1.pem", NULL, NULL, NULL, NULL);
  if (err != MOSQ_ERR_SUCCESS){
		printf("TLS not set\n");
  }

  rc = mosquitto_connect(mosq, "broker.hivemq.com", 8883, 60);
  if(rc != MOSQ_ERR_SUCCESS){
		flag_is_connected = 0;
        mosquitto_destroy(mosq);
        fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
        return 1;
  }
  else{
	flag_is_connected = 1;
  }

  /* Run the network loop in a background thread, this call returns quickly. */
  rc = mosquitto_loop_start(mosq);
  if(rc != MOSQ_ERR_SUCCESS){
		flag_is_connected = 0;
        mosquitto_destroy(mosq);
        fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
        return 1;
  }
  else{
	flag_is_connected = 1;
  }

  return 0;
}


int *adc_init(lv_demo_high_res_api_t * api)
{
	if (access("/sys/bus/iio/devices/iio\:device0/in_voltage0_raw", F_OK)) {
		printf("ADC does not exist\n");
		pthread_exit(0);
	}

	api_hmi = api;
    mqtt_temp_pub_init();
	char line[16];
	while(1) {
		FILE *fp = popen("cat /sys/bus/iio/devices/iio\:device0/in_voltage0_raw", "r");

		if (fp == NULL) {
			perror("popen failed");
			return NULL;
		}

	
		if (fgets(line, sizeof(line), fp) != NULL) {
			int adc_data = atoi(line);
        	lv_lock();
			lv_subject_set_int(&api_hmi->subjects.temperature_indoor, adc_data);
			lv_subject_set_int(&api_hmi->subjects.temperature_outdoor, (adc_data>65)?(adc_data-65):0);
			lv_unlock();
			publish_sensor_data(adc_data/10.0);
		}
        pclose(fp);
        usleep(500000);
	}
    mosquitto_lib_cleanup();

	return NULL;
}
