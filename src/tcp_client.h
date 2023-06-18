// -*- mode: c; tab-width: 8; indent-tabs-mode: 1; st-rulers: [70] -*-

#ifndef NARC_TCP
#define NARC_TCP

#include "narc.h"
#include "sds.h"	/* dynamic safe strings */

#include <uv.h>		/* Event driven programming library */

/* connection states */
#define NARC_TCP_INITIALIZED	0
#define NARC_TCP_ESTABLISHED	1

/*-----------------------------------------------------------------------------
 * Data types
 *----------------------------------------------------------------------------*/

typedef struct {
	int 		state;		/* connection state */
	uv_tcp_t 	*socket;	/* tcp socket */
	uv_stream_t	*stream;	/* connection stream */
	int 		attempts;	/* connection attempts */
	uv_getaddrinfo_t resolver;
} narc_tcp_client;

/*-----------------------------------------------------------------------------
 * Functions prototypes
 *----------------------------------------------------------------------------*/

/* watchers */
void	start_resolve(void);
void	start_tcp_connect(struct addrinfo *res);
void	start_tcp_read(uv_stream_t *stream);

/* api */
void	init_tcp_client(void);
void	clean_tcp_client(void);
void 	submit_tcp_message(char *message);
void	start_tcp_connect_timer(void);

#endif
