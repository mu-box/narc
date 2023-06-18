// -*- mode: c; tab-width: 8; indent-tabs-mode: 1; st-rulers: [70] -*-

#ifndef NARC_UDP
#define NARC_UDP

#include "narc.h"
#include "sds.h"	/* dynamic safe strings */

#include <uv.h>		/* Event driven programming library */

/* connection states */
#define NARC_UDP_INITIALIZED	0
#define NARC_UDP_BOUND			1
/*-----------------------------------------------------------------------------
 * Data types
 *----------------------------------------------------------------------------*/

typedef struct {
	int 		state;		/* connection state */
	uv_udp_t 	socket;	/* udp socket */
	uv_getaddrinfo_t resolver;
	struct sockaddr_in send_addr;
} narc_udp_client;

/*-----------------------------------------------------------------------------
 * Functions prototypes
 *----------------------------------------------------------------------------*/

/* watchers */

/* api */
void	init_udp_client(void);
void	clean_udp_client(void);
void 	submit_udp_message(char *message);

#endif
