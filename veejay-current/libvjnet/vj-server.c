/* libvjnet - Linux VeeJay
 * 	     (C) 2002-2005 Niels Elburg <nelburg@looze.net> 
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <libvjmsg/vj-common.h>
#include <libvjnet/vj-server.h>
#include <string.h>


#define __INVALID 0
#define __SENDER 1
#define __RECEIVER 2

typedef struct
{
	char *msg;
	int   len;
} vj_message;

typedef struct {
	int handle;
	int in_use;
	vj_message **m_queue;
	int n_queued;
	int n_retrieved;
} vj_link;

typedef struct
{
	union
	{
		mcast_receiver  *r;
		mcast_sender	*s;
	};
	int type;
} vj_proto;

#define VJ_MAX_PENDING_MSG 64
#define RECV_SIZE (16384)

int		_vj_server_free_slot(vj_server *vje);
int		_vj_server_new_client(vj_server *vje, int socket_fd);
int		_vj_server_del_client(vj_server *vje, int link_id);
int		_vj_server_parse_msg(vj_server *vje,int link_id, char *buf, int buf_len, int priority );


static	int	_vj_server_multicast( vj_server *v, char *group_name, int port )
{
	vj_link **link;
	int i;

	vj_proto	**proto  = (vj_proto**) malloc(sizeof( vj_proto* ) * 2);
	
	for( i = 0; i < 2; i ++ )
		proto[i] = (vj_proto*) malloc(sizeof( vj_proto ) );

	if( v->server_type == V_CMD )
	{
		proto[0]->s = mcast_new_sender( group_name );
		if(!proto[0]->s)
		{
			veejay_msg(VEEJAY_MSG_ERROR, "cannot setup cmd sender (%s:%d)", group_name, port);
			return 0;
		}
		proto[1]->r = mcast_new_receiver( group_name, port + 3 );
		if(!proto[1]->r)
		{
			veejay_msg(VEEJAY_MSG_ERROR, "cannot setup cmd receiver (%s:%d)", group_name, port + 3 );
			return 0;
		}
		proto[0]->type = __SENDER;
		proto[1]->type = __RECEIVER; 
		v->ports[0] = port;
		v->ports[1] = port + 3;
		veejay_msg(VEEJAY_MSG_DEBUG, "Multicast command server OUT %d, IN %d", port, port+3 );
	}

	if( v->server_type == V_STATUS )
	{
		proto[0]->s = mcast_new_sender( group_name );
		if(!proto[0]->s)
		{
			veejay_msg(VEEJAY_MSG_ERROR, "cannot setup status sender (%s:%d)", group_name, port );
			return 0;
		}
		proto[0]->type = __SENDER;
		proto[1]->type = __INVALID; // no recv on status port !
		v->ports[0] = port;
		v->ports[1] = 0;
	}
	
	v->protocol = (void**) proto;

	link = (vj_link **) vj_malloc(sizeof(vj_link *) * VJ_MAX_CONNECTIONS);

	if(!link)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Out of memory");
		return 0;
	}

	for( i = 0; i < 1; i ++ ) // only 1 link needed for multicast
	{
		int j;
		link[i] = (vj_link*) vj_malloc(sizeof(vj_link));
		if(!link[i])
		{
			return 0;
		}
		link[i]->in_use = 1;
		link[i]->m_queue = (vj_message**) vj_malloc(sizeof( vj_message * ) * VJ_MAX_PENDING_MSG );
		if(!link[i]->m_queue)
		{
			return 0;
		}
		for( j = 0; j < VJ_MAX_PENDING_MSG; j ++ )
		{
			link[i]->m_queue[j] = (vj_message*) vj_malloc(sizeof(vj_message));
			link[i]->m_queue[j]->len = 0;
			link[i]->m_queue[j]->msg = NULL;
		}
		link[i]->n_queued = 0;
		link[i]->n_retrieved = 0;
	}
	v->link = (void**) link;
	v->nr_of_links = 1;

	return 1;
}

static int	_vj_server_classic(vj_server *vjs, int port_num)
{
	int on = 1;
	
	vj_link **link;
	int i = 0;
	if ((vjs->handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "%s", strerror(errno));
		return 0;
    }
    if (setsockopt( vjs->handle, SOL_SOCKET, SO_REUSEADDR, (const char*) &on, sizeof(on) )== -1)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "%s", strerror(errno));
		return 0;
    }
    vjs->myself.sin_family = AF_INET;
    vjs->myself.sin_addr.s_addr = INADDR_ANY;
    vjs->myself.sin_port = htons(port_num);
    memset(&(vjs->myself.sin_zero), 0, 8);
    if (bind(vjs->handle, (struct sockaddr *) &(vjs->myself), sizeof(vjs->myself) ) == -1 )
	{
		veejay_msg(VEEJAY_MSG_ERROR, "%s", strerror(errno));
		return 0;
    }
    if (listen(vjs->handle, VJ_MAX_CONNECTIONS) == -1)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "%s", strerror(errno));
		return 0;
    }

	link = (vj_link **) vj_malloc(sizeof(vj_link *) * VJ_MAX_CONNECTIONS);
	if(!link)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Out of memory");
		return 0;
	}

	for( i = 0; i < VJ_MAX_CONNECTIONS; i ++ )
	{
		int j;
		link[i] = (vj_link*) vj_malloc(sizeof(vj_link));
		if(!link[i])
		{
			return 0;
		}
		link[i]->in_use = 0;
		link[i]->m_queue = (vj_message**) vj_malloc(sizeof( vj_message * ) * VJ_MAX_PENDING_MSG );
		if(!link[i]->m_queue)
		{
			return 0;
		}
		for( j = 0; j < VJ_MAX_PENDING_MSG; j ++ )
		{
			link[i]->m_queue[j] = (vj_message*) vj_malloc(sizeof(vj_message));
			link[i]->m_queue[j]->len = 0;
			link[i]->m_queue[j]->msg = NULL;
		}
		link[i]->n_queued = 0;
		link[i]->n_retrieved = 0;		
	}
	vjs->link = (void**) link;
	vjs->nr_of_links = 0;
	vjs->nr_of_connections = vjs->handle;
	return 1;
}
vj_server *vj_server_alloc(int port, char *mcast_group_name, int type)
{
    vj_server *vjs = (vj_server *) vj_malloc(sizeof(struct vj_server_t));
	
    if (!vjs)
		return NULL;

	memset( vjs, 0, sizeof(vjs) );

	vjs->recv_buf = (char*) malloc(sizeof(char) * 16384 );
	if(!vjs->recv_buf)
	{
		if(vjs) free(vjs);
		return NULL;
	}	
	bzero( vjs->recv_buf, 16384 );

	vjs->ports[type] = port;
	vjs->server_type = type;

	/* setup peer to peer socket */
	if( mcast_group_name == NULL )
	{
		vjs->use_mcast = 0;
		if ( _vj_server_classic( vjs,port ) )
			return vjs;
	}
    else
	{	/* setup multicast socket */
		vjs->use_mcast = 1;
		veejay_msg(VEEJAY_MSG_DEBUG, "Setup multicast server, port offset %d, group %s", port, mcast_group_name);
		if ( _vj_server_multicast(vjs, mcast_group_name, port) )
			return vjs;
	}
	  
	if(vjs)
		free(vjs);

    return NULL;
}


int vj_server_send( vj_server *vje, int link_id, uint8_t *buf, int len )
{
    unsigned int total = 0;
    unsigned int bytes_left = len;
    int n;

	if(len <= 0 || buf == NULL)
		return 0;

	if( !vje->use_mcast)
	{
		vj_link **Link = (vj_link**) vje->link;
		if (len <= 0 || Link[link_id]->in_use==0)
		{
			return 0;
		}

		if(!FD_ISSET( Link[link_id]->handle, &(vje->wds) ) )
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Socket not ready not sending");
			return 0;
		}

		while (total < len)
		{
			n = send(Link[link_id]->handle, buf + total, bytes_left, 0);
			if (n <= 0)
			{
				if(n == -1) 
					veejay_msg(VEEJAY_MSG_ERROR, "%s", strerror(errno));
		   		return 0;
			}
			total += n;
			bytes_left -= n;
   		}
	}
	else
	{
		vj_proto **proto = (vj_proto**) vje->protocol;
		if( vje->server_type == V_STATUS )
			return mcast_send( proto[0]->s, buf, bytes_left, vje->ports[0] );

		if( vje->server_type == V_CMD )
			return mcast_send( proto[0]->s, buf, bytes_left, vje->ports[0] );
/*		if( vje->server_type == V_CMD && len > 512 )
		{	
			VJFrame frame;
			frame.data[0] = buf;
			frame.len = 352 * 288;
			return mcast_send_frame( proto[0]->s, &frame, vje->ports[0]);
		}	*/
	}
  
 
    return total;
}

int		vj_server_send_frame( vj_server *vje, int link_id, uint8_t *buf, int len, 
					VJFrame *frame, VJFrameInfo *info, long ms )
{
	if(len <= 0 || buf == NULL )
		return 0;
	if(!vje->use_mcast )
	{
		return vj_server_send( vje, link_id, buf, len );
	}
	else
	{
		vj_proto **proto = (vj_proto**) vje->protocol;
		if(vje->server_type == V_STATUS )
			return 0;
		if( vje->server_type == V_CMD  )
		{
			return mcast_send_frame( proto[0]->s, frame,info, buf,len,ms, vje->ports[0] );
		}
	}
	return 0;
}

int _vj_server_free_slot(vj_server *vje)
{
	vj_link **Link = (vj_link**) vje->link;
    int i;
	for (i = 0; i < VJ_MAX_CONNECTIONS; i++)
	{
	    if (Link[i]->in_use == 0)
			return i;
    }
    return VJ_MAX_CONNECTIONS;
}

int _vj_server_new_client(vj_server *vje, int socket_fd)
{
    int entry = _vj_server_free_slot(vje);
	vj_link **Link = (vj_link**) vje->link;
    if (entry == VJ_MAX_CONNECTIONS)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Cannot take more connections (max %d allowed)", VJ_MAX_CONNECTIONS);
		return VJ_MAX_CONNECTIONS;
	}
    Link[entry]->handle = socket_fd;
    Link[entry]->in_use = 1;
    FD_SET( socket_fd, &(vje->fds) );
	FD_SET( socket_fd, &(vje->wds) );
	vje->nr_of_links ++;
    return entry;
}

int _vj_server_del_client(vj_server * vje, int link_id)
{
	vj_link **Link = (vj_link**) vje->link;
	if(Link[link_id]->in_use )
	{
		Link[link_id]->in_use = 0;
		FD_CLR( Link[link_id]->handle, &(vje->fds) );
		FD_CLR( Link[link_id]->handle, &(vje->wds) );
		close(Link[link_id]->handle);
		vje->nr_of_links --;
		return 1;
		
    	}
    return 0;
}

void	vj_server_close_connection(vj_server *vje, int link_id )
{
	if(vje->use_mcast)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "\n*\n*\n* test me");
		vj_proto **proto = (vj_proto**) vje->protocol;
		if( proto[0]->type == __SENDER )    
			mcast_close_sender( proto[0]->s );
		if( proto[0]->type == __RECEIVER )
			mcast_close_receiver( proto[0]->r );
		if( proto[1]->type == __SENDER )
			mcast_close_sender( proto[1]->s );
		if( proto[1]->type == __RECEIVER )
			mcast_close_receiver( proto[1]->r );
	}
	_vj_server_del_client( vje, link_id );
}

int vj_server_poll(vj_server * vje)
{
	int status = 0;
	struct timeval t;
	int i;
	if(vje->use_mcast)	// no polling for mcast here ! (see recv)
		return 0 ;

	memset( &t, 0, sizeof(t));

	FD_ZERO( &(vje->fds) );
    	FD_ZERO( &(vje->wds) );

	FD_SET( vje->handle, &(vje->fds) );
	if(vje->server_type == V_STATUS)
		FD_SET( vje->handle, &(vje->wds) );

	for( i = 0; i < vje->nr_of_links ; i ++ )
	{
		vj_link **Link= (vj_link**) vje->link; 
		if(vje->server_type == V_CMD )
		{
			FD_SET( Link[i]->handle, &(vje->fds) );
			FD_SET( Link[i]->handle, &(vje->wds) );	
		}
		if(vje->server_type == V_STATUS )
			FD_SET( Link[i]->handle, &(vje->wds));
	}
 
	status = select(vje->nr_of_connections + 1, &(vje->fds), &(vje->wds), 0, &t);



	if( status > 0 )
	{
		return 1;
	}

	return 0;
}

static	int	_vj_server_empty_queue(vj_server *vje, int link_id)
{
	// ensure message queue is empty!!
	vj_link **Link = (vj_link**) vje->link;
	vj_message **v = Link[link_id]->m_queue;

	int i;
	for( i = 0; i < VJ_MAX_PENDING_MSG; i ++ )
	{
		if( v[i]->msg )
			free(v[i]->msg);
		v[i]->msg = NULL;
		v[i]->len = 0;
	}
	Link[link_id]->n_queued = 0;
	Link[link_id]->n_retrieved = 0;
	return 1;
}

#define _vj_malfunction(msg,content,buflen, index)\
{ \
if(msg != NULL)\
veejay_msg(VEEJAY_MSG_DEBUG,"%s",msg);\
veejay_msg(VEEJAY_MSG_DEBUG,"Message content");\
veejay_msg(VEEJAY_MSG_DEBUG,"msg [%s] (%d bytes) at position %d",\
 content, buflen, index);\
int _foobar;\
for ( _foobar = 0; _foobar < (index+4); _foobar++);\
veejay_msg(VEEJAY_MSG_PRINT, " ");\
veejay_msg(VEEJAY_MSG_PRINT, "^");\
veejay_msg(VEEJAY_MSG_DEBUG, "- dropped message");\
}
static  int	_vj_verify_msg(vj_server *vje,int link_id, char *buf, int buf_len )
{
	int i = 0;
	char *s = buf;
	int num_msg = 0;
	vj_link **Link = (vj_link**) vje->link;

	if( (i+4) > buf_len )
	  _vj_malfunction("Message too small", buf, buf_len, i );

	while( (i+4) < buf_len )
	{
		if( !(s[i]=='V' && s[i+4] == 'D') )
		{
			_vj_malfunction( "Cannot identify message as VIMS message",
					buf,buf_len,i );
			return 0;
		}

		char tmp_len[4];
		char net_id[4];
		int  slen = 0;
		int	netid = 0;
		bzero(tmp_len,4);
		bzero(net_id,4 );

		char *str_ptr = &s[i];
		str_ptr ++;  // skip 'V'

		strncpy( tmp_len,str_ptr, 3 ); // header length   

		if( sscanf(tmp_len, "%03d", &slen) <= 0 )
		{
			_vj_malfunction( "Cannot read header length", buf, buf_len, i + 1);
			return 0;
		}
		if( slen > buf_len )
		{
			char msg[256];
			bzero(msg,256);
			snprintf(msg, 256,"Remote %s is sending corrupted packets", (char*) (inet_ntoa( vje->remote.sin_addr ) ) );
			_vj_malfunction( NULL, buf, buf_len, i + 1 );
			vj_server_close_connection( vje, link_id );
			return 0;
		}
	

		if ( slen > 999 )
		{
			char msg[256];
			bzero(msg,256);
			snprintf(msg, 256, "Remote %s is acting very suspiciously", (char* )( inet_ntoa( vje->remote.sin_addr ) ));
			_vj_malfunction( msg, buf, buf_len, i + 1);  
			vj_server_close_connection( vje, link_id );
			return 0;
		}

		i += 5; // advance to message content
		str_ptr += 5;
		strncpy( net_id, str_ptr, 3 );

		if( sscanf( net_id, "%03d", &netid ) <= 0 )
		{
			_vj_malfunction( "Corrupt VIMS selector", buf, buf_len, i );
			return 0;
		}
		if( netid < 0 && netid > 600 )
		{
			_vj_malfunction( "VIMS selector out of range", buf,buf_len, i );
			return 0;
		}

		num_msg ++; 
		i += slen; // try next message
	}
	if(num_msg > 0 )
		return num_msg;
	return 0;
}

static  int	_vj_parse_msg(vj_server *vje,int link_id, char *buf, int buf_len, int priority )
{
	int i = 0;
	char *s = buf;
	int num_msg = 0;
	vj_link **Link = (vj_link**) vje->link;
	vj_message **v = Link[link_id]->m_queue;

	while( (i+4) < buf_len )
	{
		char tmp_len[4];
		char net_id[4];
		int  slen = 0;
		int	netid = 0;
		bzero(tmp_len,4);
		bzero(net_id,4 );

		char *str_ptr = &s[i];
		str_ptr ++;  // skip 'V'

		strncpy( tmp_len,str_ptr, 3 ); // header length   

		if( sscanf(tmp_len, "%03d", &slen) <= 0 )
		{
			return 0;
		}
		i += 4; // advance to message content
		str_ptr += 4;
		strncpy( net_id, str_ptr, 3 );


		if( sscanf( net_id, "%03d", &netid ) <= 0 )
		{
			return 0;
		}
		if(! priority )
		{
			// store message anyway
			v[num_msg]->len = slen;
			v[num_msg]->msg = (char*)strndup( str_ptr , slen );
			num_msg++;
			veejay_msg(VEEJAY_MSG_DEBUG,
				 "Stored msg %d [%s]", num_msg, v[num_msg-1]->msg);
		}
		if(priority && netid > 255 )
		{
			// store high priority only (reduce load) -         
			v[num_msg]->len = slen;
			v[num_msg]->msg = (char*)strndup( str_ptr , slen );
			num_msg++;
		}

		if(num_msg == VJ_MAX_PENDING_MSG )
		{
			veejay_msg(VEEJAY_MSG_DEBUG, "VIMS server queue full - max messages per interval is %d",
				VJ_MAX_PENDING_MSG );	
			   return num_msg; // cant take more
		}
				
		i += slen; // try next message
	}

	if( ! priority )
	{
		veejay_msg(VEEJAY_MSG_DEBUG, "Queued %d messages from link %d", num_msg,link_id);
		Link[link_id]->n_queued = num_msg;
		Link[link_id]->n_retrieved = 0;
	}

	return num_msg;
}


static	int	_vj_get_n_msg_waiting(char *buf, int buf_len, int *offset )
{
	int nmsgs = 0;
	int i = 0;
	char *s = buf;
	while( i < buf_len )
	{
		if( s[i] == 'V' && s[i+4] == 'D' )
		{ // found message
			char tmp_len[4];
			int  slen = 0;
			bzero(tmp_len,4);
			i++;
			char *str_ptr = &s[i];
			strncpy( tmp_len, str_ptr, 3 );
			if(sscanf(tmp_len,"%03d", &slen)<= 0)
				return nmsgs;
			nmsgs++;
			if(slen > 0)
				i += slen;
			if( nmsgs >= VJ_MAX_PENDING_MSG )
			{
				*offset = i;
				return VJ_MAX_PENDING_MSG;
			}
		}
		else
		{
			return nmsgs;
		}
	}
	return nmsgs;
}


int	vj_server_new_connection(vj_server *vje)
{
	if( vje->use_mcast )
		return -1;

	if( FD_ISSET( vje->handle, &(vje->fds) ) )
	{
		int addr_len = sizeof(vje->remote);
		int n = 0;
		int fd = accept( vje->handle, (struct sockaddr*) &(vje->remote), &addr_len );
		if(fd == -1)
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Error accepting connection");
			return 0;
		}	

		char *host = inet_ntoa( vje->remote.sin_addr ); 
		veejay_msg(VEEJAY_MSG_DEBUG, "Connection with %s", host);		
			

		if( vje->nr_of_connections < fd ) vje->nr_of_connections = fd;

		//fcntl( fd, F_SETFL, O_NONBLOCK );
		n = _vj_server_new_client(vje, fd); 
		if( n == VJ_MAX_CONNECTIONS )
		{
			close(fd);
			return 0;
		}
		return 1;
	}
	return 0;
}

int	vj_server_update( vj_server *vje, int id )
{
	int sock_fd = vje->handle;
	int n = 0;

	if( !vje->use_mcast)
	{
 		if(!vj_server_poll(vje))
			return 0;

		vj_link **Link = (vj_link**) vje->link;
		sock_fd = Link[id]->handle;

		if(!FD_ISSET( sock_fd, &(vje->fds)) )
			return 0;

		// clear recv_buf
		bzero( vje->recv_buf, 16384);
		n = recv( sock_fd, vje->recv_buf, RECV_SIZE, 0 );
	}
	else
	{
		// command socket ?
		if(vje->server_type == V_CMD )
		{
			vj_proto **proto = (vj_proto**) vje->protocol;
			// get data from command socket
			if( proto[1]->type == __RECEIVER )
			{
				if( mcast_poll( proto[1]->r ))
				{
					bzero( vje->recv_buf, RECV_SIZE );
					n = mcast_recv( proto[1]->r , vje->recv_buf, RECV_SIZE );  
				}
			}
		}

	}

	if(!vje->use_mcast)
	{
		if( n <= 0)
		{
			_vj_server_del_client( vje, id );
			return -1;
		}
	}

	// ensure all is empty
	_vj_server_empty_queue(vje, id);
	char *msg_buf = vje->recv_buf;
	int bytes_left = n;

	int n_msg = _vj_verify_msg( vje, id, msg_buf, bytes_left ); 
	if( n_msg < VJ_MAX_PENDING_MSG )
	{
		veejay_msg(VEEJAY_MSG_DEBUG, "(VIMS) received %d bytes and %d from remote", bytes_left, n_msg);
		return _vj_parse_msg( vje, id, msg_buf, bytes_left,0 );
	}
	else
	{
		// 
		int skip_bytes = 0;
		int num_msg_left = _vj_get_n_msg_waiting( msg_buf, bytes_left, &skip_bytes ); // returns position to first pri msg
		veejay_msg(VEEJAY_MSG_WARNING, "*** HIGH LOAD *** : Remote '%s' is sending messages at a very high rate",
		 	(char*) ( inet_ntoa( vje->remote.sin_addr ) ) );
		veejay_msg(VEEJAY_MSG_WARNING, "*** HIGH LOAD *** : Or veejay is running on an extremely low frame rate" );
		veejay_msg(VEEJAY_MSG_WARNING, "*** HIGH LOAD *** : Filtering out most relevant VIMS selectors" );
		int done = 0;
		int result = 0;
		while(bytes_left > 0 )
		{
			int left = _vj_get_n_msg_waiting( msg_buf, bytes_left, &skip_bytes ); // returns position to first pri msg
			veejay_msg(VEEJAY_MSG_DEBUG, "*** HIGH LOAD *** : filtering out high priority VIMS selectors ( > 255 ) %d - %d",
				done, left);
			int chunk_len = bytes_left - skip_bytes;
			if( done < VJ_MAX_PENDING_MSG )
			{
				veejay_msg(VEEJAY_MSG_DEBUG, "*** HIGH LOAD *** : accepting last arriving messages (%d)",
					done );
				result += _vj_parse_msg( vje, id, msg_buf + skip_bytes, chunk_len, 0 );
			}
			else
			{
				result += _vj_parse_msg( vje, id, msg_buf + skip_bytes, chunk_len, 1 );
			}
			done += left;
			bytes_left -= chunk_len;
		}
		return result;
	}  

	return 0;
}
void vj_server_shutdown(vj_server *vje)
{
	int j,i;
	vj_link **Link = (vj_link**) vje->link;
	int k = VJ_MAX_CONNECTIONS;

	if(vje->use_mcast) k = 1;
    
	for(i=0; i < k; i++)
	{
		if(Link[i]->in_use) 
			close(Link[i]->handle);
		for( j = 0; j < VJ_MAX_PENDING_MSG; j ++ )
		{
			if(Link[i]->m_queue[j]->msg ) free( Link[i]->m_queue[j]->msg );
			if(Link[i]->m_queue[j] ) free( Link[i]->m_queue[j] );
		}
		if( Link[i]->m_queue ) free( Link[i]->m_queue );
		if( Link[i] ) free( Link[i] );
    }

	if(!vje->use_mcast)
	    close(vje->handle);
	else
	{
		vj_proto **proto = (vj_proto**) vje->protocol;
		if(proto[0]->type == __SENDER)
			mcast_close_sender( proto[0]->s );
		if(proto[1]->type == __RECEIVER)
			mcast_close_receiver( proto[1]->r );
		if(proto[0])
			free(proto[0]);
		if(proto[1])
			free(proto[1]);
		if(proto) free(proto);
	}

	if( vje->recv_buf )
		free(vje->recv_buf);

	free(Link);
	if(vje) free(vje);
}

int vj_server_retrieve_msg(vj_server *vje, int id, char *dst )
{
	vj_link **Link = (vj_link**) vje->link;
   	if (Link[id]->in_use == 0)
		return 0;

	int index = Link[id]->n_retrieved;
	char *msg;
	int   len;

	if( index == Link[id]->n_queued )
	{
		veejay_msg(VEEJAY_MSG_DEBUG, "\tRetrieved %d messages from queue",
			index);
		return 0; // done
	}
	msg = Link[id]->m_queue[index]->msg;
	len = Link[id]->m_queue[index]->len;

	strncpy( dst, msg, len );

	index ++;

	veejay_msg(VEEJAY_MSG_DEBUG, "\tExec message %d [%s]",
		index-1, msg );

	Link[id]->n_retrieved = index;
    return 1;			
}

