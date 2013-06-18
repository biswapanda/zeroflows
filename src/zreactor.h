#ifndef ZEROFLOWS_zreactor_h
# define ZEROFLOWS_zreactor_h 1

// Zero-Flows, actors plumbing with ZeroMQ & ZooKeeper
// Copyright (C) 2013 Jean-Francois SMIGIELSKI and all the contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

# include <zmq.h>
# include <zookeeper.h>

struct zreactor_s;

typedef int (*zreactor_fn_fd)  (void *u, int fd, int e);

typedef int (*zreactor_fn_zmq) (void *u, void *s, int e);

struct zreactor_s* zreactor_create(void);

void zreactor_destroy(struct zreactor_s *zr);

void zreactor_stop(struct zreactor_s *zr);

int zreactor_run(struct zreactor_s *zr);

void zreactor_add_zk(struct zreactor_s *zr, zhandle_t *zh);

void zreactor_add_fd(struct zreactor_s *zr, int fd, int *evt,
        zreactor_fn_fd fn, gpointer fnu);

void zreactor_add_zmq(struct zreactor_s *zr, void *s, int *evt,
        zreactor_fn_zmq fn, gpointer fnu);

#endif
