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

#include <modbus/modbus.h>

#include "app.h"

#define WR_BASE_LOW  0
#define WR_BASE_HIGH 1
#define RD_BASE      2
#define RD_VAL       3

int f_debug = 0;

char *prog_name = NULL;

void
usage()
{
	printf(
"Usage: %s [-P port] [-j job] [-dh] (host)\n"
"    job: 0: let mode low\n"
"         1: let mode high\n"
"         2: read mode\n"
"         3: read value (default)\n"
	, prog_name);

	exit(0);
}

int
m_write_register_one(modbus_t *ctx, int addr, uint16_t reg1)
{
	int ret;

	ret = modbus_write_register(ctx, addr, reg1);
	if (ret != 1) {
		warnx("ERROR: modbus_write_register(addr=%d reg1=%d): %d",
		    addr, reg1, ret);
		return -1;
	}

	return 0;
}

int
m_read_register_one(modbus_t *ctx, int addr, uint16_t *reg1)
{
	int ret;

	ret = modbus_read_registers(ctx, addr, 1, reg1);
	if (ret != 1) {
		warnx("ERROR: modbus_read_registers(addr=%d): %d", addr, ret);
		return -1;
	}

	return ret;
}

int
m_read_input_register_one(modbus_t *ctx, int addr, uint16_t *reg1)
{
	int ret;

	ret = modbus_read_input_registers(ctx, addr, 1, reg1);
	if (ret != 1) {
		warnx("ERROR: modbus_input_read_registers(addr=%d): %d",
		    addr, ret);
		return -1;
	}

	return ret;
}

modbus_t *
m_connect(char *host, char *port)
{
	modbus_t *ctx;

	if ((ctx = modbus_new_tcp_pi(host, port)) == NULL)
		err(1, "ERROR: %s: modbus_new_tcp_pi()", __FUNCTION__);

	if (f_debug > 2)
		modbus_set_debug(ctx, TRUE);

	if (modbus_connect(ctx) < 0) {
		modbus_free(ctx);
		errx(1, "ERROR: %s: modbus_connect()", __FUNCTION__);
	}

	return ctx;
}

void
m_close(modbus_t *ctx)
{
	modbus_close(ctx);
	modbus_free(ctx);
}

int
run(char *host, char *port, int f_job)
{
	modbus_t *ctx;
	uint16_t reg = 0;

	ctx = m_connect(host, port);

	switch (f_job) {
	case WR_BASE_LOW:
		m_write_register_one(ctx, APP_BASE_ADDR, APP_BASE_LOW);
		break;
	case WR_BASE_HIGH:
		m_write_register_one(ctx, APP_BASE_ADDR, APP_BASE_HIGH);
		break;
	case RD_BASE:
		m_read_register_one(ctx, APP_BASE_ADDR, &reg);
		printf("reg[%d] = %d\n", APP_BASE_ADDR, reg);
		break;
	case RD_VAL:
		m_read_input_register_one(ctx, APP_VAL_ADDR, &reg);
		printf("reg[%d] = %d\n", APP_VAL_ADDR, reg);
		break;
	default:
		usage();
	}

	m_close(ctx);

	return 0;
}

int
main(int argc, char *argv[])
{
	int ch;
	int f_job = 3;
	char *host = NULL, *port = "50200";

	prog_name = 1 + rindex(argv[0], '/');

	while ((ch = getopt(argc, argv, "P:j:dh")) != -1) {
		switch (ch) {
		case 'P':
			port = optarg;
			break;
		case 'j':
			f_job = atoi(optarg);
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

	if (argc != 1)
		usage();

	host = argv[0];

	run(host, port, f_job);

	return 0;
}

