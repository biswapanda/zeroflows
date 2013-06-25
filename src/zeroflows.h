#ifndef ZEROFLOWS_zeroflows_h
# define ZEROFLOWS_zeroflows_h 1

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

# include <glib.h>
# include <zookeeper.h>
# include <zmq.h>

//
gint strcmp3(gconstpointer p0, gconstpointer p1, gpointer ignored);

// Transform the string form of the ZMQ type to the ZMQ_* constant
GError* zsocket_resolve(const gchar *zname, int *ztype);

//------------------------------------------------------------------------------

struct cfg_listen_s
{
    gchar *type;
    gchar *ztype;
    gchar *uuid;
    gchar *cell;
    gchar *url;
};

struct cfg_sock_s
{
    gchar *sockname;
    gchar *ztype;
    gchar **connect; // pairs of char*
    gchar **listen; // char*
};

struct cfg_srv_s
{
    gchar *srvtype;
    GPtrArray *socks; // (struct cfg_sock_s *)
};

void cfg_listen_destroy(struct cfg_listen_s *cfg);
void cfg_sock_destroy(struct cfg_sock_s *cfg);
void cfg_srv_destroy(struct cfg_srv_s *cfg);

struct cfg_listen_s * zlisten_parse_config_buffer(const gchar *b, gsize blen);
struct cfg_srv_s * zservice_parse_config_buffer(const gchar *b, gsize bl);
struct cfg_srv_s * zservice_parse_config_string(const gchar *cfg);
struct cfg_srv_s* zservice_parse_config_from_path(const gchar *path);

//------------------------------------------------------------------------------

struct zreactor_s;

/**
 * TODO document
 */
struct zsock_s
{
    void *zs; // ZMQ socket
    zhandle_t *zh; // ZooKeeper handle

    gchar *localname;
    gchar *fullname; // often, srvtype . localname
    const gchar *puuid;
    const gchar *pcell;

    GTree *connect_real; // char* -> gulong
    GTree *connect_cfg; // char* -> (struct zconnect_s*)
    GTree *bind_set; // char* -> char*

    void (*ready_out)(struct zsock_s*);
    void (*ready_in)(struct zsock_s*);
    int evt; // to be monitored ZMQ_POLLIN|ZMQ_POLLOUT
};

/**
 * TODO document
 */
struct zservice_s
{
    void *zctx; // a ZMQ context 
    struct zreactor_s *zr; // the reactor managing this reactor
    zhandle_t *zh; // the ZooKeeper handle

    const gchar *puuid;
    const gchar *pcell;

    gpointer on_config_data;
    void (*on_config)(struct zservice_s *zsrv, gpointer data);

    gchar *srvtype;
    GTree *socks;
};

/**
 * One structure to gather all the mandatory elements of a working context,
 * and to easily create zsock's and zservice's.
 */
struct zenv_s
{
    zhandle_t *zh; // ZooKeeper handle
    void *zctx; // ZeroMQ context
    struct zreactor_s *zr;

    gchar *uuid;
    gchar *cell;
};

//------------------------------------------------------------------------------

struct zsock_s* zsock_create(const gchar *uuid, const gchar *cell);

void zsock_set_zmq_socket(struct zsock_s *zsock, void *zs);

void zsock_destroy(struct zsock_s *zsock);

gboolean zsock_ready(struct zsock_s *zsock);

void zsock_configure(struct zsock_s *zsock, struct cfg_sock_s *cfg);

void zsock_register_in_reactor(struct zreactor_s *zr, struct zsock_s *zsock);

void zsock_connect(struct zsock_s *zsock, const gchar *type,
        const gchar *policy);

//------------------------------------------------------------------------------

struct zservice_s* zservice_create(struct zenv_s *zenv, const gchar *type);

void zservice_destroy(struct zservice_s *zsrv);

struct zsock_s* zservice_get_socket(struct zservice_s *zsrv, const gchar *n);

void zservice_reload_configuration(struct zservice_s *zsrv);

void zservice_on_config(struct zservice_s *zsrv, gpointer u,
        void (*hook)(struct zservice_s*, gpointer));

//------------------------------------------------------------------------------

typedef int (*zreactor_fn_fd)  (void *u, int fd, int e);

typedef int (*zreactor_fn_zmq) (void *u, void *s, int e);

struct zreactor_s* zreactor_create(void);

void zreactor_destroy(struct zreactor_s *zr);

void zreactor_stop(struct zreactor_s *zr);

int zreactor_run(struct zreactor_s *zr);

void zreactor_add_zk(struct zreactor_s *zr, void *zh);

void zreactor_add_fd(struct zreactor_s *zr, int fd, int *evt,
        zreactor_fn_fd fn, gpointer fnu);

void zreactor_add_zmq(struct zreactor_s *zr, void *s, int *evt,
        zreactor_fn_zmq fn, gpointer fnu);

//------------------------------------------------------------------------------

void zenv_init(struct zenv_s *zenv);

void zenv_close(struct zenv_s *zenv);

struct zsock_s* zenv_create_socket(struct zenv_s *zenv, const gchar *name,
        const gchar *ztype);

#endif // ZEROFLOWS_zeroflows_h
