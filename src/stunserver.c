#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <pthread.h>
#include <getopt.h>

#include <stdbool.h>

#include <stunlib.h>
#include <stunclient.h>
#include "sockethelper.h"
#include "utils.h"
#include "iphelper.h"
#include "version.h"

#define MYPORT "3478"    /* the port users will be connecting to */
#define PASSWORD "VOkJxbRl1RmTxUk/WvJxBt"
#define max_iface_len 10
int sockfd;


void
teardown()
{
  close(sockfd);
  printf("Quitting...\n");
  exit(0);
}


void
stunHandler(struct socketConfig* config,
            struct sockaddr*     from_addr,
            void*                cb,
            unsigned char*       buf,
            int                  buflen)
{
  StunMessage            stunRequest;
  STUN_INCOMING_REQ_DATA pReq;
  STUN_CLIENT_DATA*      clientData = (STUN_CLIENT_DATA*)cb;
  char                   realm[STUN_MSG_MAX_REALM_LENGTH];

  printf("Got a STUN message... (%i)\n", buflen);
  stunlib_DecodeMessage(buf, buflen, &stunRequest, NULL, stdout);
  printf("Finished decoding..\n");
  if (stunRequest.msgHdr.msgType == STUN_MSG_DataIndicationMsg)
  {
    if (stunRequest.hasData)
    {
      /* Decode and do something with the data? */
      /* config->data_handler(config->socketConfig[i].sockfd, */
      /*                     config->socketConfig[i].tInst, */
      /*                     &buf[stunResponse.data.offset]); */
    }
  }
  if (stunRequest.hasRealm)
  {
    memcpy(&realm, stunRequest.realm.value, STUN_MSG_MAX_REALM_LENGTH);
  }

  if (stunRequest.hasMessageIntegrity)
  {
    printf("Checking integrity..%s\n", config->pass);
    if ( stunlib_checkIntegrity( buf,
                                 buflen,
                                 &stunRequest,
                                 (uint8_t*)config->pass,
                                 strlen(config->pass) ) )
    {
      printf("     - Integrity check OK\n");
    }
    else
    {
      printf("     - Integrity check NOT OK\n");
    }
  }

  StunServer_HandleStunIncomingBindReqMsg(clientData,
                                          &pReq,
                                          &stunRequest,
                                          false);

  StunServer_SendConnectivityBindingResp(clientData,
                                         config->sockfd,
                                         stunRequest.msgHdr.id,
                                         PASSWORD,
                                         from_addr,
                                         from_addr,
                                         NULL,
                                         sendPacket,
                                         SOCK_DGRAM,
                                         false,
                                         200,
                                         NULL);

}

void
printUsage()
{
  printf("Usage: nptrace [options] host\n");
  printf("Options: \n");
  printf("  -i, --interface               Interface\n");
  printf("  -p, --port                    Destination port\n");
  printf("  -v, --version                 Prints version number\n");
  printf("  -h, --help                    Print help text\n");
  exit(0);

}

int
main(int   argc,
     char* argv[])
{
  //struct addrinfo*        servinfo;
  //int                     numbytes;
  //struct sockaddr_storage their_addr;
  //unsigned char           buf[MAXBUFLEN];
  struct sockaddr_storage localAddr;
  char                    interface[10];
  int port;

  //StunMessage            stunRequest;
  //STUN_INCOMING_REQ_DATA pReq;

  pthread_t socketListenThread;

  STUN_CLIENT_DATA*   clientData;
  struct listenConfig listenConfig;

  StunClient_Alloc(&clientData);

  signal(SIGINT, teardown);
  int                 c;
  /* int                 digit_optind = 0; */
  /* set config to default values */
  strncpy(interface, "default", 7);
  port         = 3478;

  static struct option long_options[] = {
    {"interface", 1, 0, 'i'},
    {"port", 1, 0, 'p'},
    {"help", 0, 0, 'h'},
    {"version", 0, 0, 'v'},
    {NULL, 0, NULL, 0}
  };
  if (argc < 1)
  {
    printUsage();
    exit(0);
  }
  int option_index = 0;
  while ( ( c = getopt_long(argc, argv, "hvi:p:j:m:M:w:r:",
                            long_options, &option_index) ) != -1 )
  {
    /* int this_option_optind = optind ? optind : 1; */
    switch (c)
    {
    case 'i':
      strncpy(interface, optarg, max_iface_len);
      break;
    case 'p':
      port = atoi(optarg);
      break;
    case 'h':
      printUsage();
      break;
    case 'v':
      printf("Version %s\n", VERSION_SHORT);
      exit(0);
      break;
    default:
      printf("?? getopt returned character code 0%o ??\n", c);
    }
  }

  if ( !getLocalInterFaceAddrs( (struct sockaddr*)&localAddr,
                                interface,
                                AF_INET,
                                IPv6_ADDR_NORMAL,
                                false ) )
  {
    printf("Error getting IPaddr on %s\n", interface);
    exit(1);
  }

  //sockfd = createSocket(NULL, MYPORT, AI_PASSIVE, servinfo, &p);
  sockfd = createLocalSocket(AF_INET,
                             (struct sockaddr*)&localAddr,
                             SOCK_DGRAM,
                             port);

  listenConfig.tInst  = clientData;
  listenConfig.socketConfig[0].sockfd = sockfd;
  listenConfig.socketConfig[0].user   = NULL;
  listenConfig.socketConfig[0].pass   = PASSWORD;
  listenConfig.stun_handler           = stunHandler;
  listenConfig.numSockets             = 1;



  pthread_create(&socketListenThread,
                 NULL,
                 socketListenDemux,
                 (void*)&listenConfig);

  while (1)
  {
    printf("stunserver: waiting to recvfrom...\n");

    sleep(1000);
  }
}
