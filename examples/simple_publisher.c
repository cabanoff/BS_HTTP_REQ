
/**
 * @file
 * A simple program to that publishes the current time whenever ENTER is pressed.
 * install mosquitto
 * run mosquitto server
 * sudo service mosquitto restart
 * sudo systemctl restart mosquitto
 * Usage:  ./bin/simple_subscriber [address [port [topic]]]
 * 
 */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include <mqtt.h>
#include "templates/posix_sockets.h"


/**
 * @brief The function that would be called whenever a PUBLISH is received.
 *
 * @note This function is not used in this example.
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
 * A simple program to that publishes the current time whenever ENTER is pressed.
 */
//int main()
int main(int argc, const char *argv[]) 
{
    const char* addr = "localhost";
    const char* port = "1883";
    const char* topic = "mqtt-kontron/lora-gatway";
    const char* mqtt_mess = "3C0000002E0F000000020F020005030201000002000000000000000000000000";
    //const char* mqtt_mess = "56EF76AC2E9FD79109420FE89D7583C2C1FFFFFFFFFFFFEDDFFDDDDDCCCAAA22";

    /* get the current time */
    time_t timer;
    struct tm* tm_info = localtime(&timer);
    char timebuf[26];
    char application_message[256];

       
    /* open the non-blocking TCP socket (connecting to the broker) */
    int sockfd = open_nb_socket(addr, port);

    if (sockfd == -1) {
        perror("Failed to open socket: ");
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

    /* start publishing the time */
    printf("Press ENTER to publish the current time.\n");
    printf("Press CTRL-D (or any other key) to exit.\n\n");
    //while(fgetc(stdin) == '\n') {
    while(1) {
        time(&timer);
        tm_info = localtime(&timer);
        strftime(timebuf, 26, "%Y-%m-%d %H:%M:%S", tm_info);

        /* print a message */
        snprintf(application_message, sizeof(application_message), "%s ,%s", timebuf,mqtt_mess);
        printf("MQTT message \"%s\"", application_message);
        printf("\n Publish\n");

        /* publish the time */
        mqtt_publish(&client, topic, application_message, strlen(application_message) + 1, MQTT_PUBLISH_QOS_0);

        /* check for errors */
        if (client.error != MQTT_OK) {
            fprintf(stderr, "error: %s\n", mqtt_error_str(client.error));
            exit_example(EXIT_FAILURE, sockfd, &client_daemon);
        }
        sleep(10);
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
    exit(status);
}



void publish_callback(void** unused, struct mqtt_response_publish *published)
{
    /* not used in this example */
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
