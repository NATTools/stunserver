
#include <stdio.h>
#include <string.h>

#include <stdlib.h>
#include <stdarg.h>

#include <poll.h>
#ifdef __linux
#include <arpa/inet.h>

#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <linux/types.h>        /* required for linux/errqueue.h */
#include <linux/errqueue.h>     /* SO_EE_ORIGIN_ICMP */
#endif


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
socketListenDemux(void* ptr)
{
  struct pollfd           ufds[10];
  struct listenConfig*    config = (struct listenConfig*)ptr;
  struct sockaddr_storage their_addr;
  unsigned char           buf[MAXBUFLEN];
  socklen_t               addr_len;
  int                     rv;
  int                     numbytes;
  int                     i;

  /* int  keyLen = 16; */
  /* char md5[keyLen]; */

  for (i = 0; i < config->numSockets; i++)
  {
    ufds[i].fd     = config->socketConfig[i].sockfd;
    ufds[i].events = POLLIN;
  }

  addr_len = sizeof their_addr;

  while (1)
  {
    rv = poll(ufds, config->numSockets, -1);
    if (rv == -1)
    {
        perror("poll");     /* error occurred in poll() */
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
        #if defined(__linux)
        if (ufds[i].revents & POLLERR)
        {
          /* Do stuff with msghdr */
          struct msghdr      msg;
          struct sockaddr_in response;                          /* host answered
                                                                 * IP_RECVERR */
          char            control_buf[1500];
          struct iovec    iov;
          char            buf[1500];
          struct cmsghdr* cmsg;

          memset( &msg, 0, sizeof(msg) );
          msg.msg_name       = &response;                       /* host */
          msg.msg_namelen    = sizeof(response);
          msg.msg_control    = control_buf;
          msg.msg_controllen = sizeof(control_buf);
          iov.iov_base       = buf;
          iov.iov_len        = sizeof(buf);
          msg.msg_iov        = &iov;
          msg.msg_iovlen     = 1;

          if (recvmsg(ufds[i].fd, &msg, MSG_ERRQUEUE) == -1)
          {
            /* Ignore for now. Will get it later.. */
            continue;
          }
          for ( cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
                cmsg = CMSG_NXTHDR(&msg,cmsg) )
          {
            if ( (cmsg->cmsg_level == IPPROTO_IP) &&
                 (cmsg->cmsg_type == IP_RECVERR) )
            {
              struct sock_extended_err* ee;
              ee = (struct sock_extended_err*) CMSG_DATA(cmsg);

              if (ee->ee_origin == SO_EE_ORIGIN_ICMP)
              {
                config->icmp_handler(&config->socketConfig[1],
                                     SO_EE_OFFENDER(ee),
                                     config->tInst,
                                     ee->ee_type);
              }
            }
          }
          continue;
        }
        #endif
        if (ufds[i].revents & POLLIN)
        {
          if ( ( numbytes =
                   recvfrom(config->socketConfig[i].sockfd, buf, MAXBUFLEN, 0,
                            (struct sockaddr*)&their_addr, &addr_len) ) == -1 )
          {
            perror("recvfrom");
            exit(1);
          }
        }

        if ( stunlib_isStunMsg(buf, numbytes) )
        {
          /* Send to STUN, with CB to data handler if STUN packet contations
           * DATA */
          config->stun_handler(&config->socketConfig[i],
                               (struct sockaddr*)&their_addr,
                               config->tInst,
                               buf,
                               numbytes);
        }
        else
        {

         /* Nasty hack on osx to ignore not ICMP ports.. */
          if(i==0 && config->numSockets==2){
            continue;
          }
          /* TODO IPV6..*/
          config->icmp_handler( &config->socketConfig[i],
                                (struct sockaddr*)&their_addr,
                                config->tInst,
                                getICMPTypeFromBuf(AF_INET, buf) );

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
