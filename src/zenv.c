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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <zmq.h>
#include <zookeeper.h>

#include "./macros.h"
#include "./zeroflows.h"

static void
uuid_randomize(gchar *d, gsize dl)
{
    static guint32 seq = 0;

    struct {
        int pid, ppid;
        GTimeVal now;
        guint32 seq;
        gpointer p0, p1, p2;
        gchar h[512];
    } bulk;

    memset(&bulk, 0, sizeof(bulk));
    bulk.pid = getpid();
    bulk.ppid = getppid();
    g_get_current_time(&(bulk.now));
    bulk.seq = ++seq;
    bulk.p0 = &bulk;
    bulk.p1 = d;
    bulk.p2 = &d;
    gethostname(bulk.h, sizeof(bulk.h));

    GChecksum *cs = g_checksum_new(G_CHECKSUM_SHA256);
    g_checksum_update(cs, (guint8*) &bulk, sizeof(bulk));
    g_strlcpy(d, g_checksum_get_string(cs), dl);
    g_checksum_free(cs);

    for (; dl ;--dl)
        d[dl-1] = g_ascii_toupper(d[dl-1]);
}

static gboolean
_has_forbidden_chars(const gchar *s)
{
    gchar forbidden[256];

    if (!s)
        return FALSE;

    // mark special characters forbidden in ZooKeeper paths
    memset(forbidden, 0, 256);
    forbidden['/'] = 1;
    forbidden[','] = 1;
    forbidden[';'] = 1;

    while (*s) {
        if (!g_ascii_isprint(*s)
                || g_ascii_isspace(*s)
                || forbidden[(guint8)*s])
            return TRUE;
    }
    return FALSE;
}

static gchar*
_zenv_get_zk_url(void)
{
    // First, try from the environment
    const gchar *zk_env = g_getenv(ZKURL_KEY_ENV);
    if (NULL != zk_env)
        return g_strdup(zk_env);

    // Now try in several configuration files
    gchar *result = NULL;
    gchar *u = NULL;
    gsize u_len = 0;

    if (g_file_get_contents(ZKFILE_LOCAL_PATH, &u, &u_len, NULL))
        result = g_strndup(u, u_len);
    else if (g_file_get_contents(ZKFILE_HOME_PATH, &u, &u_len, NULL))
        result = g_strndup(u, u_len);
    else if (g_file_get_contents(ZKFILE_GLOBAL_PATH, &u, &u_len, NULL))
        result = g_strndup(u, u_len);

    if (u) {
        g_free(u);
        u = NULL;
    }
    return result;
}

void
zenv_init(struct zenv_s *zenv)
{

    g_assert(zenv != NULL);
    memset(zenv, 0, sizeof(*zenv));

    // Init the "cell" from the environment if present or set a default value
    const gchar *cell_env = g_getenv(CELL_KEY_ENV);
    const gchar *uuid_env = g_getenv(UUID_KEY_ENV);
    gchar *zkurl = _zenv_get_zk_url();

    if (!zkurl)
        g_error("ZooKeeper not configured, found none of %s, %s, %s",
                ZKFILE_LOCAL_PATH, ZKFILE_HOME_PATH, ZKFILE_GLOBAL_PATH);
    if (_has_forbidden_chars(cell_env))
        g_error("CELL has invalid characters");
    if (_has_forbidden_chars(uuid_env))
        g_error("UUID has invalid characters");

    if (NULL != cell_env)
        zenv->cell = g_strdup(cell_env);
    else
        zenv->cell = g_strdup(CELL_VALUE_DEFAULT);

    if (NULL != uuid_env)
        zenv->uuid = g_strdup(uuid_env);
    else {
        zenv->uuid = g_malloc0(33);
        uuid_randomize(zenv->uuid, 33);
    }

    zenv->zh = zookeeper_init(zkurl, NULL, 5000, NULL, NULL, 0);
    g_assert(zenv->zh != NULL);

    zenv->zctx = zmq_ctx_new();
    g_assert(zenv->zctx != NULL);

    zenv->zr = zreactor_create();
    g_assert(zenv->zr != NULL);

    zreactor_add_zk(zenv->zr, zenv->zh);
}

void
zenv_close(struct zenv_s *zenv)
{
    g_assert(zenv != NULL);

    zreactor_destroy(zenv->zr);
    zmq_ctx_destroy(zenv->zctx);
    zookeeper_close(zenv->zh);
}

struct zservice_s*
zenv_create_service(struct zenv_s *zenv, const gchar *srvtype)
{
    g_assert(zenv != NULL);
    g_assert(srvtype != NULL);

    struct zservice_s *zsrv = zservice_create(zenv->zctx, zenv->zh, srvtype,
            zenv->uuid, zenv->cell);
    zservice_register_in_reactor(zenv->zr, zsrv);

    return zsrv;
}

struct zsock_s*
zenv_create_socket(struct zenv_s *zenv, const gchar *name, const gchar *ztype)
{
    g_assert(zenv != NULL);
    g_assert(name != NULL);
    g_assert(ztype != NULL);

    int zt = 0;
    GError *e = zsocket_resolve(ztype, &zt);
    if (e != NULL) {
        g_message("ZMQ socket error : (%d) %s", e->code, e->message);
        g_clear_error(&e);
        errno = EINVAL;
        return NULL;
    }

    struct zsock_s *zsock = zsock_create(zenv->uuid, zenv->cell);
    zsock->zs = zmq_socket(zenv->zctx, zt);
    zsock->zh = zenv->zh;
    zsock->zctx = zenv->zctx;
    zsock->localname = g_strdup(name);
    zsock->fullname = g_strdup(name);

    return zsock;
}
 
