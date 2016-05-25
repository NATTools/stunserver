
#include <stdio.h>
#include <string.h>

#include <stdlib.h>
#include <stdarg.h>

#include <poll.h>
#include <pthread.h>

#include <stunclient.h>
#include "sockethelper.h"
#include "utils.h"


int
createLocalSocket(int                    ai_family,
                  const struct sockaddr* localIp,
                  int                    ai_socktype,
                  uint16_t               port)
{
  int sockfd;

  int             rv;
  struct addrinfo hints, * ai, * p;
  char            addr[SOCKADDR_MAX_STRLEN];
  char            service[8];

  sockaddr_toString(localIp, addr, sizeof addr, false);

  /* itoa(port, service, 10); */

  snprintf(service, 8, "%d", port);
  /* snprintf(service, 8, "%d", 3478); */


  /* get us a socket and bind it */
  memset(&hints, 0, sizeof hints);
  hints.ai_family   = ai_family;
  hints.ai_socktype = ai_socktype;
  hints.ai_flags    = AI_NUMERICHOST | AI_ADDRCONFIG;


  if ( ( rv = getaddrinfo(addr, service, &hints, &ai) ) != 0 )
  {
    fprintf(stderr, "selectserver: %s ('%s')\n", gai_strerror(rv), addr);
    exit(1);
  }

  for (p = ai; p != NULL; p = p->ai_next)
  {
    if ( sockaddr_isAddrAny(p->ai_addr) )
    {
      /* printf("Ignoring any\n"); */
      continue;
    }

    if ( ( sockfd = socket(p->ai_family, p->ai_socktype,
                           p->ai_protocol) ) == -1 )
    {
      perror("client: socket");
      continue;
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) < 0)
    {
      printf("Bind failed\n");
      close(sockfd);
      continue;
    }

    if (localIp != NULL)
    {
      struct sockaddr_storage ss;
      socklen_t               len = sizeof(ss);
      if (getsockname(sockfd, (struct sockaddr*)&ss, &len) == -1)
      {
        perror("getsockname");
      }
      else
      {
        if (ss.ss_family == AF_INET)
        {
          ( (struct sockaddr_in*)p->ai_addr )->sin_port =
            ( (struct sockaddr_in*)&ss )->sin_port;
        }
        else
        {
          ( (struct sockaddr_in6*)p->ai_addr )->sin6_port =
            ( (struct sockaddr_in6*)&ss )->sin6_port;
        }
      }
    }
    break;
  }
  return sockfd;
}

void*
handleRequest(void* ptr)
{
  struct Request* request = (struct Request*)ptr;
  if ( stunlib_isStunMsg(request->buf, request->numbytes) )
  {
    /* Send to STUN, with CB to data handler if STUN packet contations
     * DATA */
    request->stun_handler(request->socketConfig,
                          (struct sockaddr*)&request->their_addr,
                          request->tInst,
                          request->buf,
                          request->numbytes);
  }
  free(request);
  /* pthread_exit(NULL); */
  return NULL;
}

void*
socketListenDemux(void* ptr)
{
  struct pollfd        ufds[10];
  struct listenConfig* config = (struct listenConfig*)ptr;
  /* struct sockaddr_storage their_addr; */
  /* unsigned char           buf[MAXBUFLEN]; */
  /* socklen_t               addr_len; */
  int rv;
  /* int                     numbytes; */
  int i;

  /* int  keyLen = 16; */
  /* char md5[keyLen]; */
  config->thread_no = 0;
  for (i = 0; i < config->numSockets; i++)
  {
    ufds[i].fd     = config->socketConfig[i].sockfd;
    ufds[i].events = POLLIN;
  }

  /*  */

  while (1)
  {
    rv = poll(ufds, config->numSockets, -1);
    if (rv == -1)
    {
      perror("poll");       /* error occurred in poll() */
    }
    else if (rv == 0)
    {
      printf("Timeout occurred! (Should not happen)\n");
    }
    else
    {
      /* check for events on s1: */
      for (i = 0; i < config->numSockets; i++)
      {
        if (ufds[i].revents & POLLIN)
        {
          struct Request* request = malloc( sizeof(struct Request) );
          request->socketConfig = &config->socketConfig[i];
          request->tInst        = config->tInst;
          request->stun_handler = config->stun_handler;
          request->addr_len     = sizeof request->their_addr;
          if ( ( request->numbytes =
                   recvfrom(request->socketConfig->sockfd,
                            request->buf, MAXBUFLEN, 0,
                            (struct sockaddr*)&request->their_addr,
                            &request->addr_len) ) == -1 )
          {
            perror("recvfrom");
            exit(1);
          }
          int pt = pthread_create(&config->threads[config->thread_no],
                                  NULL,
                                  handleRequest,
                                  (void*)request);
          if (pt)
          {
            perror("Failed to create thread");
            printf(
              "Could not create thread for STUN handling.. (Ret:%i num:%i)\n",
              pt,
              config->thread_no++);
            exit(EXIT_FAILURE);
          }
          else
          {
            config->thread_no++;
            if (config->thread_no + 1 >= MAX_THREADS)
            {
              /* We roll over.. Hopefully the old threads are finished by now..
               * */
              config->thread_no = 0;

            }
          }
        }
      }
    }
  }
}




void
sendPacket(void*                  ctx,
           int                    sockHandle,
           const uint8_t*         buf,
           int                    bufLen,
           const struct sockaddr* dstAddr,
           int                    proto,
           bool                   useRelay,
           uint8_t                ttl)
{
  int32_t numbytes;
  /* char addrStr[SOCKADDR_MAX_STRLEN]; */
  uint32_t sock_ttl;
  uint32_t addr_len;
  (void) ctx;
  (void) proto; /* Todo: Sanity check? */
  (void) useRelay;

  if (dstAddr->sa_family == AF_INET)
  {
    addr_len = sizeof(struct sockaddr_in);
  }
  else
  {
    addr_len = sizeof(struct sockaddr_in6);
  }

  if (ttl > 0)
  {
    /*Special TTL, set it send packet and set it back*/
    int          old_ttl;
    unsigned int optlen;
    if (dstAddr->sa_family == AF_INET)
    {
      getsockopt(sockHandle, IPPROTO_IP, IP_TTL, &old_ttl, &optlen);
    }
    else
    {
      getsockopt(sockHandle, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &old_ttl,
                 &optlen);
    }

    sock_ttl = ttl;

    /* sockaddr_toString(dstAddr, addrStr, SOCKADDR_MAX_STRLEN, true); */
    /* printf("Sending Raw (To: '%s'(%i), Bytes:%i/%i  (Addr size: %u)\n",
     * addrStr, sockHandle, numbytes, bufLen,addr_len); */

    if (dstAddr->sa_family == AF_INET)
    {
      setsockopt( sockHandle, IPPROTO_IP, IP_TTL, &sock_ttl, sizeof(sock_ttl) );
    }
    else
    {
      setsockopt( sockHandle, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &sock_ttl,
                  sizeof(sock_ttl) );
    }

    if ( ( numbytes =
             sendto(sockHandle, buf, bufLen, 0, dstAddr, addr_len) ) == -1 )
    {
      perror("Stun sendto");
      exit(1);
    }
    if (dstAddr->sa_family == AF_INET)
    {
      setsockopt(sockHandle, IPPROTO_IP, IP_TTL, &old_ttl, optlen);
    }
    else
    {
      setsockopt(sockHandle, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &old_ttl, optlen);
    }


  }
  else
  {
    /*Nothing special, just send the packet*/
    if ( ( numbytes =
             sendto(sockHandle, buf, bufLen, 0, dstAddr, addr_len) ) == -1 )
    {
      perror("Stun sendto");
      exit(1);
    }
  }
}
