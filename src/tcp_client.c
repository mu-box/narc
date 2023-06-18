// -*- mode: c; tab-width: 8; indent-tabs-mode: 1; st-rulers: [70] -*-

#include "narc.h"
#include "tcp_client.h"

#include "sds.h"	/* dynamic safe strings */
// #include "malloc.h"	/* total memory usage aware version of malloc/free */

#include <stdio.h>	/* standard buffered input/output */
#include <stdlib.h>	/* standard library definitions */
#include <unistd.h>	/* standard symbolic constants and types */
#include <uv.h>		/* Event driven programming library */
#include <string.h>	/* string operations */

/*============================ Utility functions ============================ */

void
free_tcp_write_req(uv_write_t *req)
{
	sdsfree((char *)req->data);
	free(req->bufs);
	free(req);
}

narc_tcp_client
*new_tcp_client(void)
{
	narc_tcp_client *client = (narc_tcp_client *)malloc(sizeof(narc_tcp_client));

	client->state    = NARC_TCP_INITIALIZED;
	client->socket   = NULL;
	client->stream   = NULL;
	client->attempts = 0;

	return client;
}

int
tcp_client_established(narc_tcp_client *client)
{
	return (client->state == NARC_TCP_ESTABLISHED);
}

/*=============================== Callbacks ================================= */

void
handle_tcp_connect(uv_connect_t* connection, int status)
{
	narc_tcp_client *client = server.client;

	if (status == -1) {
		uv_close((uv_handle_t *)client->socket, (uv_close_cb)free);
		client->socket = NULL;
		narc_log(NARC_WARNING, "Error connecting to %s:%d (%d/%d)",
			server.host,
			server.port,
			client->attempts,
			server.max_connect_attempts);

		if (client->attempts == server.max_connect_attempts) {
			narc_log(NARC_WARNING, "Reached max connect attempts: %s:%d",
				server.host,
				server.port);
			exit(1);
		} else
			start_tcp_connect_timer();

	} else {
		narc_log(NARC_NOTICE, "Connection established: %s:%d", server.host, server.port);

		client->stream   = (uv_stream_t *)connection->handle;
		client->state    = NARC_TCP_ESTABLISHED;
		client->attempts = 0;

		start_tcp_read(client->stream);
	}
	free(connection);
}

void
handle_tcp_write(uv_write_t* req, int status)
{
	free_tcp_write_req(req);
}

void
handle_tcp_read_alloc_buffer(uv_handle_t *handle, size_t len,  struct uv_buf_t *buf)
{
	buf->base = malloc(len);
	buf->len = len;
}

// uv_buf_t
// handle_tcp_read_alloc_buffer(uv_handle_t* handle, size_t size)
// {
// 	return uv_buf_init(malloc(size), size);
// }

void start_tcp_resolve(void);

void
handle_tcp_read(uv_stream_t* tcp, ssize_t nread, const struct uv_buf_t *buf)
{
	if (nread >= 0)
		narc_log(NARC_WARNING, "server responded unexpectedly: %s", buf->base);

	else {
		narc_log(NARC_WARNING, "Connection dropped: %s:%d, attempting to re-connect",
			server.host,
			server.port);

		narc_tcp_client *client = (narc_tcp_client *)server.client;
		uv_close((uv_handle_t *)client->socket, (uv_close_cb)free);
		client->socket = NULL;
		client->state = NARC_TCP_INITIALIZED;

		start_tcp_connect_timer();
	}
	if (buf->base)
		free(buf->base);
}

void
handle_tcp_connect_timeout(uv_timer_t* timer)
{
	start_tcp_resolve();
	uv_close((uv_handle_t *)timer, (uv_close_cb)free);
}

void
handle_tcp_resolved(uv_getaddrinfo_t *resolver, int status, struct addrinfo *res)
{
	if (status >= 0){
		narc_log(NARC_WARNING, "server resolved: %s", server.host);
		start_tcp_connect(res);
	}else{
		narc_log(NARC_WARNING, "server did not resolve: %s", server.host);
	}
}

/*=============================== Watchers ================================== */

void
start_tcp_resolve(void)
{
	struct addrinfo hints;
	hints.ai_family = PF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = 0;
	narc_log(NARC_WARNING, "server resolving: %s", server.host);
	narc_tcp_client *client = (narc_tcp_client *)server.client;
	uv_getaddrinfo(server.loop, &client->resolver, handle_tcp_resolved, server.host, "80", &hints);
}

void
start_tcp_connect(struct addrinfo *res)
{
	narc_tcp_client *client = (narc_tcp_client *)server.client;
	uv_tcp_t 	*socket = (uv_tcp_t *)malloc(sizeof(uv_tcp_t));

	uv_tcp_init(server.loop, socket);
	uv_tcp_keepalive(socket, 1, 60);

	struct sockaddr_in dest;
	uv_ip4_addr(res->ai_addr->sa_data, server.port, &dest);

	uv_connect_t *connect = malloc(sizeof(uv_connect_t));
	if(uv_tcp_connect(connect, socket, (struct sockaddr *)&dest, handle_tcp_connect) == 0) {
		client->socket = socket;
		client->attempts += 1;
	}
	uv_freeaddrinfo(res);
}

void
start_tcp_read(uv_stream_t *stream)
{
	uv_read_start(stream, handle_tcp_read_alloc_buffer, handle_tcp_read);
}

void
start_tcp_connect_timer(void)
{
	uv_timer_t *timer = malloc(sizeof(uv_timer_t));
	if (uv_timer_init(server.loop, timer) == 0)
		uv_timer_start(timer, handle_tcp_connect_timeout, server.connect_retry_delay, 0);
}

/*================================== API ==================================== */

void
init_tcp_client(void)
{
	server.client = (void *)new_tcp_client();

	start_tcp_resolve();
}

void
clean_tcp_client(void)
{
	narc_tcp_client *client = (narc_tcp_client *)server.client;
	if (client->socket != NULL) {
		uv_close((uv_handle_t *)client->socket, (uv_close_cb)free);
		client->socket = NULL;
	}
	// server.client = NULL;
	// free(client);
}

void
submit_tcp_message(char *message)
{
	narc_tcp_client *client = (narc_tcp_client *)server.client;

	if ( ! tcp_client_established(client) ) {
		sdsfree(message);
		return;
	}

	uv_write_t *req = (uv_write_t *)malloc(sizeof(uv_write_t));
	uv_buf_t buf    = uv_buf_init(message, strlen(message));

	if (uv_write(req, client->stream, &buf, 1, handle_tcp_write) == 0)
		req->data = (void *)message;

}
