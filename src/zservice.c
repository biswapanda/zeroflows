#ifndef G_LOG_DOMAIN
# define G_LOG_DOMAIN "zeroflows"
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

#include <string.h>

#include <glib.h>
#include <zmq.h>
#include <zookeeper.h>

#include "./macros.h"
#include "./zeroflows.h"

static struct zsock_s *
zservice_create_socket(struct zservice_s *zsrv, struct cfg_sock_s *itf)
{
    int ztype = 0;
    GError *e = zsocket_resolve(itf->ztype, &ztype);
    if (e != NULL)
        g_error("Invalid socket type : (%d) %s", e->code, e->message);

    struct zsock_s *zsock = zsock_create(zsrv->puuid, zsrv->pcell);
    zsock_set_zmq_socket(zsock, zmq_socket(zsrv->zctx, ztype));
    zsock->zh = zsrv->zh;
    zsock->localname = g_strdup(itf->sockname);
    zsock->fullname = g_strconcat(zsrv->srvtype, ".", itf->sockname, NULL);

    g_debug("SOCK [%s] [%s]", itf->ztype, zsock->fullname);
    return zsock;
}

static void
zservice_ensure_socket(struct zservice_s *zsrv, struct cfg_sock_s *itf)
{
    struct zsock_s *zsock;

    if (!(zsock = g_tree_lookup(zsrv->socks, itf->sockname))) {
        zsock = zservice_create_socket(zsrv, itf);
        g_tree_insert(zsrv->socks, g_strdup(itf->sockname), zsock);
        zsock_register_in_reactor(zsrv->zr, zsock);
        zsock_configure(zsock, itf);
    }
    else {
        zsock_configure(zsock, itf);
    }
}

static void
on_config_completion(int r, const char *v, int vlen, const struct Stat *s, const void *u)
{
    struct zservice_s *zsrv = u;

    g_debug("%s(%d,%p,%p) %.*s", __FUNCTION__, r, s, u, vlen, v);
    ASSERT(zsrv != NULL);

    (void) s;
    if (r != ZOK) {
        ZK_DEBUG("get error : %d", r);
        return;
    }

    struct cfg_srv_s *cfg = zservice_parse_config_buffer(v, vlen);
    if (!cfg)
        g_warning("CFG error : invalid JSON object");
    else {
        for (guint i=0; i < cfg->socks->len ;++i) {
            struct cfg_sock_s *itf = cfg->socks->pdata[i];
            zservice_ensure_socket(zsrv, itf);
        }
        cfg_srv_destroy(cfg);
        g_debug("CFG done");
    }

    if (zsrv->on_config)
        zsrv->on_config(zsrv, zsrv->on_config_data);
}

static void
on_config_change(zhandle_t *zh, int t, int s, const char *p, void *u)
{
    struct zservice_s *zsrv = u;
    g_debug("%s(%p,%d,%d,%s,%p)", __FUNCTION__, zh, t, s, p, u);
    zservice_reload_configuration(zsrv);
}

void
zservice_reload_configuration(struct zservice_s *zsrv)
{
    ASSERT(zsrv != NULL);

    gchar *p = g_strdup_printf("/services/%s", zsrv->srvtype);
    int zrc = zoo_awget(zsrv->zh, p,
            on_config_change, zsrv,
            on_config_completion, zsrv);
    g_free(p);

    if (zrc != ZOK) {
        g_debug("Failed to ask the first configuration for [%s] (%d)",
                zsrv->srvtype, zrc);
        zreactor_stop(zsrv->zr);
    }
}

struct zservice_s*
zservice_create(struct zenv_s *zenv, const gchar *srvtype)
{
    static gchar *empty[] = {NULL};
    ASSERT(zenv != NULL);
    ASSERT(srvtype != NULL);

    // Create the service itself
    struct zservice_s *zsrv = g_malloc0(sizeof(struct zservice_s));
    zsrv->zh = zenv->zh;
    zsrv->zr = zenv->zr;
    zsrv->zctx = zenv->zctx;
    zsrv->srvtype = g_strdup(srvtype);
    zsrv->puuid = zenv->uuid;
    zsrv->pcell = zenv->cell;
    zsrv->socks = g_tree_new_full(strcmp3, NULL, g_free,
            (GDestroyNotify)zsock_destroy);

    // A few internal sockets always exist
    struct cfg_sock_s cfg_tick = { "_t", "zmq:SUB", empty, empty };
    zservice_ensure_socket(zsrv, &cfg_tick);

    struct cfg_sock_s cfg_stats = { "_stats", "zmq:PUB", empty, empty };
    zservice_ensure_socket(zsrv, &cfg_stats);

    struct cfg_sock_s cfg_sig = { "_sig", "zmq:SUB", empty, empty };
    zservice_ensure_socket(zsrv, &cfg_sig);

    // Trigger a reload of the configuration, then exit
    zservice_reload_configuration(zsrv);
    return zsrv;
}

void
zservice_destroy(struct zservice_s *zsrv)
{
    if (!zsrv)
        return;
    if (zsrv->socks)
        g_tree_destroy(zsrv->socks);
    if (zsrv->srvtype)
        g_free(zsrv->srvtype);
    g_free(zsrv);
}

struct zsock_s*
zservice_get_socket(struct zservice_s *zsrv, const gchar *n)
{
    struct zsock_s *zs = g_tree_lookup(zsrv->socks, n);
    if (!zs)
        g_error("BUG : required a socket that is not configured [%s]", n);
    return zs;
}

void
zservice_on_config(struct zservice_s *zsrv, gpointer data,
        void (*hook)(struct zservice_s*, gpointer))
{
    ASSERT(zsrv != NULL);
    zsrv->on_config = hook;
    zsrv->on_config_data = data;
}

