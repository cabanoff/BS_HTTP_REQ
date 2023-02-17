
/**
 * @file
 * A simple program that subscribes to a topic.
 */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <curl/curl.h>

#include <mqtt.h>
#include "templates/posix_sockets.h"


/**
 * @brief The function will be called whenever a PUBLISH message is received.
 */
void publish_callback(void** unused, struct mqtt_response_publish *published);

/**
 * @brief The client's refresher. This function triggers back-end routines to
 *        handle ingress/egress traffic to the broker.
 *
 * @note All this function needs to do is call \ref __mqtt_recv and
 *       \ref __mqtt_send every so often. I've picked 100 ms meaning that
 *       client ingress/egress traffic will be handled every 100 ms.
 */
void* client_refresher(void* client);

/**
 * @brief Safelty closes the \p sockfd and cancels the \p client_daemon before \c exit.
 */
void exit_example(int status, int sockfd, pthread_t *client_daemon);
/**
 * @brief fields of data from cow sensors
 * 
 */

struct sensor_fields{
    uint32_t deviceID;
    uint32_t messageID;
    uint32_t activity1;
    uint32_t activity2;
    uint32_t activity3;
    uint32_t activity4;
    uint32_t rumination1;
    uint32_t rumination2;
    uint32_t rumination3;
    uint32_t chewing1;
    uint32_t chewing2;
    uint32_t chewing3;
    uint32_t rest1;
    uint32_t rest2;
    uint32_t rest3;
    uint32_t position1;
    __time_t time;
    uint32_t data_ready;
};
/**
 * @brief parse data from sensor to json format
 * 
 * @note message must be
 *  01000000 970F0000 00 01 9F000000 00 00 00000001000000000000000000000000
 *  first 8 chars of message is device ID
 *  there are 64 chars in the message
 *  each char from 0 to F 
 */
int fill_sensor_field(struct sensor_fields* sensor, const char* message);

#define TOP_INDEX 500

CURL *curl;
CURLcode res;
struct sensor_fields sensor = {.data_ready = 0};
FILE* log_file = NULL;
struct tm * x;
struct timespec fetch_time;
pthread_mutex_t lock;
struct sensor_fields sensor_array[TOP_INDEX];
volatile int sensor_index = 0;

int main(int argc, const char *argv[])
{
    
    const char* addr = "localhost";
    const char* port = "1883";
    const char* topic = "mqtt-kontron/lora-gatway";
    const char* url_post;

    char json_arr[250];
    char fetch_timestamp[30];
    


     /* get address (argv[1] if present) */
    if (argc > 1) {
        url_post = "http://tfka.online/json_ins_1.php";
    } else {
        url_post = "http://tfka.online/json_ins.php";
    }

    /* open the non-blocking TCP socket (connecting to the broker) */
    int sockfd = open_nb_socket(addr, port);

    if (sockfd == -1) {
        perror("Failed to open socket: ");
        exit_example(EXIT_FAILURE, sockfd, NULL);
    }

    log_file = fopen("log_file.log", "a");  /* create log file, append if file already exist */
	if (log_file == NULL) {
		printf("ERROR: impossible to create log file \n");
		exit_example(EXIT_FAILURE, sockfd, NULL);
	}


    /* setup a client */
    struct mqtt_client client;
    uint8_t sendbuf[2048]; /* sendbuf should be large enough to hold multiple whole mqtt messages */
    uint8_t recvbuf[1024]; /* recvbuf should be large enough any whole mqtt message expected to be received */
    mqtt_init(&client, sockfd, sendbuf, sizeof(sendbuf), recvbuf, sizeof(recvbuf), publish_callback);
    /* Create an anonymous session */
    const char* client_id = NULL;
    /* Ensure we have a clean session */
    uint8_t connect_flags = MQTT_CONNECT_CLEAN_SESSION;
    /* Send connection request to the broker. */
    mqtt_connect(&client, client_id, NULL, NULL, 0, NULL, NULL, connect_flags, 400);

    /* check that we don't have any errors */
    if (client.error != MQTT_OK) {
        fprintf(stderr, "error: %s\n", mqtt_error_str(client.error));
        exit_example(EXIT_FAILURE, sockfd, NULL);
    }

    /* start a thread to refresh the client (handle egress and ingree client traffic) */
    pthread_t client_daemon;
    if(pthread_create(&client_daemon, NULL, client_refresher, &client)) {
        fprintf(stderr, "Failed to start client daemon.\n");
        exit_example(EXIT_FAILURE, sockfd, NULL);
    }

    if (pthread_mutex_init(&lock, NULL) != 0) {
        printf("\n mutex init failed\n");
        exit_example(EXIT_FAILURE, sockfd, NULL);
    }

    /* subscribe */
    mqtt_subscribe(&client, topic, 0);

    /* start publishing the time */
    printf("listening for '%s' messages. field length = %d \n", topic,(int)sizeof(__time_t));

    /* block */
    while(1) {

        /* In windows, this will init the winsock stuff */
        curl_global_init(CURL_GLOBAL_ALL);
          /* get a curl handle */
        curl = curl_easy_init();
        if(!curl) {
            perror("Failed to open curl: ");
            exit_example(EXIT_FAILURE, sockfd, NULL);
        }
        /* First set the URL that is about to receive our POST. This URL can
         just as well be a https:// URL if that is what should receive the
         data. */
        curl_easy_setopt(curl, CURLOPT_URL, url_post);

        printf("Set URL '%s' \n", url_post);

        while(1) {
            pthread_mutex_lock(&lock);
            if(sensor_index) {
                /* local timestamp generation until we get accurate GPS time */
                //clock_gettime(CLOCK_REALTIME, &fetch_time);
                if(sensor_index)sensor_index--;
                x = gmtime(&(sensor_array[sensor_index].time));

                sprintf(fetch_timestamp,"%04i-%02i-%02iT%02i;%02i;%02i.%03liZ",\
                (x->tm_year)+1900,(x->tm_mon)+1,x->tm_mday,x->tm_hour,x->tm_min,x->tm_sec,(fetch_time.tv_nsec)/1000000); /* ISO 8601 format */

                sprintf(json_arr,"json_data=_id:%u,deviceId:%u,timestamp:%s,\
                activity1:%u,activity2:%u,activity3:%u,activity4:%u,rumination:%u,\
                chewing:%u,rest:%u,rumination2:%u,position1:%u",
                sensor_array[sensor_index].messageID,sensor_array[sensor_index].deviceID,fetch_timestamp,
                sensor_array[sensor_index].activity1,sensor_array[sensor_index].activity2,sensor_array[sensor_index].activity3,
                sensor_array[sensor_index].activity4,sensor_array[sensor_index].rumination1,sensor_array[sensor_index].chewing1,
                sensor_array[sensor_index].rest1,sensor_array[sensor_index].rumination2,sensor_array[sensor_index].position1);
                
                

                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_arr);

                res = curl_easy_perform(curl);

                if(res != CURLE_OK) {
                    if(sensor_index < (TOP_INDEX-1))sensor_index++;  // data is not sent set index back
                    pthread_mutex_unlock(&lock);
                    fprintf(stderr, "curl_easy_perform() failed: %s\n",curl_easy_strerror(res));
                    break;
                }
               
               
                printf("index = %d Sent to URL: %s\n",sensor_index,json_arr);
            }
            pthread_mutex_unlock(&lock);
            
        }
        if(curl) curl_easy_cleanup(curl);
        curl_global_cleanup();
         /* disconnect */
        printf("\n cURL disconnecting from %s\n", url_post);
        printf("\n cURL  waits for 10 minutes to connect again\n");
        /* writing to log file the time of connection loss*/
		fprintf(log_file, "%s : connection lost.\n", fetch_timestamp);
        fflush(log_file);
        sleep(600);
    }

    /* disconnect */
    printf("\n disconnecting from %s\n", addr);
    sleep(1);

    /* exit */
    exit_example(EXIT_SUCCESS, sockfd, &client_daemon);
}

void exit_example(int status, int sockfd, pthread_t *client_daemon)
{
    if (sockfd != -1) close(sockfd);
    if (client_daemon != NULL) pthread_cancel(*client_daemon);
    if(curl) curl_easy_cleanup(curl);
    curl_global_cleanup();
    if(log_file != NULL)fclose(log_file);
    exit(status);
}



void publish_callback(void** unused, struct mqtt_response_publish *published)
{
    /* note that published->topic_name is NOT null-terminated (here we'll change it to a c-string) */
    char* topic_name = (char*) malloc(published->topic_name_size + 1);
    //char json_arr[300];
    //char fetch_timestamp[30];
    //struct tm * x;
    //struct timespec fetch_time;
    char* sensor_message;

    sensor_message = strchr((const char*) published->application_message,',');  //find ',' which separates time from data
    sensor_message++; //next simbol after comma

    //printf("Received topic('%s')\n", topic_name);
   //printf("DEBUG1\n");
    if(fill_sensor_field(&sensor, (const char*) sensor_message)) {

        /* save data in stack array*/
        pthread_mutex_lock(&lock);
        sensor.data_ready = 1;
        memcpy(&sensor_array[sensor_index],&sensor,sizeof(sensor));  // save data to stack array
        if(sensor_index < (TOP_INDEX-1))sensor_index++;
        pthread_mutex_unlock(&lock);
    }
    free(topic_name);
}

void* client_refresher(void* client)
{
    while(1)
    {
        mqtt_sync((struct mqtt_client*) client);
        usleep(100000U);
    }
    return NULL;
}

#define IS_DIGIT(x)  ((x>='0')&&(x<='9'))||((x>='a')&&(x<='f'))||((x>='A')&&(x<='F'))

int fill_sensor_field(struct sensor_fields* sensor, const char* message)
{
     for(int i = 0; i < 64; i++)if(!(IS_DIGIT(message[i])))return 0;  //message should contain only hex digits

    /*find sensor ID*/
    char IDstring[9]; //8 digits and \0
    IDstring[0] = message[6];
    IDstring[1] = message[7];
    IDstring[2] = message[4];
    IDstring[3] = message[5];
    IDstring[4] = message[2];
    IDstring[5] = message[3];
    IDstring[6] = message[0];
    IDstring[7] = message[1];
    IDstring[8] = 0;
    uint32_t DevNumber = (uint32_t)strtol(IDstring, NULL, 16);
    sensor->deviceID = DevNumber;
    /* find message number */
    IDstring[0] = message[14];
    IDstring[1] = message[15];
    IDstring[2] = message[12];
    IDstring[3] = message[13];
    IDstring[4] = message[10];
    IDstring[5] = message[11];
    IDstring[6] = message[8];
    IDstring[7] = message[9];
    IDstring[8] = 0;
    uint32_t MessNumber = (uint32_t)strtol(IDstring, NULL, 16);
    sensor->messageID = MessNumber;
    /* find condition 1 byte N */
    uint8_t rumination, chewing, rest;
    switch(message[19]){
        case '0':
        rest = 1;rumination = 0; chewing = 0;
        break;
        case '1':
        rest = 0;rumination = 0; chewing = 1;
        break;
        case '2':
        rest = 0;rumination = 1; chewing = 0;
        break;
        default:
        rest = 0; rumination = 0; chewing = 0;
    }
    sensor->chewing1 = chewing;
    sensor->rumination1 = rumination;
    sensor->rest1 = rest;
    switch(message[29]){
        case '0':
        rest = 1;rumination = 0; chewing = 0;
        break;
        case '1':
        rest = 0;rumination = 0; chewing = 1;
        break;
        case '2':
        rest = 0;rumination = 1; chewing = 0;
        break;
        default:
        rest = 0; rumination = 0; chewing = 0;
    }
    sensor->chewing2 = chewing;
    sensor->rumination2 = rumination;
    sensor->rest3 = rest;
    switch(message[31]){
        case '0':
        rest = 1;rumination = 0; chewing = 0;
        break;
        case '1':
        rest = 0;rumination = 0; chewing = 1;
        break;
        case '2':
        rest = 0;rumination = 1; chewing = 0;
        break;
        default:
        rest = 0; rumination = 0; chewing = 0;
    }
    sensor->chewing3 = chewing;
    sensor->rumination3 = rumination;
    sensor->rest3 = rest;
    if(message[33] == '0')sensor->position1 = 0;
    else sensor->position1 = 1;
    /*find activity 1*/
    char activity1Str[3];
    activity1Str[0] = message[20];
    activity1Str[1] = message[21];
    activity1Str[2] = 0;
    sensor->activity1 = (uint8_t)strtol(activity1Str, NULL, 16);
    /*find activity 2*/
    char activity2Str[3];
    activity2Str[0] = message[22];
    activity2Str[1] = message[23];
    activity2Str[2] = 0;
    sensor->activity2 = (uint8_t)strtol(activity2Str, NULL, 16);
    /*find activity 2*/
    char activity3Str[3];
    activity3Str[0] = message[24];
    activity3Str[1] = message[25];
    activity3Str[2] = 0;
    sensor->activity3 = (uint8_t)strtol(activity3Str, NULL, 16);
    /*find activity 4*/
    char activity4Str[3];
    activity4Str[0] = message[26];
    activity4Str[1] = message[27];
    activity4Str[2] = 0;
    sensor->activity4 = (uint8_t)strtol(activity4Str, NULL, 16);

    clock_gettime(CLOCK_REALTIME, &fetch_time);
    sensor->time = fetch_time.tv_sec;

    return 1;
}
