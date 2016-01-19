
#ifndef UTILS_H
#define UTILS_H


StunMsgId
generateTransactionId(void);
int32_t
getICMPTypeFromBuf(sa_family_t    family,
                   unsigned char* rcv_message);

int32_t
getTTLFromBuf(sa_family_t    family,
              int            firstPktLen,
              unsigned char* rcv_message);


#endif
