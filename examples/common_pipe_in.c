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
#include "./common_pipe_in.h"

static struct zclt_env_s ctx;
static int out_evt = 0;

static void
_manage_in(struct zsock_s *zs)
{
    int rc;
    guint count_messages = 0;
    size_t count_bytes = 0;

    g_assert(zs != NULL);
    g_assert(zs->zs != NULL);

    for (;;) {
        zmq_msg_t msg;
        zmq_msg_init(&msg);
        rc = zmq_msg_recv(&msg, zs->zs, ZMQ_NOBLOCK);

        if (rc < 0) {
            zmq_msg_close(&msg);
            break;
        }

        ++ count_messages;
        count_bytes += zmq_msg_size(&msg);
        fwrite(zmq_msg_data(&msg), zmq_msg_size(&msg), 1, stdout);
        zmq_msg_close(&msg);
    }

    fflush(stdout);
    g_debug("ZSOCK [%s] managed %u / %"G_GSIZE_FORMAT, zs->fullname,
            count_messages, count_bytes);
}

static int
on_output(void *c, int fd, int evt)
{
    FILE *out = c;
    (void) fd, (void) evt;

    if (!zsock_ready(ctx.zsock)) {
        out_evt = 0;
        return 0;
    }

    if (ferror(out) || feof(out)) {
        g_debug("EOF!");
        zreactor_stop(ctx.zenv.zr);
        return -1;
    }

    return 0;
}

static void
sighandler_stop(int s)
{
    zreactor_stop(ctx.zenv.zr);
    signal(s, sighandler_stop);
}

int
main_common_pipe_in(const gchar *ztype, int argc, char **argv)
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
    ctx.zsock->evt = ZMQ_POLLIN;
    ctx.zsock->ready_in = _manage_in;

    fcntl(0, F_SETFL, O_NONBLOCK|fcntl(0, F_GETFL));
    zreactor_add_fd(ctx.zenv.zr, 1, &out_evt, on_output, stdout);
    int rc = zreactor_run(ctx.zenv.zr);
    zclt_env_close(&ctx);
    return rc != 0;
}

