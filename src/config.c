// -*- mode: c; tab-width: 8; indent-tabs-mode: 1; st-rulers: [70] -*-
// vim: ts=8 sw=8 ft=c noet

#include "config.h"
#include "narc.h"
#include "stream.h"

#include "sds.h"	/* dynamic safe strings */
// #include "malloc.h"	/* total memory usage aware version of malloc/free */

#include <stdio.h>	/* standard buffered input/output */
#include <stdlib.h>	/* standard library definitions */
#include <errno.h>	/* system error numbers */
#include <syslog.h>	/* definitions for system error logging */
#include <string.h>	/* string operations */

/*-----------------------------------------------------------------------------
 * Data types
 *----------------------------------------------------------------------------*/

static struct {
	const char     *name;
	const int       value;
} validSyslogFacilities[] = {
	{"user",    LOG_USER},
	{"local0",  LOG_LOCAL0},
	{"local1",  LOG_LOCAL1},
	{"local2",  LOG_LOCAL2},
	{"local3",  LOG_LOCAL3},
	{"local4",  LOG_LOCAL4},
	{"local5",  LOG_LOCAL5},
	{"local6",  LOG_LOCAL6},
	{"local7",  LOG_LOCAL7},
	{NULL, 0}
};

static struct {
	const char	*name;
	const int 	value;
} validSyslogPriorities[] = {
	{"emergency",	LOG_EMERG},
	{"alert",	LOG_ALERT},
	{"critical",	LOG_CRIT},
	{"error",	LOG_ERR},
	{"warning",	LOG_WARNING},
	{"notice",	LOG_NOTICE},
	{"info",	LOG_INFO},
	{"debug",	LOG_DEBUG},
	{NULL, 0}
};

/*-----------------------------------------------------------------------------
 * Config file parsing
 *----------------------------------------------------------------------------*/

int
yesnotoi(char *s)
{
	if (!strcasecmp(s,"yes")) return 1;
	else if (!strcasecmp(s,"no")) return 0;
	else return -1;
}

void
load_server_config_from_string(char *config)
{
	char *err = NULL;
	int linenum = 0, totlines, i;
	sds *lines;

	lines = sdssplitlen(config,strlen(config),"\n",1,&totlines);

	for (i = 0; i < totlines; i++) {
		sds *argv;
		int argc;

		linenum = i+1;
		lines[i] = sdstrim(lines[i]," \t\r\n");

		/* Skip comments and blank lines */
		if (lines[i][0] == '#' || lines[i][0] == '\0') continue;

		/* Split into arguments */
		argv = sdssplitargs(lines[i],&argc);
		if (argv == NULL) {
			err = "Unbalanced quotes in configuration line";
			goto loaderr;
		}

		/* Skip this line if the resulting command vector is empty. */
		if (argc == 0) {
			sdsfreesplitres(argv,argc);
			continue;
		}
		sdstolower(argv[0]);

		/* Execute config directives */
		if (!strcasecmp(argv[0], "daemonize") && argc == 2) {
			if ((server.daemonize = yesnotoi(argv[1])) == -1) {
				err = "argument must be 'yes' or 'no'"; goto loaderr;
			}
		} else if (!strcasecmp(argv[0], "pidfile") && argc == 2) {
			free(server.pidfile);
			server.pidfile = strdup(argv[1]);
		} else if (!strcasecmp(argv[0], "loglevel") && argc == 2) {
			if (!strcasecmp(argv[1],"debug")) server.verbosity = NARC_DEBUG;
			else if (!strcasecmp(argv[1],"verbose")) server.verbosity = NARC_VERBOSE;
			else if (!strcasecmp(argv[1],"notice")) server.verbosity = NARC_NOTICE;
			else if (!strcasecmp(argv[1],"warning")) server.verbosity = NARC_WARNING;
			else {
				err = "Invalid log level. Must be one of debug, notice, warning";
				goto loaderr;
			}
		} else if (!strcasecmp(argv[0],"logfile") && argc == 2) {
			FILE *logfp;

			free(server.logfile);
			server.logfile = strdup(argv[1]);
			if (server.logfile[0] != '\0') {
				/* Test if we are able to open the file. The server will not
				* be able to abort just for this problem later... */
				logfp = fopen(server.logfile,"a");
				if (logfp == NULL) {
					err = sdscatprintf(sdsempty(),
						"Can't open the log file: %s", strerror(errno));
					goto loaderr;
				}
				fclose(logfp);
			}
		} else if (!strcasecmp(argv[0],"syslog-enabled") && argc == 2) {
			if ((server.syslog_enabled = yesnotoi(argv[1])) == -1) {
				err = "argument must be 'yes' or 'no'"; goto loaderr;
			}
		} else if (!strcasecmp(argv[0],"syslog-ident") && argc == 2) {
			if (server.syslog_ident) free(server.syslog_ident);
				server.syslog_ident = strdup(argv[1]);
		} else if (!strcasecmp(argv[0],"syslog-facility") && argc == 2) {
			int i;

			for (i = 0; validSyslogFacilities[i].name; i++) {
				if (!strcasecmp(validSyslogFacilities[i].name, argv[1])) {
					server.syslog_facility = validSyslogFacilities[i].value;
					break;
				}
			}

			if (!validSyslogFacilities[i].name) {
				err = "Invalid log facility. Must be one of 'user' or between 'local0-local7'";
				goto loaderr;
			}
		} else if (!strcasecmp(argv[0], "remote-host") && argc == 2) {
			free(server.host);
			server.host = strdup(argv[1]);
		} else if (!strcasecmp(argv[0], "remote-port") && argc == 2) {
			server.port = atoi(argv[1]);
			if (server.port < 0 || server.port > 65535) {
				err = "Invalid port"; goto loaderr;
			}
		} else if (!strcasecmp(argv[0], "remote-proto") && argc == 2) {
			if (!strcasecmp(argv[1],"udp")) server.protocol = NARC_PROTO_UDP;
			else if (!strcasecmp(argv[1],"tcp")) server.protocol = NARC_PROTO_TCP;
			else {
				err = "Invalid protocol. Must be either udp or tcp";
				goto loaderr;
			}
		} else if (!strcasecmp(argv[0], "max-connect-attempts") && argc == 2) {
			server.max_connect_attempts = atoi(argv[1]);
		} else if (!strcasecmp(argv[0], "connect-retry-delay") && argc == 2) {
			server.connect_retry_delay = atoll(argv[1]);
		} else if (!strcasecmp(argv[0], "max-open-attempts") && argc == 2) {
			server.max_open_attempts = atoi(argv[1]);
		} else if (!strcasecmp(argv[0], "open-retry-delay") && argc == 2) {
			server.open_retry_delay = atoll(argv[1]);
		} else if (!strcasecmp(argv[0], "stream-id") && argc == 2) {
			free(server.stream_id);
			server.stream_id = strdup(argv[1]);
		} else if (!strcasecmp(argv[0], "stream-facility") && argc == 2) {
			int i;

			for (i = 0; validSyslogFacilities[i].name; i++) {
				if (!strcasecmp(validSyslogFacilities[i].name, argv[1])) {
					server.stream_facility = validSyslogFacilities[i].value;
					break;
				}
			}

			if (!validSyslogFacilities[i].name) {
				err = "Invalid stream facility. Must be one of 'user' or between 'local0-local7'";
				goto loaderr;
			}
		} else if (!strcasecmp(argv[0], "stream-priority") && argc == 2) {
			int i;

			for (i = 0; validSyslogPriorities[i].name; i++) {
				if (!strcasecmp(validSyslogPriorities[i].name, argv[1])) {
					server.stream_priority = validSyslogPriorities[i].value;
					break;
				}
			}

			if (!validSyslogPriorities[i].name) {
				err = "Invalid stream priority. Must be one of: 'emergency', 'alert', 'critical', 'error', 'warning', 'info', 'notice', or 'debug'";
				goto loaderr;
			}
		} else if (!strcasecmp(argv[0],"stream") && argc == 3) {
			char *id = sdsdup(argv[1]);
			char *file = sdsdup(argv[2]);
			narc_stream *stream = new_stream(id, file);
			listAddNodeTail(server.streams, (void *)stream);
		} else if (!strcasecmp(argv[0],"rate-limit") && argc == 2) {
			server.rate_limit = atoi(argv[1]);
		} else if (!strcasecmp(argv[0],"rate-time") && argc == 2) {
			server.rate_time = atoi(argv[1]);
		} else if (!strcasecmp(argv[0],"truncate-limit") && argc == 2) {
			server.truncate_limit = atoi(argv[1]);
		} else {
			err = "Bad directive or wrong number of arguments"; goto loaderr;
		}
		sdsfreesplitres(argv,argc);
	}
	sdsfreesplitres(lines,totlines);

	return;

loaderr:
	fprintf(stderr, "\n*** FATAL CONFIG FILE ERROR ***\n");
	fprintf(stderr, "Reading the configuration file, at line %d\n", linenum);
	fprintf(stderr, ">>> '%s'\n", lines[i]);
	fprintf(stderr, "%s\n", err);
	exit(1);
}

/* Load the server configuration from the specified filename.
 * The function appends the additional configuration directives stored
 * in the 'options' string to the config file before loading.
 *
 * Both filename and options can be NULL, in such a case are considered
 * empty. This way load_server_config can be used to just load a file or
 * just load a string. */
void
load_server_config(char *filename, char *options)
{
	sds config = sdsempty();
	char buf[NARC_CONFIGLINE_MAX+1];

	/* Load the file content */
	if (filename) {
		FILE *fp;

		if (filename[0] == '-' && filename[1] == '\0') {
			fp = stdin;
		} else {
			if ((fp = fopen(filename,"r")) == NULL) {
				narc_log(NARC_WARNING,
					"Fatal error, can't open config file '%s'", filename);
				exit(1);
			}
		}
		while(fgets(buf,NARC_CONFIGLINE_MAX+1,fp) != NULL)
			config = sdscat(config,buf);
		if (fp != stdin) fclose(fp);
	}
	/* Append the additional options */
	if (options) {
		config = sdscat(config,"\n");
		config = sdscat(config,options);
	}
	load_server_config_from_string(config);
	sdsfree(config);
}
