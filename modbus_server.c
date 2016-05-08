/*
 * Copyright (C) 2000 Shoichi Sakane <sakane@tanu.org>, All rights reserved.
 * See the file LICENSE in the top level directory for more details.
 */
#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <err.h>
#include <errno.h>

#include <modbus/modbus.h>
#include <pthread.h>

#include <sys/time.h>	/* for gettimeofday() */
#define TIME_AVOID_ERROR_REPEATING 10

#include <math.h>	/* sign module */

#include "app.h"

int f_debug = 0;

char *prog_name = NULL;

struct server_param_t {
	pthread_t th;
	modbus_t *ctx;
	int s;
	modbus_mapping_t *map;
};

#define MAX_THREAD 4
int n_thread = 0;
pthread_mutex_t mutex;	/* to increase/descrease n_thread */

#define D2R (3.14/180)

void
usage()
{
	printf(
"Usage: %s [-S host] [-P port] [-dh]\n"
	, prog_name);

	exit(0);
}

void *
module01(void *param)
{
	modbus_mapping_t *map = (modbus_mapping_t *)param;
	static double t = 0;
	double k;
	struct timeval tv;

#define _V_APP_BASE(m) ((m)->tab_registers[APP_BASE_ADDR])
#define _V_APP_VAL(m) ((m)->tab_input_registers[APP_VAL_ADDR])

	tv.tv_sec = 1;
	tv.tv_usec = 0;
	while (1) {
		select(0, NULL, NULL, NULL, &tv);
		k = sin(t * D2R * 2) + 2 * sin(t * D2R) * 100;
		_V_APP_VAL(map) = k + 100 * _V_APP_BASE(map);
		if (f_debug > 2) {
			printf("base=%u k=%f val=%u\n",
			_V_APP_BASE(map), k, _V_APP_VAL(map));
		}
		t++;
	}

	return 0;
}

int
start_pseudo_sensor(modbus_mapping_t *map)
{
	pthread_attr_t attr;
	pthread_t th;

	/* set initi value */
	map->tab_registers[APP_BASE_ADDR] = APP_BASE_LOW;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&th, &attr, (void *)module01, (void *)map);

	return 0;
}

void *
server_process(void *param)
{
	struct server_param_t *sp = (struct server_param_t *)param;
	uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
	int recv_len, sent_len;

	/* XXX make it thread. */
	if (f_debug)
		printf("start the session on %d\n", sp->s);

	while ((recv_len = modbus_receive(sp->ctx, query)) > 0) {
		if (f_debug > 1)
			printf("received %d bytes on %d.\n", recv_len, sp->s);
		sent_len = modbus_reply(sp->ctx, query, recv_len, sp->map);
		if (sent_len < 0)
			break;
		if (f_debug > 1)
			printf("sent %d bytes on %d.\n", sent_len, sp->s);
	}

	if (f_debug)
		printf("quit the session on %d\n", sp->s);

	modbus_close(sp->ctx);
	modbus_free(sp->ctx);
	free(sp);

	pthread_mutex_lock(&mutex);
	n_thread--;
	pthread_mutex_unlock(&mutex);

	return 0;
}

void
new_session(modbus_t *ctx0, int s0, modbus_mapping_t *map)
{
	modbus_t *ctx;
	int s;
	struct server_param_t *sp;
	pthread_attr_t attr;
	static struct timeval prev = { 0, 0 };

	if (n_thread == MAX_THREAD) {
		struct timeval now;
		gettimeofday(&now, NULL);
		if (now.tv_sec - prev.tv_sec < TIME_AVOID_ERROR_REPEATING) {
			return;
		}
		warn("ERROR: %s: the # of threads has reached to the max %d.",
		    __FUNCTION__, n_thread);
		prev.tv_sec = now.tv_sec;
		return;
	}
	pthread_mutex_lock(&mutex);
	n_thread++;
	pthread_mutex_unlock(&mutex);

	if ((ctx = modbus_copy_ctx(ctx0)) == NULL) {
		warn("ERROR: %s: modbus_copy_ctx()", __FUNCTION__);
		return;
	}
	if ((s = modbus_tcp_pi_accept(ctx, &s0)) < 0) {
		warn("ERROR: %s: modbus_tcp_pi_accept()", __FUNCTION__);
		/* XXX s0 was closed in modbus_tcp_pi_accept(), sigh... */
		modbus_free(ctx);
		return;
	}

	if ((sp = malloc(sizeof(struct server_param_t))) == NULL) {
		warn("ERROR: %s: malloc(session_t)", __FUNCTION__);
		modbus_close(ctx);
		modbus_free(ctx);
		return;
	}
	sp->ctx = ctx;
	sp->s = s;
	sp->map = map;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&sp->th, &attr, (void *)server_process, (void *)sp);

	/* thread th is not stored, because the thread is detached. */
}

int
run(char *host, char *port, int mode)
{
	modbus_t *ctx;
	modbus_mapping_t *map;
	int *sock_list, sock_len;
	int nfds, i;
	fd_set readfds, fdmask;
	int error;

	if ((error = pthread_mutex_init(&mutex, NULL)) != 0) {
		errx(1, "ERROR: %s: pthread_mutex_init(): %s",
		    __FUNCTION__, strerror(error));
	}

	if ((ctx = modbus_new_tcp_pi(host, port)) == NULL)
		err(1, "ERROR: %s: modbus_new_tcp_pi()", __FUNCTION__);

	if (f_debug) {
		printf("start modbus server on %s [%s]\n",
		    host == NULL ? "any" : host, port);
	}

	if (f_debug > 2)
		modbus_set_debug(ctx, TRUE);

	if ((map = modbus_mapping_new(0, 0, 8, 8)) == NULL) {
		modbus_free(ctx);
		errx(1, "ERROR: %s: modbus_mapping_new(): %s",
		    __FUNCTION__, modbus_strerror(errno));
	}

	switch (mode) {
	case 1:
		start_pseudo_sensor(map);
		break;
	default: /* do nothing (default) */
		break;
	}

	sock_list = NULL;
	sock_len = 0;
	if (modbus_tcp_pim_listen(ctx, 1, &sock_list, &sock_len) < 0) {
		modbus_free(ctx);
		errx(1, "ERROR: %s: modbus_tcp_listen()",
		    __FUNCTION__);
	}

	nfds = 0;
	FD_ZERO(&fdmask);
	for (i = 0; i < sock_len; i++) {
		FD_SET(sock_list[i], &fdmask);
		nfds = nfds > sock_list[i] ? nfds : sock_list[i];
	}
	nfds++;

	do {
		readfds = fdmask;

		error = select(nfds, &readfds, NULL, NULL, NULL);
		if (error == -1) {
			switch (errno) {
			case EAGAIN:
				continue;
			default:
				err(1, "ERROR: %s: select()", __FUNCTION__);
			}
		}

		for (i = 0; i < sock_len; i++) {
			if (FD_ISSET(sock_list[i], &readfds))
				new_session(ctx, sock_list[i], map);
		}

	} while (1);

	modbus_mapping_free(map);
	modbus_close(ctx);
	modbus_free(ctx);

	pthread_mutex_destroy(&mutex);

	return 0;
}

int
main(int argc, char *argv[])
{
	int ch;
	char *host = NULL, *port = "50200";
	int mode = 0;

	prog_name = 1 + rindex(argv[0], '/');

	while ((ch = getopt(argc, argv, "S:P:m:dh")) != -1) {
		switch (ch) {
		case 'S':
			host = optarg;
			break;
		case 'P':
			port = optarg;
			break;
		case 'm':
			mode = atoi(optarg);
			break;
		case 'd':
			f_debug++;
			break;
		case 'h':
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		usage();

	run(host, port, mode);

	return 0;
}

