/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * network.cpp
 * Implementation of the Network class
 * Copyright (C) 2005  Simon Newton
 */

#include "network.h"
#include <llad/logger.h>

#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>


/*
 * Create a new Network object
 */
Network::Network() {
  m_sd = 0;

}

/*
 * Clean up handlers
 *
 *
 */
Network::~Network() {
  unsigned int i;
  vector<Listener*>::iterator it;

  for (i = 0; i < m_rhandlers_vect.size(); i++) {
    delete m_rhandlers_vect[i];
  }

  for (i = 0; i < m_whandlers_vect.size(); i++) {
    delete m_whandlers_vect[i];
  }

  m_rhandlers_vect.clear();
  m_whandlers_vect.clear();
}


/*
 * Initialise this network object
 *
 *
 */
int Network::init() {
  struct sockaddr_in servaddr;

  m_sd = socket(AF_INET, SOCK_DGRAM, 0);

  if (m_sd < 0) {
    Logger::instance()->log(Logger::CRIT, "Failed to create socket %s", strerror(errno));
    goto e_socket;
  }

  // setup
  memset(&servaddr, 0x00, sizeof(servaddr) );
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(LLAD_PORT);
  inet_aton(LLAD_ADDR, &servaddr.sin_addr);

  if( bind(m_sd, (struct sockaddr *) &servaddr, sizeof(servaddr) ) == -1 ) {
    Logger::instance()->log(Logger::CRIT, "Bind error %s", strerror(errno));
    goto e_bind;
  }

  return 0;

e_bind:
  close(m_sd);
e_socket:
  return -1;
}




/*
 * Register a file descriptor
 *
 * @param  fd    the file descriptor to register
 * @param  dir    LLA_FD_RD (read) or LLA_FD_WR (write)
 * @param  fh    the function to invoke
 * @param  data  data passed to the handler
 *
 * @return 0 on sucess, -1 on failure
 */
int Network::register_fd(int fd, Network::Direction dir, FDListener *listener, FDManager *manager ) {
  Listener *list;

  list = new Listener(fd, listener, manager);

  // add to handlers
  if( dir == Network::READ ) {
    m_rhandlers_vect.push_back(list);
  } else {
    m_whandlers_vect.push_back(list);
  }
  Logger::instance()->log(Logger::INFO, "Registered fd %d", fd);

  return 0;

}


/*
 * Unregister a file descriptor
 *
 * @param  fh  the file descriptor to unregister
 * @return 0 on sucess, -1 on failure
 */
int Network::unregister_fd(int fd, Network::Direction dir) {
  vector<Listener*>::iterator it;

  if( dir == Network::READ ) {
    for (it = m_rhandlers_vect.begin(); it != m_rhandlers_vect.end(); ++it) {
      if((*it)->m_fd == fd) {
        delete *it;
        m_rhandlers_vect.erase(it);
        Logger::instance()->log(Logger::INFO, "Unregistered fd %d", fd);
        break;
      }
    }
  } else {
    for (it = m_whandlers_vect.begin(); it != m_whandlers_vect.end(); ++it) {
      if((*it)->m_fd == fd) {
        delete *it;
        m_rhandlers_vect.erase(it);
        Logger::instance()->log(Logger::INFO, "Unregistered fd %d!!\n", fd);
        break;
      }
    }

  }
  return 0;
}


/*
 * Register a timeout function
 *
 * @param seconds    the delay between function calls
 */
int Network::register_timeout(int ms, TimeoutListener *listener, bool recur,
                              bool free) {
  event_t event;
  event.listener = listener;
  event.interval = recur ? ms : 0;
  event.free = free;

  gettimeofday(&event.next, NULL);
  event.next.tv_sec += ms / 1000;
  event.next.tv_usec += 1000 * (ms % 1000);

  m_event_cbs.push(event);
  printf("registed %d\n", ms);
  return 1;
}

#define max(a,b) a>b?a:b
/*
 *
 *
 * @return -1 on error, 0 on timeout or interrupt, else the number of bytes read
 */
int Network::read(lla_msg *msg) {
  int maxsd, ret;
  unsigned int i;
  fd_set r_fds, w_fds;
  struct timeval tv;
  struct timeval now;

  while(1) {
    FD_ZERO(&r_fds);
    FD_ZERO(&w_fds);

    FD_SET(m_sd, &r_fds);

    maxsd = m_sd;

    for(i=0; i < m_rhandlers_vect.size(); i++) {
      FD_SET(m_rhandlers_vect[i]->m_fd, &r_fds);
      maxsd = max(maxsd, m_rhandlers_vect[i]->m_fd);
    }

    for(i=0; i < m_whandlers_vect.size(); i++) {
      FD_SET(m_whandlers_vect[i]->m_fd, &r_fds);
      maxsd = max(maxsd, m_whandlers_vect[i]->m_fd);
    }

    now = check_timeouts();

    if (m_event_cbs.empty()) {
      tv.tv_sec = 1;
      tv.tv_usec = 0;
    } else {
      struct timeval next = m_event_cbs.top().next;
      long long now_l = (long long) now.tv_sec * 1000000 + now.tv_usec;
      long long next_l = (long long) next.tv_sec * 1000000 + next.tv_usec;
      long rem = next_l - now_l;
      tv.tv_sec = rem / 1000000;
      tv.tv_usec = rem % 1000000;
    }

    switch( select(maxsd+1, &r_fds, &w_fds, NULL, &tv)) {
      case 0:
        // timeout
        return 0;
        break;
      case -1:
        if( errno == EINTR) {
          return 0;
        }
        Logger::instance()->log(Logger::WARN, "select error: %s", strerror(errno));
        return -1;
        break;
      default:
        check_timeouts();

        // loop thru registered sd's
        for(i=0; i < m_rhandlers_vect.size(); i++) {
          if(FD_ISSET(m_rhandlers_vect[i]->m_fd,&r_fds) ) {
            ret = m_rhandlers_vect[i]->m_listener->fd_action();

            if( ret < 0) {
              m_rhandlers_vect[i]->m_manager->fd_error(ret, m_rhandlers_vect[i]->m_listener );
            }
          }
        }

        for(i=0; i < m_whandlers_vect.size(); i++) {
          if(FD_ISSET(m_whandlers_vect[i]->m_fd,&r_fds) ) {
            ret = m_whandlers_vect[i]->m_listener->fd_action();

            if( ret < 0) {
              m_whandlers_vect[i]->m_manager->fd_error(ret, m_whandlers_vect[i]->m_listener );
            }
          }
        }

        // check for msgs from clients
        if(FD_ISSET(m_sd, &r_fds)) {
          // got a msg from a client
          return fetch_msg_from_client(msg);
        }
        break;
    }
  }
  return 0;
}



/*
 * Called when a client sends a msg
 *
 * @param   sd  the socket descriptor
 * @return  number of bytes read, or 0 on error
 */
int Network::fetch_msg_from_client(lla_msg *msg) {
  socklen_t clilen = sizeof(msg->from);
  int status = recvfrom(m_sd, &msg->data, sizeof(lla_msg_data) ,0, (struct sockaddr *) &msg->from,  &clilen);

  if(status < 0 ) {
    if( errno != EINTR) {
      Logger::instance()->log(Logger::DEBUG, "Error reading %s" , strerror(errno));
      return -1;
    }
  } else if(status > 0) {
    msg->len = status;
    Logger::instance()->log(Logger::DEBUG, "Recv msg from client on port %d" , ntohs(msg->from.sin_port) );
  }
  Logger::instance()->log(Logger::DEBUG, "Recv msg from client on port %d" , ntohs(msg->from.sin_port) );

  return status;
}


/*
 * send a datagram to a client
 *
 *
 */
int Network::send_msg(lla_msg *msg) {

  int ret = sendto(m_sd, &msg->data, msg->len, 0, (struct sockaddr *) &msg->to, sizeof(msg->to));

  if ( -1 == ret ) {
    Logger::instance()->log(Logger::CRIT, "Sendto failed: %s", strerror(errno) );
    return -1;
  } else if (msg->len != ret ) {
    Logger::instance()->log(Logger::CRIT, "Failed to send full datagram");
    return -1;
  }

  return 0;
}


/*
 * Check for expired timeouts
 */
struct timeval Network::check_timeouts() {
  struct timeval now;
  gettimeofday(&now, NULL);

  event_t e;
  if (m_event_cbs.empty())
    return now;

  for (e = m_event_cbs.top(); !m_event_cbs.empty() && timercmp(&e.next, &now, <); e = m_event_cbs.top()) {
    if (e.listener != NULL)
      e.listener->timeout_action();
    m_event_cbs.pop();

    if(e.interval) {
      e.next = now;
      e.next.tv_sec += e.interval / 1000;
      e.next.tv_usec += e.interval / 1000;
      m_event_cbs.push(e);
    }

    if(!e.interval && e.free)
      delete e.listener;
    gettimeofday(&now, NULL);
  }
  return now;
}
