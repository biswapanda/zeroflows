#ifndef G_LOG_DOMAIN
# define G_LOG_DOMAIN "zsock"
#endif

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

#include "./macros.h"
#include "./zsock.h"

gint
strcmp3(gconstpointer p0, gconstpointer p1, gpointer ignored)
{
    (void) ignored;
    return g_strcmp0((const gchar*)p0, (const gchar*)p1);
}

GError*
zsocket_resolve(const gchar *zname, int *ztype)
{
    static struct named_type_s { const gchar *zname; int ztype; } defs[] = {
        {"PUB", ZMQ_PUB},
        {"SUB", ZMQ_SUB},
        {"PUSH", ZMQ_PUSH},
        {"PULL", ZMQ_PULL},
        {NULL,0}
    };
    
    g_assert(zname != NULL);
    g_assert(ztype != NULL);

    if (!g_str_has_prefix(zname, "zmq:"))
        return NEWERROR(EINVAL, "Invalid ZMQ socket type [%s]", zname);
    zname += sizeof("zmq:")-1;

    for (struct named_type_s *nt = defs; nt->zname ;++nt) {
        if (!g_ascii_strcasecmp(zname, nt->zname)) {
            *ztype = nt->ztype;
            return NULL;
        }
    }

    return NEWERROR(EINVAL, "Invalid ZMQ socket type [%s]", zname);
}

