/* Do stuff like whois lookup and geoip here */


#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
whois_query(char*  server,
            char*  query,
            char** response)
{
  char message[100], buffer[1500];
  int  read_size, total_size = 0;
  int  error;
  int  querySocket;

  struct addrinfo hints, * res;

  /* first, load up address structs with getaddrinfo(): */
  memset(&hints, 0, sizeof hints);
  hints.ai_family   = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  error = getaddrinfo(server, "43", &hints, &res);
  if (error)
  {
    return error;
  }
  /* make a socket: */
  querySocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

  /* connect! */
  if (connect(querySocket, res->ai_addr, res->ai_addrlen) < 0)
  {
    perror("connect failed when trying to do whois query");
  }

  /* Now send some data or message */
  sprintf(message, "%s\r\n", query);
  if (send(querySocket, message, strlen(message), 0) < 0)
  {
    perror("send failed");
  }
  /* Now receive the response */
  while ( ( read_size = recv(querySocket, buffer, sizeof(buffer), 0) ) )
  {
    *response = realloc(*response, read_size + total_size);
    if (*response == NULL)
    {
      printf("realloc failed");
    }
    memcpy(*response + total_size, buffer, read_size);
    total_size += read_size;
  }

  *response                 = realloc(*response, total_size + 1);
  *(*response + total_size) = '\0';

  close(querySocket);
  return 0;
}


int
parseOrigin(char* as)
{
  int num = 0;
  if (as != NULL)
  {
    char* cnum;

    cnum = strrchr(as, ':');
    cnum++;
    while ( isspace(*cnum) )
    {
      cnum++;
    }
    num = atoi(cnum + 2);
    return num;
  }
  return 0;
}

int
getAsNumber(char* buffer,
            char* nextServer,
            int   sizeSrv)
{
  char* tok, * as, * serv;
  int   num;

  tok = strtok(buffer, "\n");
  while (tok != NULL)
  {
    /* Check for origin: */
    as = strstr(tok, "origin:");
    if (as != NULL)
    {
      num = parseOrigin(tok);
      if (num != 0)
      {
        return num;
      }
    }
    as = strstr(tok, "OriginAS:");
    if (as != NULL)
    {
      num = parseOrigin(tok);
      if (num != 0)
      {
        return num;
      }
    }

    /* While we are at it set nextserv as well */
    serv = strstr(tok, "refer:");
    if ( (serv != NULL) )
    {
      serv = serv + 7;
      while ( isspace(*serv) )
      {
        serv++;
      }
      strncpy(nextServer, serv, sizeSrv);
    }
    serv = strstr(tok, "ReferralServer:");
    if ( (serv != NULL) )
    {
      serv = serv + 16;
      while ( isspace(*serv) )
      {
        serv++;
      }
      serv = serv + strlen("whois://");
      /* printf("Setting next server!\n"); */
      strncpy(nextServer, serv, sizeSrv);
    }
    /* Next line */
    tok = strtok(NULL, "\n");
  }
  return 0;
}


int
asLookup(char* ip)
{
  char* response = NULL;

  int  asNum = 0;
  char server[256];
  int  i = 0;
  strcpy(server, "whois.iana.org\0");

  /* Sometimes we need to ask 3 different servers.. */
  while (asNum == 0 && i <= 3)
  {
    i++;
    if ( whois_query(server, ip, &response) )
    {
      free(response);
      return 0;
    }
    asNum = getAsNumber( response, server, sizeof(server) );

  }

      free(response);
  return asNum;
}
