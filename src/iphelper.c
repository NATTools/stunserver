#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#define _OPEN_SYS_SOCK_IPV6
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <string.h>

#include <sockaddr_util.h>

#ifndef   NI_MAXHOST
#define   NI_MAXHOST 1025
#endif


#include "iphelper.h"

bool
getLocalInterFaceAddrs(struct sockaddr* addr,
                       char*            iface,
                       int              ai_family,
                       IPv6_ADDR_TYPE   ipv6_addr_type,
                       bool             force_privacy)
{
  struct ifaddrs* ifaddr, * ifa;
  int             family, s;
  char            host[NI_MAXHOST];
  (void) force_privacy;

  if (getifaddrs(&ifaddr) == -1)
  {
    perror("getifaddrs");
    exit(EXIT_FAILURE);
  }

  /* Walk through linked list, maintaining head pointer so we
   *  can free list later */

  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
  {
    if (ifa->ifa_addr == NULL)
    {
      continue;
    }

    if (ifa->ifa_addr->sa_family != ai_family)
    {
      continue;
    }

    if ( sockaddr_isAddrLoopBack(ifa->ifa_addr) )
    {
      continue;
    }

    if ( (strcmp(iface, ifa->ifa_name) != 0) && strcmp(iface, "default\0") )
    {
      continue;
    }

    family = ifa->ifa_addr->sa_family;

    if (family == AF_INET6)
    {
#if defined(__APPLE__)
      if ( sockaddr_isAddrDeprecated( ifa->ifa_addr, ifa->ifa_name,
                                      sizeof(ifa->ifa_name) ) )
      {
        continue;
      }
      if ( sockaddr_isAddrLinkLocal(ifa->ifa_addr) )
      {
        continue;
      }
      if ( sockaddr_isAddrSiteLocal(ifa->ifa_addr) )
      {
        continue;
      }

      if (force_privacy)
      {
        if ( !sockaddr_isAddrTemporary( ifa->ifa_addr,
                                        ifa->ifa_name,
                                        sizeof(ifa->ifa_name) ) )
        {
          continue;
        }
      }
#endif
      switch (ipv6_addr_type)
      {
      case IPv6_ADDR_NONE:
        /* huh */
        break;
      case IPv6_ADDR_ULA:
        if ( !sockaddr_isAddrULA(ifa->ifa_addr) )
        {
          continue;
        }

        break;
      case IPv6_ADDR_PRIVACY:
  #if defined(__APPLE__)
        if ( !sockaddr_isAddrTemporary( ifa->ifa_addr,
                                        ifa->ifa_name,
                                        sizeof(ifa->ifa_name) ) )
        {
          continue;
        }
        #endif
        break;
      case IPv6_ADDR_NORMAL:
        if ( sockaddr_isAddrULA(ifa->ifa_addr) )
        {
          continue;
        }
        break;
      }      /* switch */

    }    /* IPv6 */

    s = getnameinfo(ifa->ifa_addr,
                    (family == AF_INET) ? sizeof(struct sockaddr_in) :
                    sizeof(struct sockaddr_in6),
                    host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
    if (s != 0)
    {
      /* printf("getnameinfo() failed: %s\n", gai_strerror(s)); */
      exit(EXIT_FAILURE);
    }
    break;     /* for */

  }  /* for */
  freeifaddrs(ifaddr);

  if ( sockaddr_initFromString(addr, host) )
  {
    return true;
  }

  return false;
}


int
getRemoteIpAddr(struct sockaddr* remoteAddr,
                char*            fqdn,
                uint16_t         port)
{
  struct addrinfo hints, * res, * p;
  int             status;
  /* char ipstr[INET6_ADDRSTRLEN]; */

  memset(&hints, 0, sizeof hints);
  hints.ai_family   = AF_UNSPEC; /* AF_INET or AF_INET6 to force version */
  hints.ai_socktype = SOCK_STREAM;

  if ( ( status = getaddrinfo(fqdn, NULL, &hints, &res) ) != 0 )
  {
    fprintf( stderr, "getaddrinfo: %s\n", gai_strerror(status) );
    return 2;
  }

  for (p = res; p != NULL; p = p->ai_next)
  {
    /* void* addr; */
    /* get the pointer to the address itself, */
    /* different fields in IPv4 and IPv6: */
    if (p->ai_family == AF_INET)       /* IPv4 */
    {
      struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
      if (ipv4->sin_port == 0)
      {
        sockaddr_initFromIPv4Int( (struct sockaddr_in*)remoteAddr,
                                  ipv4->sin_addr.s_addr,
                                  htons(port) );
      }
      /* addr = &(ipv4->sin_addr); */
    }
    else         /* IPv6 */
    {
      struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)p->ai_addr;
      if (ipv6->sin6_port == 0)
      {
        sockaddr_initFromIPv6Int( (struct sockaddr_in6*)remoteAddr,
                                  ipv6->sin6_addr.s6_addr,
                                  htons(port) );
      }
      /* addr = &(ipv6->sin6_addr); */
    }
  }
  freeaddrinfo(res);   /* free the linked list */
  return 1;
}
