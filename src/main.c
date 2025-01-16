#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include "exchange_matcher.h"

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

int main(void) {
  int rc = -1, on = 1, i;
  int server_socket, max_sd, sd, activity, new_socket;

  int client_sockets[MAX_CLIENTS] = {0};
  struct sockaddr_in address;
  fd_set readfds;
  char buffer[BUFFER_SIZE];
  int addrlen = sizeof(address);

  t_list *trades = NULL;
  t_list *bid = NULL;
  t_list *offer = NULL;

  if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    goto clean_up;
  }

  if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
    goto clean_up;
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
    goto clean_up;
  }

  if (listen(server_socket, 3) < 0) {
    goto clean_up;
  }

  trades = new_list(NULL);
  if (!trades) goto clean_up;

  bid = new_list(NULL);
  if(!bid) goto clean_up;

  offer = new_list(NULL);
  if (!offer) goto clean_up;

  printf("Listening on port %d\n", PORT);

  while (1) {
    FD_ZERO(&readfds);
    FD_SET(server_socket, &readfds);
    max_sd = server_socket;

    for (i = 0; i < MAX_CLIENTS; i++) {
      sd = client_sockets[i];
      if (sd > 0) FD_SET(sd, &readfds);
      if (sd > max_sd) max_sd = sd;
    }

    activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
    if ((activity < 0) && (errno != EINTR)) {
      break;
    }

    if (FD_ISSET(server_socket, &readfds)) {
      new_socket = accept(
        server_socket, (struct sockaddr *)&address, (socklen_t *)&addrlen
      );
      if (new_socket < 0) break;
      printf(
        "New connection from %s:%d\n",
        inet_ntoa(address.sin_addr),
        ntohs(address.sin_port)
      );

      for (i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] == 0) {
          client_sockets[i] = new_socket;
          printf("Adding to list of sockets as %d\n", i);
          break;
        }
      }
    }

    for (i = 0; i < MAX_CLIENTS; i++) {
      sd = client_sockets[i];
      if (FD_ISSET(sd, &readfds)) {
        rc = read(sd, buffer, BUFFER_SIZE);
        if (rc == 0) {
          getpeername(sd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
          printf(
            "Host disconnected, ip %s, port %d\n",
            inet_ntoa(address.sin_addr), ntohs(address.sin_port)
          );
          close(sd);
          client_sockets[i] = 0;
        } else {
          buffer[rc] = '\0';

          char type = 0, side = 0, ceil_1 = '0', ceil_2 = '0';
          unsigned order_id = 0, ceil = 0;

          if (buffer[0] == 'C') {
            order_id = atoi(buffer + 2);
            if (cancel_order(order_id, &offer, &bid) == SUCCESS) {
              memset(buffer, 0, sizeof(buffer));
              rc = snprintf(buffer, sizeof(buffer) - 1, "X,%u", order_id);
              if (rc > 0) {
                buffer[rc] = '\0';
                send(sd, buffer, rc, 0);
              }
            } else {
              memset(buffer, 0, sizeof(buffer));
              rc = snprintf(buffer, sizeof(buffer) - 1, "X,%u -> failed", order_id);
              if (rc > 0) {
                buffer[rc] = '\0';
                send(sd, buffer, rc, 0);
              }
            }
          } else if (buffer[0] == 'O') {
            t_order *ord = calloc(1, sizeof(t_order));
            if (!ord) {
              close(sd);
              break;
            }

            if (sscanf(
              buffer, "%c,%u,%c,%u,%u.%c%c", &type, &ord->oid,
              &side, &ord->qty, &ord->price[0], &ceil_1, &ceil_2
            ) < 0) {
              close(sd);
              free(ord);
              break;
            }

            ceil_1 = (ceil_1 > '9' || ceil_1 < '0') ? '0' : ceil_1;
            ceil_2 = (ceil_2 > '9' || ceil_2 < '0') ? '0' : ceil_2;

            ord->price[1] = (ceil_1 - 48) * 10 + ceil_2 - 48;
            if (place_order(side, ord, &bid, &offer, trades) == SUCCESS) {
              ord = NULL;
              t_list *trade = trades->next;
              while (trade) {
                ceil = ((t_trade*)trade->data)->price[1];
                if (!(ceil % 10)) ceil /= 10;
                rc = snprintf(
                  buffer, sizeof(buffer) - 1, "T,%u,%c,%u,%u,%u,%u.%u",
                  ((t_trade *)trade->data)->id, ((t_trade *)trade->data)->side,
                  ((t_trade *)trade->data)->oid1, ((t_trade *)trade->data)->oid2,
                  ((t_trade *)trade->data)->qty, ((t_trade *)trade->data)->price[0],
                  ceil
                );
                if (rc < 0) break;
                buffer[rc] = '\0';
                send(sd, buffer, rc, 0);
                trade = trade->next;
              }
              if (rc > 0) {
                free_list(&(trades->next));
                trades->next = NULL;
              }
            }
          }
        }
      }
    }
  }

clean_up:
  close(server_socket);
  if (trades) free_list(&trades);
  if (bid) free_list(&bid);
  if (offer) free_list(&offer);
  return rc;
}
