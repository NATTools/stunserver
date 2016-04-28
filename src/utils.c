

#include <stdio.h>
#include <stdlib.h>
#include <stunlib.h>
#include <time.h>
#include <string.h>

#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <arpa/inet.h>
#include "utils.h"


typedef struct {
  uint64_t ms64;
  uint64_t ls64;
} _uint128_t;

typedef union {
  _uint128_t ui128;
  uint8_t    ba[ 16];
} ui128byteArray_t;


uint64_t
random64(void);
_uint128_t
random128(void);


uint64_t
random64(void)
{
  uint64_t p2 = ( (uint64_t)rand() << 62 ) & 0xc000000000000000LL;
  uint64_t p1 = ( (uint64_t)rand() << 31 ) & 0x3fffffff80000000LL;
  uint64_t p0 = ( (uint64_t)rand() <<  0 ) & 0x000000007fffffffLL;

  return p2 | p1 | p0;
}


_uint128_t
random128(void)
{
  _uint128_t result;

  result.ms64 = random64();
  result.ls64 = random64();

  return result;
}


StunMsgId
generateTransactionId(void)
{
  StunMsgId        tId;
  ui128byteArray_t ui128byteArray;

  srand( time(NULL) ); /* Initialise the random seed. */

  ui128byteArray.ui128 = random128();

  memcpy(&tId.octet[ 0], &ui128byteArray.ba[ 0], STUN_MSG_ID_SIZE);

  return tId;
}


int32_t
getICMPTypeFromBuf(sa_family_t    family,
                   unsigned char* rcv_message)
{

  if (family == AF_INET)
  {
    struct ip*   ip_packet;
    struct icmp* icmp_packet;
    ip_packet   = (struct ip*) rcv_message;
    icmp_packet =
      (struct icmp*) ( rcv_message + (ip_packet->ip_hl << 2) );

    return icmp_packet->icmp_type;
  }
  else
  {
    struct icmp6_hdr* icmp_hdr;
    icmp_hdr = (struct icmp6_hdr*) rcv_message;
    return icmp_hdr->icmp6_type;

  }

}


int32_t
getTTLFromBuf(sa_family_t    family,
              int            firstPktLen,
              unsigned char* rcv_message)
{

  if (family == AF_INET)
  {
    int32_t      ttl;
    struct ip*   ip_packet, * inner_ip_packet;
    struct icmp* icmp_packet;
    ip_packet   = (struct ip*) rcv_message;
    icmp_packet =
      (struct icmp*) ( rcv_message + (ip_packet->ip_hl << 2) );
    inner_ip_packet = &icmp_packet->icmp_ip;

    ttl = (inner_ip_packet->ip_len - firstPktLen - 24) / 4;
    return ttl;
  }
  else
  {
    int32_t           ttl_v6;
    uint16_t          paylen;
    uint32_t          stunlen;
    struct ip6_hdr*   inner_ip_hdr;
    struct icmp6_hdr* icmp_hdr;
    icmp_hdr     = (struct icmp6_hdr*) rcv_message;
    inner_ip_hdr = (struct ip6_hdr*)rcv_message + 8;

    paylen  = ntohs(inner_ip_hdr->ip6_plen);
    stunlen = firstPktLen + 4;

    /*FIX me*/
    if (icmp_hdr->icmp6_type != 3)
    {
      return 0;
    }

    ttl_v6 = (paylen - stunlen) / 4;

    return ttl_v6;
  }
  return 0;
}
