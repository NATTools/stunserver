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
/* #include <stunclient.h> */
#include <stunserver.h>
#include "sockethelper.h"
#include "utils.h"
#include "iphelper.h"
#include "version.h"

#define MYPORT "3478"    /* the port users will be connecting to */
#define PASSWORD "VOkJxbRl1RmTxUk/WvJxBt"
#define max_iface_len 10
int             sockfd;
static uint32_t upstream_loss;

#define MAX_TRANS_IDS 100000
struct transIDInfo {
  StunMsgId id;
  uint32_t  cnt;
  uint32_t  tick;
};
uint32_t transIDSinUse;
uint32_t maxTransIDSinUse = 0;
uint32_t stun_pkt_cnt     = 0;
uint32_t max_stun_pkt_cnt = 0;
uint32_t byte_cnt         = 0;
uint32_t max_byte_cnt     = 0;

unsigned long long cpu_user, cpu_nice, cpu_system, cpu_idle, cpu_iowait;

bool csv_output;

struct transIDInfo transIDs[MAX_TRANS_IDS];
pthread_mutex_t    mutexTransId;


static void
printCSV(/* arguments */)
{
  struct timeval start;
  time_t         nowtime;
  struct tm*     nowtm;
  char           tmbuf[64], buf[64];

  gettimeofday(&start, NULL);
  /* gettimeofday(&tv, NULL); */
  nowtime = start.tv_sec;
  nowtm   = localtime(&nowtime);
  strftime(tmbuf, sizeof tmbuf, "%Y-%m-%d %H:%M:%S", nowtm);
  #ifdef __APPLE__
  snprintf(buf, sizeof buf, "%s.%06d",  tmbuf, start.tv_usec);
  #else
  snprintf(buf, sizeof buf, "%s.%06ld", tmbuf, start.tv_usec);
  #endif

  printf("%s, ", buf);

  printf("%i, %i, %i, %i, %i, %i, %llu, %llu, %llu, %llu, %llu\n",
         transIDSinUse,
         maxTransIDSinUse,
         stun_pkt_cnt,
         max_stun_pkt_cnt,
         byte_cnt * 8 / 1000,
         max_byte_cnt * 8 / 1000,
         cpu_user, cpu_nice, cpu_system, cpu_idle, cpu_iowait);
}


/*Todo: If we really care protect the variables by a mutex...*/
static void*
transIDCleanup(void* ptr)
{
  struct timespec timer;
  struct timespec remaining;
  (void) ptr;
  unsigned long long a[5], b[5];
  /* char str[7]; */
  FILE* fp;

  timer.tv_sec  = 1;
  timer.tv_nsec = 00000000;

  for (;; )
  {
    fp = fopen("/proc/stat","r");
    if (fp)
    {
      fscanf(fp,"%*s %llu %llu %llu %llu %llu",&a[0],&a[1],&a[2],&a[3], &a[4]);
      fclose(fp);
    }
    nanosleep(&timer, &remaining);
    for (uint32_t i = 0; i < transIDSinUse; i++)
    {
      if (transIDs[i].cnt > 12)
      {
        /* To old remove it.. */
        pthread_mutex_lock(&mutexTransId);
        for (uint32_t j = i + 1; j < transIDSinUse; j++)
        {
          memcpy( &transIDs[j - 1], &transIDs[j], sizeof(struct transIDInfo) );
        }
        transIDSinUse--;
        pthread_mutex_unlock(&mutexTransId);
      }
      else
      {
        pthread_mutex_lock(&mutexTransId);
        transIDs[i].cnt++;
        pthread_mutex_unlock(&mutexTransId);
      }
    }
    if (stun_pkt_cnt > max_stun_pkt_cnt)
    {
      max_stun_pkt_cnt = stun_pkt_cnt;
    }

    if (byte_cnt > max_byte_cnt)
    {
      max_byte_cnt = byte_cnt;
    }

    fp = fopen("/proc/stat","r");
    if (fp)
    {
      fscanf(fp,"%*s %llu %llu %llu %llu %llu",&b[0],&b[1],&b[2],&b[3],&b[4]);
      fclose(fp);
      cpu_user   = b[0] - a[0];
      cpu_nice   = b[1] - a[1];
      cpu_system = b[2] - a[2];
      cpu_idle   = b[3] - a[3];
      cpu_iowait = b[4] - a[4];
    }
    if (csv_output)
    {
      printCSV();
    }
    else
    {
      printf(
        "\rActive Transactions: %i  (Max: %i)   (Trans/sec: %i (%i), kbps: %i (%i))           ",
        transIDSinUse,
        maxTransIDSinUse,
        stun_pkt_cnt,
        max_stun_pkt_cnt,
        byte_cnt * 8 / 1000,
        max_byte_cnt * 8 / 1000);
    }

    stun_pkt_cnt = 0;
    byte_cnt     = 0;
    fflush(stdout);
  }
  return NULL;
}

void
stundbg(void*              ctx,
        StunInfoCategory_T category,
        char*              errStr)
{
  (void) category;
  (void) ctx;
  printf("%s\n", errStr);
}


int32_t
insertTransId(const StunMsgId* id)
{
  if (transIDSinUse >= MAX_TRANS_IDS)
  {
    printf(
      "No more storage left for Transaction ids. Disabling trans counters in reply");
    return -1;
  }
  /* Exist n list? */
  for (uint32_t i = 0; i < transIDSinUse; i++)
  {
    if ( stunlib_transIdIsEqual(id, &transIDs[i].id) )
    {
      pthread_mutex_lock(&mutexTransId);
      transIDs[i].cnt++;
      pthread_mutex_unlock(&mutexTransId);
      return transIDs[i].cnt;
    }
  }
  pthread_mutex_lock(&mutexTransId);
  memcpy( &transIDs[transIDSinUse].id, id, sizeof (StunMsgId) );
  transIDs[transIDSinUse].cnt  = 1;
  transIDs[transIDSinUse].tick = 0;
  transIDSinUse++;
  if (transIDSinUse > maxTransIDSinUse)
  {
    maxTransIDSinUse = transIDSinUse;
  }
  pthread_mutex_unlock(&mutexTransId);
  return 1;
}

void
teardown()
{
  close(sockfd);
  printf("Quitting..\n");
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
  stun_pkt_cnt++;
  byte_cnt += buflen;
/*  printf("Got a STUN message... (%i)\n", buflen); */
  stunlib_DecodeMessage(buf, buflen, &stunRequest, NULL, NULL);
  /* printf("Finished decoding..\n"); */
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

#if 0
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
#endif
  StunServer_HandleStunIncomingBindReqMsg(clientData,
                                          &pReq,
                                          &stunRequest,
                                          false);


  uint32_t lastReqCnt = 0;
  /* uint32_t lastRespCnt = 0; */

  if (stunRequest.hasTransCount)
  {
    lastReqCnt = stunRequest.transCount.reqCnt;
    /* lastRespCnt = stunRequest.transCount.respCnt; */
  }
  if (lastReqCnt >= upstream_loss)
  {
    /* Store some stats.. */
    int32_t num = insertTransId(&stunRequest.msgHdr.id);

    StunServer_SendConnectivityBindingResp(clientData,
                                           config->sockfd,
                                           stunRequest.msgHdr.id,
                                           PASSWORD,
                                           from_addr,
                                           from_addr,
                                           lastReqCnt,
                                           num,
                                           NULL,
                                           sendPacket,
                                           SOCK_DGRAM,
                                           false,
                                           200,
                                           NULL);
  }
  else
  {
    printf("Weird...\n");
  }

}

void
printUsage()
{
  printf("Usage: stunserver [options] host\n");
  printf("Options: \n");
  printf("  -i, --interface               Interface\n");
  printf("  -p, --port                    Listen port\n");
  printf("  -u, --upstream                Upstream losst\n");
  printf("  --csv                         CVS stat output\n");
  printf("  -v, --version                 Prints version number\n");
  printf("  -h, --help                    Print help text\n");
  exit(0);
}

int
main(int   argc,
     char* argv[])
{
  /* struct addrinfo*        servinfo; */
  /* int                     numbytes; */
  /* struct sockaddr_storage their_addr; */
  /* unsigned char           buf[MAXBUFLEN]; */
  struct sockaddr_storage localAddr;
  char                    interface[10];
  int                     port;

  transIDSinUse = 0;
  /* StunMessage            stunRequest; */
  /* STUN_INCOMING_REQ_DATA pReq; */

  pthread_t socketListenThread;
  pthread_t cleanupThread;

  STUN_CLIENT_DATA*   clientData;
  struct listenConfig listenConfig;

  StunClient_Alloc(&clientData);
  /* StunClient_RegisterLogger(clientData, */
  /*                          stundbg, */
  /*                          NULL); */

  signal(SIGINT, teardown);
  int c;
  /* int                 digit_optind = 0; */
  /* set config to default values */
  strncpy(interface, "default", 7);
  port          = 3478;
  upstream_loss = 0;

  static struct option long_options[] = {
    {"interface", 1, 0, 'i'},
    {"port", 1, 0, 'p'},
    {"upstream", 1, 0, 'u'},
    {"csv", 0, 0, '2'},
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
  while ( ( c = getopt_long(argc, argv, "h2vi:p:m:u:",
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
    case 'u':
      upstream_loss = atoi(optarg);
      break;
    case '2':
      csv_output = true;
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

  /* sockfd = createSocket(NULL, MYPORT, AI_PASSIVE, servinfo, &p); */
  sockfd = createLocalSocket(AF_INET,
                             (struct sockaddr*)&localAddr,
                             SOCK_DGRAM,
                             port);
  pthread_mutex_init(&mutexTransId, NULL);
  pthread_create(&cleanupThread, NULL, transIDCleanup, (void*)transIDs);

  listenConfig.tInst                  = clientData;
  listenConfig.socketConfig[0].sockfd = sockfd;
  listenConfig.socketConfig[0].user   = NULL;
  listenConfig.socketConfig[0].pass   = PASSWORD;
  listenConfig.stun_handler           = stunHandler;
  listenConfig.numSockets             = 1;



  pthread_create(&socketListenThread,
                 NULL,
                 socketListenDemux,
                 (void*)&listenConfig);
  pause();

}
