#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include "../common/common.h"

char servIP[16]={'\0'}; //sender IP address
char flow_max_buf[TG_MAX_WRITE]={'\0'};
int servPort=TG_SERVER_PORT;    //sender TCP port
unsigned int flow_size=1024; //flow size in bytes
unsigned int flow_tos=0;  //ToS value of flows
unsigned int flow_number=10;  //number of flows
unsigned int flow_rate=0; //sending rate of flows

/* Print usage of the program */
void print_usage(char *program);
/* Read command line arguments */
void read_args(int argc, char *argv[]);

int main(int argc, char *argv[])
{
    unsigned int i = 0;
    unsigned int flow_id = 0;
    struct timeval tv_start, tv_end;    //start and end time
    int sockfd; //Socket
    int sock_opt = 1;
    struct sockaddr_in serv_addr;   //Server address
    unsigned int meta_data_size = 4 * sizeof(unsigned int);
    char buf[4 * sizeof(unsigned int)] = {'\0'}; // buffer to hold meta data
    unsigned int fct_us;
    unsigned int goodput_mbps;

    read_args(argc,argv);

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(servIP);
    serv_addr.sin_port = htons(servPort);

    /* Initialize server socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("Error: initialize socket");

    /* Set socket options */
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &sock_opt, sizeof(sock_opt)) < 0)
        error("Error: set SO_REUSEADDR option");
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &sock_opt, sizeof(sock_opt)) < 0)
        error("ERROR: set TCP_NODELAY option");
    if (setsockopt(sockfd, IPPROTO_IP, IP_TOS, &flow_tos, sizeof(flow_tos)) < 0)
        error("Error: set IP_TOS option");

    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("Error: connect");

    for (i = 0; i < flow_number; i ++)
    {
        printf("Generate flow request %u\n", i);

        /* fill in meta data */
        memcpy(buf, &i, sizeof(unsigned int));
        memcpy(buf + sizeof(unsigned int), &flow_size, sizeof(unsigned int));
        memcpy(buf + 2 * sizeof(unsigned int), &flow_tos, sizeof(unsigned int));
        memcpy(buf + 3 * sizeof(unsigned int), &flow_rate, sizeof(unsigned int));

        gettimeofday(&tv_start, NULL);

        if(write_exact(sockfd, buf, meta_data_size, meta_data_size, 0, flow_tos, 0, false) != meta_data_size)
            error("Error: write meta data");

        if (read_exact(sockfd, buf, meta_data_size, meta_data_size, false) != meta_data_size)
            error("Error: read meata data");

        /* extract meta data */
        memcpy(&flow_id, buf, sizeof(unsigned int));
        memcpy(&flow_size, buf + sizeof(unsigned int), sizeof(unsigned int));
        memcpy(&flow_tos, buf + 2 * sizeof(unsigned int), sizeof(unsigned int));
        memcpy(&flow_rate, buf + 3 * sizeof(unsigned int), sizeof(unsigned int));

        if (read_exact(sockfd, flow_max_buf, flow_size, TG_MAX_WRITE, true) != flow_size)
            error("Error: receive flow");

        gettimeofday(&tv_end, NULL);
        fct_us = (tv_end.tv_sec - tv_start.tv_sec) * 1000000 + (tv_end.tv_usec - tv_start.tv_usec);
        goodput_mbps = flow_size * 8 / fct_us;

        printf("Flow: ID: %u Size: %u ToS: %u Rate: %u\n", flow_id, flow_size, flow_tos, flow_rate);
        printf("FCT: %u us Goodput: %u Mbps\n", fct_us, goodput_mbps);
    }

    close(sockfd);
    return 0;
}

void print_usage(char *program)
{
    printf("Usage: %s [options]\n", program);
    printf("-s <sender>        IP address of sender\n");
    printf("-p <port>          port number (default %d)\n", TG_SERVER_PORT);
    printf("-n <bytes>         flow size in bytes (default %u)\n", flow_size);
    printf("-q <tos>           Type of Service (ToS) value (default %u)\n", flow_tos);
    printf("-c <count>         number of flows (default %u)\n", flow_number);
    printf("-r <rate (Mbps)>   sending rate of flows (default 0: no rate limiting)\n");
    printf("-h                 display help information\n");
}

void read_args(int argc, char *argv[])
{
    int i=1;
    while (i < argc)
    {
        if (strlen(argv[i]) == 2 && strcmp(argv[i], "-s") == 0)
        {
            if (i+1 < argc)
            {
                if (strlen(argv[i+1]) <= 15)
                    strncpy(servIP, argv[i+1], strlen(argv[i+1]));
                else
                    error("Illegal IP address\n");
                //printf("Server IP address %s\n", servIP);
                i += 2;
            }
            /* cannot read IP address */
            else
            {
                printf("Cannot read IP address\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
        }
        else if (strlen(argv[i]) == 2 && strcmp(argv[i], "-p") == 0)
        {
            if (i+1 < argc)
            {
                servPort = atoi(argv[i+1]);
                if (servPort < 0 || servPort > 65535)
                    error("Illegal port number");
                i += 2;
            }
            /* cannot read port number */
            else
            {
                printf("Cannot read port number\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
        }
        else if (strlen(argv[i]) == 2 && strcmp(argv[i], "-n") == 0)
        {
            if (i+1 < argc)
            {
                sscanf(argv[i+1], "%u", &flow_size);
                i += 2;
            }
            /* cannot read flow size */
            else
            {
                printf("Cannot read flow size\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
        }
        else if (strlen(argv[i]) == 2 && strcmp(argv[i], "-q") == 0)
        {
            if (i+1 < argc)
            {
                sscanf(argv[i+1], "%u", &flow_tos);
                i += 2;
            }
            /* cannot read ToS value */
            else
            {
                printf("Cannot read ToS\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
        }
        else if (strlen(argv[i]) == 2 && strcmp(argv[i], "-c") == 0)
        {
            if (i+1 < argc)
            {
                sscanf(argv[i+1], "%u", &flow_number);
                i += 2;
            }
            /* cannot read number of flows */
            else
            {
                printf("Cannot read number of flows\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
        }
        else if (strlen(argv[i]) == 2 && strcmp(argv[i], "-r") == 0)
        {
            if (i+1 < argc)
            {
                sscanf(argv[i+1], "%u", &flow_rate);
                i += 2;
            }
            /* cannot read sending rate of flows */
            else
            {
                printf("Cannot read sending rate of flows\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
        }
        else if (strlen(argv[i]) == 2 && strcmp(argv[i], "-h") == 0)
        {
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("Invalid option %s\n", argv[i]);
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
}
