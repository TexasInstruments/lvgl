/*
 * This example shows how to write a client that subscribes to a topic and does
 * not do anything other than handle the messages that are received.
 */

#include <mosquitto.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "lv_demo_high_res_private.h"

static lv_demo_high_res_api_t * api_hmi;
char devices[6][20] = {"Light_matter","Light_zigbee","Fan_matter","Fan_zigbee", "Airpurifier", "Door"};
int num_devices = sizeof(devices) / sizeof(*devices);
char SERVER_IP_ADDR[20]={'\0'};

/* Callback called when the client receives a CONNACK message from the broker. */
void on_connect_sub_PG(struct mosquitto *mosq, void *obj, int reason_code)
{
  int rc;
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

  /* Making subscriptions in the on_connect() callback means that if the
   * connection drops and is automatically resumed by the client, then the
   * subscriptions will be recreated when the client reconnects. */
  for (int i=0; i < num_devices; i++){
    rc = mosquitto_subscribe(mosq, NULL, devices[i], 1);
    if(rc != MOSQ_ERR_SUCCESS){
        fprintf(stderr, "Error subscribing: %s\n", mosquitto_strerror(rc));
        /* We might as well disconnect if we were unable to subscribe */
        mosquitto_disconnect(mosq);
    }
  }
}


/* Callback called when the broker sends a SUBACK in response to a SUBSCRIBE. */
void on_subscribe_sub_PG(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos)
{
  int i;
  bool have_subscription = false;

  /* In this example we only subscribe to a single topic at once, but a
   * SUBSCRIBE can contain many topics at once, so this is one way to check
   * them all. */
  for(i=0; i<qos_count; i++){
        printf("on_subscribe: %d:granted qos = %d\n", i, granted_qos[i]);
        if(granted_qos[i] <= 2){
                have_subscription = true;
        }
  }
  if(have_subscription == false){
        /* The broker rejected all of our subscriptions, we know we only sent
         * the one SUBSCRIBE, so there is no point remaining connected. */
        fprintf(stderr, "Error: All subscriptions rejected.\n");
        mosquitto_disconnect(mosq);
  }
}


/* Callback called when the client receives a message. */
void on_message_sub_PG(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg)
{
  char rcvd_msg[30];
  strcpy(rcvd_msg, (char *)msg->payload);
  if(strstr(rcvd_msg, "hmi")){
    //Ignore if Source of message is HMI
    return;
  }
  if(strcmp((char*)msg->topic, "Light_matter")==0){
    if(strstr(rcvd_msg, "on")){       //Light on
      lv_lock();
      lv_subject_set_int(&api_hmi->subjects.lightbulb_matter, 1);
      lv_unlock();
    }
    else if(strstr(rcvd_msg, "off")){ //Light off
      lv_lock();
      lv_subject_set_int(&api_hmi->subjects.lightbulb_matter, 0);
      lv_unlock();
    }
    else{
      printf("Invalid Message\n");
    } 
  }
  else if(strcmp((char*)msg->topic, "Light_zigbee")==0){
    if(strstr(rcvd_msg, "on")){       //Light on
      lv_lock();
      lv_subject_set_int(&api_hmi->subjects.lightbulb_zigbee, 1);
      lv_unlock();
    }
    else if(strstr(rcvd_msg, "off")){ //Light off
      lv_lock();
      lv_subject_set_int(&api_hmi->subjects.lightbulb_zigbee, 0);
      lv_unlock();
    }
    else{
      printf("Invalid Message\n");
    } 
  }
  else if(strcmp((char*)msg->topic, "Fan_matter")==0){
    if(strstr(rcvd_msg, "on")){       //Fan on
      lv_lock();
      lv_subject_set_int(&api_hmi->subjects.fan_matter, 1);
      lv_unlock();
    }
    else if(strstr(rcvd_msg, "off")){ //Fan off
      lv_lock();
      lv_subject_set_int(&api_hmi->subjects.fan_matter, 0);
      lv_unlock();
    }
    else{
      printf("Invalid Message\n");
    } 
  }
  else if(strcmp((char*)msg->topic, "Fan_zigbee")==0){
    if(strstr(rcvd_msg, "on")){       //Fan on
      lv_lock();
      lv_subject_set_int(&api_hmi->subjects.fan_zigbee, 1);
      lv_unlock();
    }
    else if(strstr(rcvd_msg, "off")){ //Fan off
      lv_lock();
      lv_subject_set_int(&api_hmi->subjects.fan_zigbee, 0);
      lv_unlock();
    }
    else{
      printf("Invalid Message\n");
    } 
  }
  else if(strcmp((char*)msg->topic, "Airpurifier")==0){
    if(strstr(rcvd_msg, "on")){       //Airpurifier on
      lv_lock();
      lv_subject_set_int(&api_hmi->subjects.air_purifier, 1);
      lv_unlock();
    }
    else if(strstr(rcvd_msg, "off")){ //Airpurifier off
      lv_lock();
      lv_subject_set_int(&api_hmi->subjects.air_purifier, 0);
      lv_unlock();
    }
    else{
      printf("Invalid Message\n");
    } 
  }
  else if(strcmp((char*)msg->topic, "Door")==0){
    if(strstr(rcvd_msg, "on")){       //Door Open
      lv_lock();
      lv_subject_set_int(&api_hmi->subjects.door, 1);
      lv_unlock();
    }
    else if(strstr(rcvd_msg, "off")){ //Door closed
      lv_lock();
      lv_subject_set_int(&api_hmi->subjects.door, 0);
      lv_unlock();
    }
    else{
      printf("Invalid Message\n");
    } 
  }
  
}


void *mqtt_sub_PG_init(lv_demo_high_res_api_t * api){
  struct mosquitto *mosq;
  int rc;
  api_hmi = api;

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
        return;
  }

  /* Configure callbacks. This should be done before connecting ideally. */
  mosquitto_connect_callback_set(mosq, on_connect_sub_PG);
  mosquitto_subscribe_callback_set(mosq, on_subscribe_sub_PG);
  mosquitto_message_callback_set(mosq, on_message_sub_PG);

  printf("Enter server IP addr:\n");
  scanf("%[^\n]%*c", SERVER_IP_ADDR);

  /* Connect to test.mosquitto.org on port 1883, with a keepalive of 60 seconds.
   * This call makes the socket connection only, it does not complete the MQTT
   * CONNECT/CONNACK flow, you should use mosquitto_loop_start() or
   * mosquitto_loop_forever() for processing net traffic. */
  rc = mosquitto_connect(mosq, SERVER_IP_ADDR, 1883, 60);
  printf("rc=%d\t MOSQ_ERR_SUCCESS=%d\n", rc, MOSQ_ERR_SUCCESS);
  if(rc != MOSQ_ERR_SUCCESS){
        mosquitto_destroy(mosq);
        fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
        return;
  }

  /* Run the network loop in a blocking call. The only thing we do in this
   * example is to print incoming messages, so a blocking call here is fine.
   *
   * This call will continue forever, carrying automatic reconnections if
   * necessary, until the user calls mosquitto_disconnect().
   */
  mosquitto_loop_forever(mosq, -1, 1);

  mosquitto_lib_cleanup();
  return;
}
