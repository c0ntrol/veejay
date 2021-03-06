/* vjnet - low level network I/O for VeeJay
 *
 *           (C) 2005-2007 Niels Elburg <nwelburg@gmail.com> 
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <config.h>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <libavutil/pixfmt.h>
#include <libvjmem/vjmem.h>
#include <libvjmsg/vj-msg.h>
#include <veejaycore/veejaycore.h>
#include "packet.h"
#include "mcastsender.h"

static void print_error(char *msg)
{
	veejay_msg(VEEJAY_MSG_ERROR, "%s: %s", msg,strerror(errno));
}

mcast_sender	*mcast_new_sender( const char *group_name )
{
	int on = 1;
	uint8_t ttl = 1;
	mcast_sender *v = (mcast_sender*) malloc(sizeof(mcast_sender));
	if(!v) return NULL;
	v->group	= (char*)strdup( group_name );
	v->addr.sin_addr.s_addr = inet_addr( v->group );
	v->addr.sin_port 	= htons( 0 );
	v->addr_len = sizeof( struct sockaddr_in );	
	v->sock_fd = socket( AF_INET, SOCK_DGRAM, 0 );
	v->stamp = 1;
	if( v->sock_fd == -1 )
	{
		print_error( "socket");
		if(v) free(v);
		return NULL;
	}

#ifdef SO_REUSEADDR
	if( setsockopt( v->sock_fd, SOL_SOCKET, SO_REUSEADDR, &on,sizeof(on))<0 )
	{
		print_error("SO_REUSEADDR");
		if(v) free(v);
		return NULL;
	}
#endif
#ifdef SO_REUSEPORT
	if( setsockopt( v->sock_fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on)) < 0 )
	{
		print_error("SO_REUSEPORT");
		if(v) free(v);
		return NULL;
	}
#endif
	if( setsockopt( v->sock_fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0 )
	{
		print_error("IP_MULTICAST_TTL");
		if(v) free(v);
		return NULL;
	}

	char *eth = getenv("VEEJAY_MCAST_INTERFACE");
	if( eth != NULL ) {
		mcast_set_interface(v, eth );
	}
	return v;
}	

void		mcast_set_interface( mcast_sender *v, const char *interface )
{
	struct sockaddr_in	if_addr;
	memset( &if_addr, 0, sizeof(if_addr) );
	
	v->addr.sin_addr.s_addr = inet_addr( interface );
	v->addr.sin_family = AF_INET;

	if( setsockopt( v->sock_fd, IPPROTO_IP, IP_MULTICAST_IF, &if_addr, sizeof(if_addr) ) < 0 )
		print_error("IP_MULTICAST_IF");
	else
		veejay_msg(VEEJAY_MSG_INFO, "Multicast interface set to %s", interface );
}

int		mcast_send( mcast_sender *v, const void *buf, int len, int port_num )
{
	v->addr.sin_port = htons( port_num );
	v->addr.sin_family = AF_INET;

	return sendto( v->sock_fd, buf, len, 0, (struct sockaddr*) &(v->addr), v->addr_len );
} 

static long stamp_make( mcast_sender *v )
{
	v->stamp ++;
	return v->stamp;
}

int		mcast_send_frame( mcast_sender *v, const VJFrame *frame,  
				uint8_t *buf, int total_len, int port_num, int mode)
{
	int i;
	int res = 0;
	uint8_t chunk[PACKET_PAYLOAD_SIZE];
	long frame_num = stamp_make(v);

	packet_header_t header = packet_construct_header( 1 );
	
	header.usec    = frame_num;

	veejay_memset( chunk, 0,sizeof(chunk));

	//@ If we can send in a single packet:
	if( total_len <= CHUNK_SIZE )
	{
		header.seq_num = 0; header.length = 1; header.data_size = total_len;
		packet_put_padded_data( &header,chunk, buf, total_len);
		res = mcast_send( v, chunk, PACKET_PAYLOAD_SIZE, port_num );
		if(res <= 0 )
			return -1;
		return 1;
	}

	int pred_chunks = (total_len / CHUNK_SIZE);
	int bytes_left  = (total_len % CHUNK_SIZE);

	header.length = pred_chunks + ( bytes_left > 0 ? 1 : 0 );

	for( i = 0; i < pred_chunks; i ++ )
	{
		const uint8_t *data = buf + (i * CHUNK_SIZE);
		header.seq_num = i;
		header.data_size = CHUNK_SIZE;

		packet_put_data( &header, chunk, data );

		res = mcast_send( v, chunk, PACKET_PAYLOAD_SIZE, port_num );
		if(res <= 0 )
		{
			return -1;
		}
	}

	if( bytes_left )
	{
		i = header.length - 1;
		header.seq_num = i;
		header.data_size = packet_put_padded_data( &header, chunk, buf + (i * CHUNK_SIZE), bytes_left );
		veejay_memset( chunk + header.data_size, 0, (PACKET_PAYLOAD_SIZE-header.data_size));
		res = mcast_send( v, chunk, PACKET_PAYLOAD_SIZE, port_num );
		if( res <= 0 )
		{
			return -1;
		}
	}

	return 1;
}

void		mcast_close_sender( mcast_sender *v )
{
	if(v)
	{
		close(v->sock_fd);
		if(v->group) free(v->group);
		v->group = NULL;
		free(v);
		v = NULL;
	}
}
