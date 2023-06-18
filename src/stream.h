// -*- mode: c; tab-width: 8; indent-tabs-mode: 1; st-rulers: [70] -*-
#ifndef NARC_STREAM
#define NARC_STREAM

#include "narc.h"
#include <uv.h>

/* Stream locking */
#define NARC_STREAM_LOCKED	1
#define NARC_STREAM_UNLOCKED	2
#define NARC_STREAM_BUFFERS	1

/*-----------------------------------------------------------------------------
 * Data types
 *----------------------------------------------------------------------------*/

typedef struct {
	char 	*id;					/* message id prefix */
	char 	*file;					/* absolute path to the file */
	int 	fd;					/* file descriptor */
	off_t 	size;					/* last known file size in bytes */
	uv_buf_t buffer[NARC_STREAM_BUFFERS];		/* read buffer (file content) */
	char 	line[(NARC_MAX_LOGMSG_LEN + 1) * 2];	/* the current and previous lines buffer */
	char	*current_line;				/* current line */
	char	*previous_line;				/* previous line */
	int	repeat_count;				/* how many times the previous line was repeated */
	int 	index;					/* the line character index */
	int 	lock;					/* read lock to prevent resetting buffers */
	int 	attempts;				/* open attempts */
	int	rate_count;				/*  */
	int	missed_count;				/*  */
	int     message_header_size;
	int64_t offset;
	int		truncate;
	uv_fs_event_t *fs_events;
	uv_timer_t *open_timer;
} narc_stream;

/*-----------------------------------------------------------------------------
 * Functions prototypes
 *----------------------------------------------------------------------------*/

/* watchers */
void	start_file_open(narc_stream *stream);
void	start_file_watcher(narc_stream *stream);
void	start_file_open_timer(narc_stream *stream);
void	start_file_stat(narc_stream *stream);
void	start_file_read(narc_stream *stream);
void	start_rate_limit_timer(narc_stream *stream);

/* api */
narc_stream 	*new_stream(char *id, char *file);
void		free_stream(void *ptr);
void		init_stream(narc_stream *stream);

#endif
