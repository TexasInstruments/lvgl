#include <mosquitto.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "lv_demo_high_res_private.h"

static struct mosquitto *mosq1;
int flag_is_PG_connected = 0;
char devices_pub[6][20] = {"Light_matter","Light_zigbee","Fan_matter","Fan_zigbee", "Airpurifier", "Door"};
int num_devices_pub = sizeof(devices_pub) / sizeof(*devices_pub);

extern char SERVER_IP_ADDR[20];

/* Callback called when the client receives a CONNACK message from the broker. */
void on_connect_pub_PG(struct mosquitto *mosq1, void *obj, int reason_code)
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
            mosquitto_disconnect(mosq1);
    }

    /* You may wish to set a flag here to indicate to your application that the
        * client is now connected. */
}

/* Callback called when the client knows to the best of its abilities that a
 * PUBLISH has been successfully sent. For QoS 0 this means the message has
 * been completely written to the operating system. For QoS 1 this means we
 * have received a PUBACK from the broker. For QoS 2 this means we have
 * received a PUBCOMP from the broker. */
void on_publish_pub_PG(struct mosquitto *mosq1, void *obj, int mid)
{
    // printf("Message with mid %d has been published.\n", mid);
}

/* This function pretends to read some data from a sensor and publish it.*/
void publish_PG_data(int device_id, int state)       //1:on, 0:off                       
{                                                                                     
    if(!flag_is_PG_connected){
      return;
    }
    char payload[40];                                                   
    int rc;                 
                                                                                                                         
    /* Print it to a string for easy human reading - payload format is highly
        * application dependent. */                                                  
    sprintf(payload, "{\"%s\": \"%s\", \"source\": \"hmi\"}", devices_pub[device_id], state?"on":"off");  
    /* Publish the message                  
	* mosq - our client instance                                                 
	* *mid = NULL - we don't want to know what the message id for this message is
	* topic = "example/temperature" - the topic on which this message will be published
	* payloadlen = strlen(payload) - the length of our payload in bytes
	* payload - the actual payload                                    
	* qos = 1 - publish with QoS 1 for this example
	* retain = false - do not use the retained message feature for this message  
	*/  
    rc = mosquitto_publish(mosq1, NULL, devices_pub[device_id], strlen(payload), payload, 0, false);
    if(rc != MOSQ_ERR_SUCCESS){                                        
		  fprintf(stderr, "Error publishing: %s\n", mosquitto_strerror(rc));
    }      
}

void *mqtt_pub_PG_init(lv_demo_high_res_api_t * api){
  int rc;

  /* Required before calling other mosquitto functions */
  mosquitto_lib_init();

  /* Create a new client instance.
   * id = NULL -> ask the broker to generate a client id for us
   * clean session = true -> the broker should remove old sessions when we connect
   * obj = NULL -> we aren't passing any of our private data for callbacks
   */
  mosq1 = mosquitto_new(NULL, true, NULL);
  if(mosq1 == NULL){
        fprintf(stderr, "Error: Out of memory.\n");
        return;
  }

  /* Configure callbacks. This should be done before connecting ideally. */
  mosquitto_connect_callback_set(mosq1, on_connect_pub_PG);
  mosquitto_publish_callback_set(mosq1, on_publish_pub_PG);

  /* Connect to test.mosquitto.org on port 1883, with a keepalive of 60 seconds.
   * This call makes the socket connection only, it does not complete the MQTT
   * CONNECT/CONNACK flow, you should use mosquitto_loop_start() or
   * mosquitto_loop_forever() for processing net traffic. */

  while(SERVER_IP_ADDR[0]=='\0'){
        usleep(500000);
  }

  rc = mosquitto_connect(mosq1, SERVER_IP_ADDR, 1883, 60);
  if(rc != MOSQ_ERR_SUCCESS){
        flag_is_PG_connected = 0;
        mosquitto_destroy(mosq1);
        fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
        return;
  }
  else{
        flag_is_PG_connected = 1;
  }

  /* Run the network loop in a background thread, this call returns quickly. */
  rc = mosquitto_loop_start(mosq1);
  if(rc != MOSQ_ERR_SUCCESS){
        flag_is_PG_connected = 0;
        mosquitto_destroy(mosq1);
        fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
        return;
  }
  else{
        flag_is_PG_connected = 1;
  }

  return;
}