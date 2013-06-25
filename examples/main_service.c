#ifndef G_LOG_DOMAIN
# define G_LOG_DOMAIN "zs.srv"
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

#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "./common.h"

struct zsrv_env_s ctx;
struct zsock_s *zs_in;
struct zsock_s *zs_out;

static void
_on_event_in(struct zsock_s *zs)
{
    int rci, rco, flags;
    size_t smore, count_bytes = 0;
    guint count_messages = 0;

    g_assert(zs != NULL);
    g_assert(zs == zs_in);
    g_assert(zs->zs != NULL);

    for (;;) {
        zmq_msg_t msg;
        zmq_msg_init(&msg);
        rci = zmq_msg_recv(&msg, zs->zs, ZMQ_NOBLOCK);
        if (rci < 0) {
            zmq_msg_close(&msg);
            break;
        }

        ++ count_messages;
        count_bytes += zmq_msg_size(&msg);

        flags = 0;
        smore = sizeof(flags);
        zmq_getsockopt(zs->zs, ZMQ_RCVMORE, &flags, &smore);
        flags = ZMQ_NOBLOCK | (flags?ZMQ_SNDMORE:0);
        
        if (0 > (rco = zmq_msg_send(&msg, zs_out->zs, flags)))
            g_debug("ZMQ rco = %d : (%d) %s", rco, errno, strerror(errno));
        zmq_msg_close(&msg);

        //g_assert(rci == rco);
    }

    g_debug("ZSOCK [%s] managed %u / %"G_GSIZE_FORMAT, zs->fullname,
            count_messages, count_bytes);
}

static void
_on_event_out(struct zsock_s *zs)
{
    g_debug("ZSOCK [%s] ready for output", zs->fullname);
}

static void
sighandler_stop(int s)
{
    zreactor_stop(ctx.zenv.zr);
    signal(s, sighandler_stop);
}

static void
_on_zservice_configured(struct zservice_s *zsrv, gpointer u)
{
    (void) u;
    g_debug("ZSRV configured, now applying event handlers");

    zs_in = zservice_get_socket(zsrv, "in");
    zs_in->ready_in = _on_event_in;
    zs_in->evt = ZMQ_POLLIN;

    zs_out = zservice_get_socket(zsrv, "out");
    zs_out->ready_out = _on_event_out;
    zs_out->evt = ZMQ_POLLOUT;
}

int
main(int argc, char **argv)
{
    main_set_log_handlers();
    if (argc < 2) {
        g_error("Usage: %s SRVTYPE", argv[0]);
        return 1;
    }

    zs_in = zs_out = NULL;
    zsrv_env_init(argv[1], &ctx);

    signal(SIGTERM, sighandler_stop);
    signal(SIGQUIT, sighandler_stop);
    signal(SIGINT, sighandler_stop);

    zservice_on_config(ctx.zsrv, ctx.zsrv, _on_zservice_configured);

    int rc = zreactor_run(ctx.zenv.zr);
    zsrv_env_close(&ctx);
    return rc != 0;
}

