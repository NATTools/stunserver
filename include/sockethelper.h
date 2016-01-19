#ifndef SOCKETHELPER_H
#define SOCKETHELPER_H

#include <sockaddr_util.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>

#include <stunlib.h>

#define MAXBUFLEN 4048
#define MAX_LISTEN_SOCKETS 10


struct socketConfig {
  int   sockfd;
  char* user;
  char* pass;
  char* realm;
  int   firstPktLen;
};


struct listenConfig {
  void* tInst;
  struct socketConfig socketConfig[MAX_LISTEN_SOCKETS];
  int                 numSockets;
  /*Handles normal data like RTP etc */
  void (* icmp_handler)(struct socketConfig*,
                        struct sockaddr    *,
                        void               *,
                        int);
  /*Handles STUN packet*/
  void (* stun_handler)(struct socketConfig*,
                        struct sockaddr    *,
                        void               *,
                        unsigned char      *,
                        int);
};

int
createLocalSocket(int                    ai_family,
                  const struct sockaddr* localIp,
                  int                    ai_socktype,
                  uint16_t               port);


void*
socketListenDemux(void* ptr);

void
sendPacket(void*                  ctx,
           int                    sockHandle,
           const uint8_t*         buf,
           int                    bufLen,
           const struct sockaddr* dstAddr,
           int                    proto,
           bool                   useRelay,
           uint8_t                ttl);


#endif
