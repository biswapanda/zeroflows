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

#include <glib.h>
#include <zmq.h>
#include <zookeeper.h>

#include "./macros.h"
#include "./zeroflows.h"

/**
 * @private
 * TODO document
 */
struct zconnect_s
{
    gchar *type;
    gchar *policy;
    struct zsock_s *zs; // the socket it belongs to

    // Always allocated and NULL terminated, it holds the currently configured
    // targets.
    gchar **urlv_current;

    // (struct cfg_listen_s*)
    GPtrArray *urlv_new;

    // Counters on ZooKeeper requests waiting.
    guint list_wanted;
    guint list_pending;
    guint list_watch;
    guint get_pending;

    // various markers used during a reconfiguration step.
    gboolean obsolete;
    gboolean exiting;
};

//------------------------------------------------------------------------------
// Various helpers without any common theme.
//------------------------------------------------------------------------------

/* Returns the ZSock string type, based on the intenral type of the underlying
 * ZMQ socket. */
static inline const gchar*
ztype2str(int ztype)
{
    switch (ztype) {
        case ZMQ_PUB:
            return "zmq:PUB";
        case ZMQ_SUB:
            return "zmq:SUB";
        case ZMQ_PUSH:
            return "zmq:PUSH";
        case ZMQ_PULL:
            return "zmq:PULL";
        default:
            return "?";
    }
}

/* Simple handler of the ZMQ socket internal type */
static inline int
get_ztype(void *s)
{
    int type = 0;
    size_t len = sizeof(int);
    zmq_getsockopt(s, ZMQ_TYPE, &type, &len);
    return type;
}

/* Returns if both socket types can be interconnected */
static inline gboolean
ztype_compatible(int ztype0, int ztype1)
{
    switch (ztype0) {
        case ZMQ_PUB:
            return ztype1 == ZMQ_SUB;
        case ZMQ_SUB:
            return ztype1 == ZMQ_PUB;
        case ZMQ_PUSH:
            return ztype1 == ZMQ_PULL;
        case ZMQ_PULL:
            return ztype1 == ZMQ_PUSH;
        default:
            return FALSE;
    }
}

/* Return the ZMQ events possible for this king of socket */
static inline int
zevents(void *s)
{
    switch (get_ztype(s)) {
        case ZMQ_PULL:
        case ZMQ_SUB:
            return ZMQ_POLLIN;
        case ZMQ_PUSH:
        case ZMQ_PUB:
            return ZMQ_POLLOUT;
        case ZMQ_XSUB:
        case ZMQ_XPUB:
        case ZMQ_ROUTER:
        case ZMQ_DEALER:
        case ZMQ_REP:
        case ZMQ_PAIR:
        case ZMQ_REQ:
            return ZMQ_POLLOUT|ZMQ_POLLIN;
        default:
            g_assert_not_reached();
            return 0;
    }
}

/* qsort()-style comparison function for (gchar*) */
static gint
pstr_cmp(gconstpointer p0, gconstpointer p1)
{
    return g_strcmp0(*(gchar**)p0, *(gchar**)p1);
}

static inline gchar**
_gpa_to_strv(GPtrArray *gpa)
{
    g_ptr_array_add(gpa, NULL);
    return (gchar**) g_ptr_array_free(gpa, FALSE);
}

/* Merge 2 sorted arrays in a signel sorted array */
static inline gchar**
_strv_merge(gchar **oldv, gchar **newv)
{
    GPtrArray *tmp = g_ptr_array_new();

    while (*oldv || *newv) {
        if (!*oldv)
            g_ptr_array_add(tmp, *(newv++));
        else if (!*newv)
            g_ptr_array_add(tmp, *(oldv++));
        else {
            int rc = strcmp(*oldv, *(newv++));
            if (!rc) {
                g_ptr_array_add(tmp, *(oldv++));
                g_ptr_array_add(tmp, *(newv++));
            }
            else if (rc < 0)
                g_ptr_array_add(tmp, *(oldv++));
            else
                g_ptr_array_add(tmp, *(newv++));
        }
    }

    return _gpa_to_strv(tmp);
}

static inline gchar **
_extract_urlv_from_listenv(GPtrArray *v)
{
    GPtrArray *tmp = g_ptr_array_new();

    for (guint i=0; i<v->len ;++i) {
        struct cfg_listen_s *cl = g_ptr_array_index(v, i);
        if (cl && cl->url) {
            g_ptr_array_add(tmp, cl->url);
            cl->url = NULL;
        }
    }

    g_ptr_array_sort(tmp, pstr_cmp);
    return _gpa_to_strv(tmp);
}


//-----------------------------------------------------------------------------
// Reconnection mechanics: when the whole list of new potential peers is known,
// we have to filter it through the sieve of interconnection policy (TODO), and
// then compute several sets of URL :
//  * kept : items from oldv, seen in newv
//  * appeared : items from newv not seen in oldv
//  * disappeared : items from oldv not seen in newv
//  * dupplicated : items from newv seen in oldv
//-----------------------------------------------------------------------------

/**
 * @private
 */
struct delta_s
{
    gchar **add;
    gchar **rem;
    gchar **kept;
    gchar **dup;
};

/* 'oldv' and 'newv' MUST be sorted and without dupplicated item.
 * We will run both arrays alongside and build 4 'deltas' arrays: one for
 * appeared items, one for disappread items, one with items of 'newv' already
 * present in 'oldv', and the last for items of 'oldv' present in 'newv'. */
static inline struct delta_s
_delta_compute(gchar **oldv, gchar **newv)
{
    GPtrArray *pnew = g_ptr_array_new();
    GPtrArray *plost = g_ptr_array_new();
    GPtrArray *pdup = g_ptr_array_new();
    GPtrArray *pkept = g_ptr_array_new();

    while (*oldv || *newv) {
        if (!*newv)
            g_ptr_array_add(plost, *(oldv++));
        else if (!*oldv)
            g_ptr_array_add(pnew, *(newv++));
        else {
            int rc = strcmp(*oldv, *newv);
            if (!rc) {
                g_ptr_array_add(pkept, *(newv++));
                g_ptr_array_add(pdup, *(oldv++));
            }
            else if (rc < 0)
                g_ptr_array_add(plost, *(oldv++));
            else
                g_ptr_array_add(pnew, *(newv++));
        }
    }

    struct delta_s delta;
    delta.add = _gpa_to_strv(pnew);
    delta.rem = _gpa_to_strv(plost);
    delta.kept = _gpa_to_strv(pkept);
    delta.dup = _gpa_to_strv(pdup);
    return delta;
}

static inline void
_zsock_real_connect(struct zsock_s *zsock, const gchar *url)
{
    guint *pi = g_tree_lookup(zsock->connect_real, url);
    if (pi)
        ++ *pi;
    else {
        zmq_connect(zsock->zs, url);
        pi = g_malloc(sizeof(guint));
        *pi = 1;
        g_tree_insert(zsock->connect_real, g_strdup(url), pi);
    }
}

static inline void
_zsock_real_connect_all(struct zsock_s *zs, gchar **urlv)
{
    while (*urlv)
        _zsock_real_connect(zs, *(urlv++));
}

static inline void
_zsock_real_disconnect(struct zsock_s *zsock, const gchar *url)
{
    guint *pi = g_tree_lookup(zsock->connect_real, url);
    if (!pi)
        g_warning("Not connected to [%s]", url);
    else {
        if (*pi) {
            zmq_disconnect(zsock->zs, url);
            -- *pi;
        }
        if (!*pi) 
            g_tree_remove(zsock->connect_real, url);
    }
}

static inline guint
_zsock_real_disconnect_all(struct zsock_s *zs, gchar **urlv)
{
    guint count = 0;
    for (; *urlv ;++count)
        _zsock_real_disconnect(zs, *(urlv++));
    return count;
}

static void
zco_reconnect(struct zconnect_s *zco)
{
    if (!zco || !zco->urlv_new)
        return ;

    gchar **oldv = zco->urlv_current;
    zco->urlv_current = NULL;

    gchar **newv = _extract_urlv_from_listenv(zco->urlv_new);
    g_ptr_array_set_size(zco->urlv_new, 0); // Cleanup hook called.

    struct delta_s delta = _delta_compute(oldv, newv);
    _zsock_real_connect_all(zco->zs, delta.add);
    _zsock_real_disconnect_all(zco->zs, delta.rem);
    zco->urlv_current = _strv_merge(delta.kept, delta.add);

    g_free(oldv); // items reused in 'delta'
    g_free(newv); // idem
    g_free(delta.kept); // items reused in 'zco->urlv_current'
    g_free(delta.add);  // idem
    g_strfreev(delta.rem);
    g_strfreev(delta.dup);
}


//-----------------------------------------------------------------------------
// Discovery of peers for active sockets, with ZooKeeper.
//-----------------------------------------------------------------------------

static void on_get(int r, const char *v, int vl, const struct Stat *s, const void *u);
static void on_list_completion(int r, const struct String_vector *sv, const void *u);
static void on_list_change(zhandle_t *zh, int t, int s, const char *p, void *u);

static void on_bind_create(int r, const char *v, const void *u);

static void
restart_list(struct zconnect_s *zco)
{
    // Get rid of values already got but not yet taken into account
    g_ptr_array_set_size(zco->urlv_new, 0);

    // Start the ZooKeeper request
    gchar *p = g_strdup_printf("/listen/%s", zco->type);
    int rc = zoo_awget_children(zco->zs->zh, p,
            on_list_change, zco,
            on_list_completion, zco);
    ZK_DEBUG("awget_children(%s) = %d", p, rc);
    g_free(p);

    if (rc == ZOK) {
        ++ zco->list_pending;
        ++ zco->list_watch;
    }
}

static inline void
maybe_relist(struct zconnect_s *zco)
{
    if (!zco->get_pending && !zco->list_pending && zco->list_wanted) {
        zco->list_wanted = 0;
        restart_list(zco);
    }
}

/* Calls the reconnection when we are sure the list of peers won't change. */
static inline void
maybe_reconnect(struct zconnect_s *zco)
{
    if (!zco->get_pending && !zco->list_pending && !zco->list_wanted)
        zco_reconnect(zco);
}

static void
on_get(int r, const char *v, int vl, const struct Stat *s, const void *u)
{
    struct zconnect_s *zco = u;
    (void) r, (void) s;

    ASSERT(zco != NULL);
    g_debug("%s(%s -> %s) %.*s", __FUNCTION__, zco->zs->fullname,
            zco->type, v ? vl : 0, v);
    -- zco->get_pending;

    if (r == ZOK) {
        struct cfg_listen_s *cfg = zlisten_parse_config_buffer(v, vl);
        if (cfg != NULL) {
            int own_ztype, opposite_ztype;
            own_ztype = get_ztype(zco->zs->zs);
            GError *e = zsocket_resolve(cfg->ztype, &opposite_ztype);
            if (e != NULL) {
                g_debug("Socket ignored (invalid ztype)");
                cfg_listen_destroy(cfg);
            }
            else if (!ztype_compatible(own_ztype, opposite_ztype)) {
                g_debug("Socket ignored (ztypes not compatible: %s vs. %s)",
                        ztype2str(own_ztype), ztype2str(opposite_ztype));
                cfg_listen_destroy(cfg);
            }
            else
                g_ptr_array_add(zco->urlv_new, cfg);
        }
    }

    maybe_reconnect(zco);
    maybe_relist(zco);
}

static void
on_list_completion(int r, const struct String_vector *sv, const void *u)
{
    struct zconnect_s *zco = u;

    ASSERT(zco != NULL);
    g_debug("%s(%s -> %s) %u", __FUNCTION__, zco->zs->fullname,
            zco->type, sv ? sv->count : 0);
    -- zco->list_pending;

    if (!zco->obsolete && r == ZOK)  {
        for (gint32 i=0; i < sv->count ;++i) {
            gchar *p = g_strdup_printf("/listen/%s/%s", zco->type, sv->data[i]);
            int rc = zoo_awget(zco->zs->zh, p, NULL, NULL, on_get, zco);
            ZK_DEBUG("awget(%s) = %d", p, rc);
            if (rc == ZOK)
                ++ zco->get_pending;
            g_free(p);
        }
    }
    maybe_relist(zco);
}

static void
on_list_change(zhandle_t *zh, int t, int s, const char *p, void *u)
{
    struct zconnect_s *zco = u;
    (void) zh, (void) t, (void) s, (void) p;

    ASSERT(zco != NULL);
    g_debug("%s(%s -> %s)", __FUNCTION__, zco->zs->fullname, zco->type);

    g_assert(zco->list_watch > 0);
    -- zco->list_watch;

    if (!zco->obsolete) {
        ++ zco->list_wanted;
        maybe_relist(zco);
    }
}

static void
zco_disconnect_all(struct zconnect_s *zco)
{
    if (_zsock_real_disconnect_all(zco->zs, zco->urlv_current)) {
        g_strfreev(zco->urlv_current);
        zco->urlv_current = g_malloc0(sizeof(gchar*));
    }
}

static void
zco_destroy(struct zconnect_s *zco)
{
    if (!zco)
        return ;
    if (zco->type) {
        g_free(zco->type);
        zco->type = NULL;
    }
    if (zco->policy) {
        g_free(zco->policy);
        zco->policy = NULL;
    }
    if (zco->urlv_current) {
        zco_disconnect_all(zco);
        g_strfreev(zco->urlv_current);
        zco->urlv_current = NULL;
    }
    if (zco->urlv_new) {
        while (zco->urlv_new->len) {
            struct cfg_listen_s *cl = zco->urlv_new->pdata[0];
            g_ptr_array_remove_index_fast(zco->urlv_new, 0);
            cfg_listen_destroy(cl);
        }
        g_ptr_array_free(zco->urlv_new, TRUE);
        zco->urlv_new = NULL;
    }

    zco->zs = NULL;
    g_free(zco);
}

static struct zconnect_s*
zco_create(struct zsock_s *zs, const gchar *type, const gchar *pol)
{
    struct zconnect_s *zco;
    zco = g_malloc0(sizeof(struct zconnect_s));
    zco->obsolete = FALSE;
    zco->type = g_strdup(type);
    zco->policy = g_strdup(pol);
    zco->zs = zs;
    zco->urlv_current = g_malloc0(sizeof(gchar*));
    zco->urlv_new = g_ptr_array_new_full(32, (GDestroyNotify)cfg_listen_destroy);
    return zco;
}

static inline void
_zsock_connect_mark_obsolete(struct zsock_s *zsock)
{
    gboolean hook(gpointer k, struct zconnect_s *zco, gpointer u) {
        (void) k, (void) u;
        zco->obsolete = TRUE;
        return FALSE;
    }
    g_tree_foreach(zsock->connect_cfg, (GTraverseFunc)hook, NULL);
}

static inline void
_zsock_connect_configure(struct zsock_s *zsock, struct cfg_sock_s *cfg)
{
    for (gchar **p = cfg->connect ;;) {
        gchar *type, *policy;
        if (!(type = *(p++)))
            break;
        if (!(policy = *(p++)))
            break;
        zsock_connect(zsock, type, policy);
    }
}

static inline void
_zsock_connect_purge_obsolete(struct zsock_s *zsock)
{
    gboolean hook(gchar *k, struct zconnect_s *zco, gpointer u) {
        (void) k, (void) u;
        if (zco->obsolete) {
            zco_disconnect_all(zco);
            // TODO : If a zconnect is still marked as obsolete, we cannot
            // destroy it because it maybe has registered watch on the ZK
            // node.
        }
        return FALSE;
    }
    g_tree_foreach(zsock->connect_cfg, (GTraverseFunc)hook, NULL);
}

void
zsock_connect(struct zsock_s *zsock, const gchar *type, const gchar *policy)
{
    struct zconnect_s *zco;

    ASSERT(type != NULL);
    ASSERT(policy != NULL);
    ASSERT(zsock != NULL);
    ASSERT(zsock->connect_cfg != NULL);

    if (!(zco = g_tree_lookup(zsock->connect_cfg, type))) {
        zco = zco_create(zsock, type, policy);
        g_tree_insert(zsock->connect_cfg, g_strdup(type), zco);
        restart_list(zco);
    }
    else {
        if (zco->policy)
            g_free(zco->policy);
        zco->policy = g_strdup(policy);
    }

    zco->obsolete = FALSE;
    g_debug(" %s -> [%s]", zsock->fullname, zco->type);
}


//-----------------------------------------------------------------------------
// Passive sockets registration, with ZooKeeper
//-----------------------------------------------------------------------------

static void
on_bind_create(int r, const char *v, const void *u)
{
    (void) u;
    g_debug("%s(%d,%s,%p)", __FUNCTION__, r, v, u);
}

static void
_zsock_register_url(struct zsock_s *zsock, const gchar *url)
{
    g_debug(" %s <- [%s]", zsock->fullname, url);

    GString *body = g_string_new("{");
    g_string_append(body, "\"type\":\"");
    g_string_append(body, zsock->fullname);
    g_string_append(body, "\",\"ztype\":\"");
    g_string_append(body, ztype2str(get_ztype(zsock->zs)));
    g_string_append(body, "\",\"url\":\"");
    g_string_append(body, url);
    g_string_append(body, "\",\"uuid\":\"");
    g_string_append(body, zsock->puuid);
    g_string_append(body, "\",\"cell\":\"");
    g_string_append(body, zsock->pcell);
    g_string_append(body, "\"}");
    
    gchar *path = g_strdup_printf("/listen/%s/%s-",
            zsock->fullname, zsock->puuid);

    int rc = zoo_acreate(zsock->zh, path, body->str, body->len,
            &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL|ZOO_SEQUENCE,
            on_bind_create, NULL);
    ZK_DEBUG("acreate(%s) = %d", path, rc);
    g_string_free(body, TRUE);
    g_free(path);
}

static void
_zsock_bind(struct zsock_s *zsock, const gchar *endpoint)
{
    int rc;
    gchar url[256];
    gsize urllen = 256;

    if (0 > (rc = zmq_bind(zsock->zs, endpoint)))
        return;

    memset(url, 0, 256);
    if (0 > zmq_getsockopt(zsock->zs, ZMQ_LAST_ENDPOINT, url, &urllen))
        return;

    g_tree_insert(zsock->bind_set, g_strdup(endpoint), g_strdup(url));
    _zsock_register_url(zsock, url);
}


//------------------------------------------------------------------------------
// Public API
//------------------------------------------------------------------------------

void
zsock_configure(struct zsock_s *zsock, struct cfg_sock_s *cfg)
{
    ASSERT(zsock != NULL);
    ASSERT(cfg != NULL);

    // connect: first, we mark the zconnect_s targets as "obsolete",
    // then when a target is refresh we remove the flag, so that we
    // can identify in a third step all the targets that were not
    // present in the last configuration
    _zsock_connect_mark_obsolete(zsock);
    _zsock_connect_configure(zsock, cfg);
    _zsock_connect_purge_obsolete(zsock);

    // listen
    // TODO : check if some URL disappeared or other appeared
    for (gchar **p = cfg->listen; *p ;++p)
        _zsock_bind(zsock, *p);
}

gboolean
zsock_ready(struct zsock_s *zsock)
{
    ASSERT(zsock != NULL);
    ASSERT(zsock->connect_real != NULL);
    ASSERT(zsock->bind_set != NULL);

    if (g_tree_nnodes(zsock->connect_real) == 0
            && g_tree_nnodes(zsock->bind_set) == 0)
        return FALSE;

    zmq_pollitem_t item = {zsock->zs, -1, ZMQ_POLLOUT, 0};
    return 1 == zmq_poll(&item, 1, 0);
}

struct zsock_s*
zsock_create(const gchar *pu, const gchar *pc)
{
    struct zsock_s *zsock = g_malloc0(sizeof(struct zsock_s));

    zsock->puuid = pu;
    zsock->pcell = pc;

    zsock->connect_real = g_tree_new_full(strcmp3, NULL, g_free, g_free);
    zsock->connect_cfg = g_tree_new_full(strcmp3, NULL, g_free,
            (GDestroyNotify)zco_destroy);
    zsock->bind_set = g_tree_new_full(strcmp3, NULL, g_free, g_free);

    return zsock;
}

void
zsock_destroy(struct zsock_s *zsock)
{
    if (!zsock)
        return;

    if (zsock->zs) {
        zmq_close(zsock->zs);
        zsock->zs = NULL;
    }

    if (zsock->fullname) {
        g_free(zsock->fullname);
        zsock->fullname = NULL;
    }

    if (zsock->connect_cfg) {
        g_tree_destroy(zsock->connect_cfg);
        zsock->connect_cfg = NULL;
    }

    if (zsock->connect_real) {
        gboolean runner(gpointer u, gpointer i0, gpointer i1) {
            (void) i0, (void) i1;
            (int) zmq_disconnect(zsock->zs, (gchar*)u);
            return FALSE;
        }
        g_tree_foreach(zsock->connect_real, runner, NULL);
        g_tree_destroy(zsock->connect_real);
        zsock->connect_real = NULL;
    }

    if (zsock->bind_set) {
        g_tree_destroy(zsock->bind_set);
        zsock->bind_set = NULL;
    }

    g_free(zsock);
}

static int
zsock_handler(struct zsock_s *zsock, void *s, int evt)
{
    (void) s;
    ASSERT(s != NULL);
    ASSERT(zsock != NULL);
    ASSERT(zsock->zs == s);

    if (evt & ZMQ_POLLOUT) {
        zsock->evt &= ~ZMQ_POLLOUT;
        if (zsock->ready_out)
            zsock->ready_out(zsock);
    }

    if (evt & ZMQ_POLLIN) {
        if (zsock->ready_in)
            zsock->ready_in(zsock);
    }

    return 0;
}

void
zsock_register_in_reactor(struct zreactor_s *zr, struct zsock_s *zsock)
{
    zreactor_add_zmq(zr, zsock->zs, &(zsock->evt),
            (zreactor_fn_zmq) zsock_handler, zsock);
}

void
zsock_set_zmq_socket(struct zsock_s *zsock, void *zs)
{
    int i, rc, ztype;

    zsock->zs = zs;
    ztype = get_ztype(zs);

    i = ZMQ_DEFAULT_SNDHVM;
    rc = zmq_setsockopt(zs, ZMQ_SNDHWM, &i, sizeof(i));
    g_assert(rc == 0);

    i = ZMQ_DEFAULT_RCVHWM;
    rc = zmq_setsockopt(zs, ZMQ_RCVHWM, &i, sizeof(i));
    g_assert(rc == 0);

    rc = zmq_setsockopt(zs, ZMQ_IDENTITY, zsock->puuid, strlen(zsock->puuid));
    g_assert(rc == 0);

    if (ztype == ZMQ_SUB) {
        // TODO do not subscribe to *any* message, but (maybe) filter on a config
        rc = zmq_setsockopt(zs, ZMQ_SUBSCRIBE, "", 0);
        g_assert(rc == 0);
    }
}

