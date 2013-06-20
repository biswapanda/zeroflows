#ifndef G_LOG_DOMAIN
# define G_LOG_DOMAIN "zs.cli"
#endif

// Zero-Flows, actors plumbing with ZeroMQ & ZooKeeper
// Copyright (C) 2013 Jean-Francois SMIGIELSKI and all the contributors
//
// This file is part of Zero-Flows.
//
// Zero-Flows is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// and of the GNU Lesser General Public License along with this program.
// If not, see <http://www.gnu.org/licenses/>.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "./common.h"
#include "./common_pipe_out.h"

static struct zclt_env_s ctx;
static int in_evt = 0;

static void
_manage_out(struct zsock_s *zs)
{
    in_evt = ZMQ_POLLIN;
    g_debug("ZSOCK [%s] ready for output", zs->fullname);
}

static inline void
_wait_for_output(struct zsock_s *zs)
{
    zs->evt = ZMQ_POLLOUT;
    zs->ready_out = _manage_out;
}

static int
on_input(void *c, int fd, int evt)
{
    FILE *in = c;
    gchar *s, b[4096];
    (void) fd, (void) evt;

    if (!zsock_ready(ctx.zsock)) {
        g_debug("Output not ready");
        in_evt = 0;
        _wait_for_output(ctx.zsock);
        return 0;
    }

    if (ferror(in) || feof(in)) {
        g_debug("EOF!");
        zreactor_stop(ctx.zenv.zr);
        return -1;
    }

    if (!(s = fgets(b, sizeof(b), in))) {
        g_debug("EOF!");
        zreactor_stop(ctx.zenv.zr);
        return -1;
    }

    zmq_msg_t msg;
    int l = strlen(s);
    for (; g_ascii_isspace(s[l-1]) ;--l) {}
    zmq_msg_init_size(&msg, l);
    memcpy(zmq_msg_data(&msg), s, zmq_msg_size(&msg));
    int rc = zmq_sendmsg(ctx.zsock->zs, &msg, ZMQ_DONTWAIT);
    zmq_msg_close(&msg);

    (void) rc;
    g_assert(rc == l);
    return 0;
}

static void
sighandler_stop(int s)
{
    zreactor_stop(ctx.zenv.zr);
    signal(s, sighandler_stop);
}

int
main_common_pipe_out(const gchar *ztype, int argc, char **argv)
{
    main_set_log_handlers();
    if (argc < 2) {
        g_error("Usage: %s TARGET", argv[0]);
        return 1;
    }

    zclt_env_init(ztype, argv[1], &ctx);
    signal(SIGTERM, sighandler_stop);
    signal(SIGQUIT, sighandler_stop);
    signal(SIGINT, sighandler_stop);
    _wait_for_output(ctx.zsock);

    fcntl(0, F_SETFL, O_NONBLOCK|fcntl(0, F_GETFL));
    zreactor_add_fd(ctx.zenv.zr, 0, &in_evt, on_input, stdin);
    int rc = zreactor_run(ctx.zenv.zr);
    zclt_env_close(&ctx);
    return rc != 0;
}

