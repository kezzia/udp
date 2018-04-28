#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include "sendlib.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <signal.h>

#define true 1
#define false 0
#define timeout_time 5

ssize_t custom_recvfrom(int sockfd, const void *buff, size_t nbytes, int flags, struct sockaddr* from, socklen_t *fromaddrlen);
int handle_alarm();

FILE *input;

int main(int argc, char *argv[])
{
  int sock;
  short int port;
  struct  sockaddr_in myaddr;
  struct sockaddr_storage storage;
  socklen_t sock_len, storagelen;
  int msglen;
  char *ip_address;
  char *portstr;
  char *endptr;
  int MaxMsgLen = 1024;

  // Variables to be pasted to server
  char *file_path;
  char *format;
  char *target;
  char *loss_prob;
  char *rseed;
  char ack[500];
  int acklen = 500;

  // Get command line arguments

  if(argc == 8)
  {
    ip_address = argv[1];
    portstr = argv[2];
    file_path = argv[3];
    format = argv[4];
    target = argv[5];
    loss_prob = argv[6];
    rseed = argv[7];
  }
  else
  {
    printf("Invalid amount of arguments.\nGood bye!!");
    return 0;
  }

  // Conversion of variable to correct format
  float loss_probability = atof(loss_prob);
  int random_seed = atoi(rseed);


  port = strtol(portstr, &endptr, 0);
  if ( *endptr )
  {
    printf("ECHOCLNT: Invalid port supplied.\n");
    exit(EXIT_FAILURE);
  }

  // Create the listening socket

  if((sock = socket (PF_INET, SOCK_DGRAM, 0)) < 0)
  {
    fprintf(stderr, "ECHOSERV: Error creating listening socket.\n");
    exit(EXIT_FAILURE);
  }

  memset((char*) &myaddr, 0, sizeof(myaddr));
  myaddr.sin_family = AF_INET;
  myaddr.sin_port = htons(port); //This is my port
  myaddr.sin_addr.s_addr = inet_addr(ip_address); // This is my IP

  // Setting the socket length base off of the socket address.

  sock_len = sizeof(myaddr);
  storagelen = sizeof(storage);


  // Send Request to Server Program

  while(1) {

    char msg[1024];
    sprintf(msg, "%s,%s,%s", file_path,format,target);
    MaxMsgLen = strlen(msg)+1;

  int resend_num = 0;
  RESEND:
  if (resend_num > 5) {
    printf("Too many resends\n");
    return 0;
  }

    msglen = lossy_sendto(loss_probability, random_seed, sock, msg, MaxMsgLen, (struct sockaddr *)&myaddr, sock_len);
    /// To Recieve Ack from Server
    acklen = 500;
    int test = custom_recvfrom(sock, ack, acklen, 0, (struct sockaddr *)&storage, &storagelen);
    ///This code would take you back to resend the last bit of data due to a timeout
    if(test == -5) {
      printf("Packet timed out");
      goto RESEND;
      resend_num = resend_num + 1;
    }
    if(test == -4)
    {
      printf("Some other error occured\n");
      return 0;
    }
    resend_num = 0;
    printf("ACK received, packet sent!\n");

    ///Opening the File to send the data.
    input = fopen(file_path, "r");
    if(input == NULL) {
      printf("Invalid file\n");
      return 0;
    }

    ///Sending the data from File line by line.
    int maxline = 1024;
    char line[1024];
    char message[1024];
    while(1) {
      if(fgets(line, sizeof(line), input) == NULL) {
        ///Sending end to Server so it know we are at end of file
        char end[] = "end";
        memset(message,0,strlen(message));
        sprintf(message, "%s", end);
        maxline = strlen(message) + 1;

        /// The line below is a label to jump back to in order to resend the last packet as there was a timeout.
        RESEND1:
        resend_num++;
        if (resend_num > 5) {
          printf("Too many resends\n");
          return 0;
        }

        acklen = 500;

        test = custom_recvfrom(sock, ack, acklen, 0, (struct sockaddr *)&storage, &storagelen);
        ///This code would take you back to resend the last bit of data due to a timeout
        if(test == -5) {
          printf("Packet timeout\n\n");
          goto RESEND1;
          resend_num = resend_num + 1;
        }
        if(test == -4) {
          printf("Some other error occured\n");
          return 0;
        }
        //when we send successfully, reset resend_num so we can send more!
        resend_num = 0;
        printf("ACK received, packet sent!\n");

        break;
      }

      ///Send line by line of the data in the file
      sprintf(message, "%s", line);
      maxline = strlen(message) + 1;
      printf("Sending: %s\n", message);

      /// The line below is a label to jump back to in order to resend the last packet as there was a timeout.
      RESEND2:
      resend_num++;
      if (resend_num > 5) {
        printf("Too many resends\n");
        return 0;
      }

      msglen = lossy_sendto(loss_probability, random_seed, sock, message, maxline, (struct sockaddr *)&myaddr, sock_len);

			test = 0;
      test = custom_recvfrom(sock, ack, acklen, 0, (struct sockaddr *)&storage, &storagelen);
      ///This code would take you back to resend the last bit of data due to a timeout
      if(test == -5) {
        printf("Packet timeout\n\n");
        goto RESEND2;
        resend_num = resend_num + 1;
      }
      if(test == -4) {
        printf("Some other error occured\n");
        return 0;
      }

      resend_num = 0;
      printf("ACK received, packet sent!\n");
    }

    ///Handle the Last Response
    /// I did not use the stop and wait version here because this response has to be sent without any request for it
    char ack[1024];
    MaxMsgLen = 1024;
		test = 0;
    test = recvfrom(sock, ack, MaxMsgLen, 0, (struct sockaddr *)&storage, &storagelen);
    if(test < 0 )
      perror("Recieve error");

    if(ack[0] == 'e' && ack[1] == 'r')
    {
      printf("\nFormat Error!\n");
    }
    else if(ack[0] == 's' && ack[1] == 'u')
    {
      printf("\nsuccess!\n");
    }

    break;

  }
}

ssize_t custom_recvfrom(int sockfd, const void *buff, size_t nbytes, int flags, struct sockaddr* from, socklen_t *fromaddrlen)
{
  ///This function is Used to get do the Stop and Wait
  ///It sets an alarm to every 20 seconds that will interupt the recvfrom command that is waiting on an ACK from server
  ssize_t n;

  /// This for Alarms and will test inside VM hopefully.
  struct sigaction action;
  sigemptyset(&action.sa_mask);
  action.sa_handler = (void (*)(int))handle_alarm;
  action.sa_flags = 0;
  if(sigaction(SIGALRM, &action, 0) == -1)
  {
    perror("sigaction");
    return 1;
  }

	//signal(SIGALRM, handle_alarm);
	alarm(2);
  n = recvfrom(sockfd, buff, nbytes, flags, from, fromaddrlen);
  if (n < 0)
  {
		if (errno == EINTR)
			/* timed out */
		{
			return -5;
		}
    else
    {
      /* some other error */
      perror("The reciving on client error is");
      return -4;
    }
  }
  else
    /* no error or time out - turn off alarm */
    alarm(0);

  return n;
}

int handle_alarm()
{
  ///This is the Alarm Handler it show what will happen when an alarm signal is fired
	errno = EINTR;
	alarm(0);
	return -1;
}
