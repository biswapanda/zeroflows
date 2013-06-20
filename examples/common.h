#ifndef ZEROFLOWS_common_h
# define ZEROFLOWS_common_h 1
# include <glib.h>
# include <zmq.h>
# include <zookeeper.h>

# include <zeroflows.h>

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

void main_set_log_handlers(void);


// Environment common to standalone services

struct zsrv_env_s
{
    struct zenv_s zenv;
    struct zservice_s *zsrv;
};

void zsrv_env_init(const gchar *type, struct zsrv_env_s *ctx);

void zsrv_env_close(struct zsrv_env_s *ctx);


// Environment common to clients

struct zclt_env_s
{
    struct zenv_s zenv;
    struct zsock_s *zsock;
};

void zclt_env_init(const gchar *type, const gchar *target,
        struct zclt_env_s *ctx);

void zclt_env_close(struct zclt_env_s *ctx);

#endif // ZEROFLOWS_common_h
