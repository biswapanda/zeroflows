#ifndef G_LOG_DOMAIN
# define G_LOG_DOMAIN "zsock"
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

#include <stdio.h>
#include <string.h>

#include "./common.h"

static void
logger_noop(const gchar *log_domain, GLogLevelFlags log_level,
        const gchar *message, gpointer user_data)
{
    (void) log_domain, (void) log_level;
    (void) message, (void) user_data;
}

static void
logger_stderr(const gchar *log_domain, GLogLevelFlags log_level,
        const gchar *message, gpointer user_data)
{
    GTimeVal tv;
    union {
        GThread *th;
        guint16 u[4];
    } b;

    b.u[0] = b.u[1] = b.u[2] = b.u[3] = 0;
    b.th = NULL;

    b.th = g_thread_self();

    (void) user_data;
    g_get_current_time(&tv);
    fprintf(stdout, "%ld.%03ld %-12s %04X %04X %s\n",
            tv.tv_sec, tv.tv_usec / 1000,
            log_domain ? log_domain : "-",
            log_level,
            ((b.u[0] ^ b.u[1]) ^ b.u[2]) ^ b.u[3],
            message);
}

void
main_set_log_handlers(void)
{
    void set(const gchar *d, GLogLevelFlags f, GLogFunc fn, gpointer u) {
        g_log_set_handler(d, G_LOG_FLAG_RECURSION
            |G_LOG_LEVEL_DEBUG |G_LOG_LEVEL_INFO |G_LOG_LEVEL_MESSAGE
            |G_LOG_LEVEL_WARNING |G_LOG_LEVEL_CRITICAL |G_LOG_LEVEL_ERROR,
            logger_noop, NULL);
        //f &= ~G_LOG_LEVEL_DEBUG;
        g_log_set_handler(d, f, fn, u);
    }
    set("GLib", G_LOG_FLAG_RECURSION|G_LOG_FLAG_FATAL
            |G_LOG_LEVEL_WARNING
            |G_LOG_LEVEL_CRITICAL
            |G_LOG_LEVEL_ERROR,
            logger_stderr, NULL);

    set("zsock", G_LOG_FLAG_RECURSION
            |G_LOG_LEVEL_DEBUG |G_LOG_LEVEL_INFO |G_LOG_LEVEL_MESSAGE
            |G_LOG_LEVEL_WARNING |G_LOG_LEVEL_CRITICAL |G_LOG_LEVEL_ERROR,
            logger_stderr, NULL);

    set("ZK", G_LOG_FLAG_RECURSION
            |G_LOG_LEVEL_INFO |G_LOG_LEVEL_MESSAGE
            |G_LOG_LEVEL_WARNING |G_LOG_LEVEL_CRITICAL |G_LOG_LEVEL_ERROR,
            logger_stderr, NULL);

    g_log_set_default_handler(logger_stderr, NULL);
}

void
zsrv_env_init(const gchar *type, struct zsrv_env_s *ctx)
{
    zenv_init(&ctx->zenv);
    ctx->zsrv = zservice_create(&ctx->zenv, type);
    g_assert(ctx->zsrv != NULL);
}

void
zsrv_env_close(struct zsrv_env_s *ctx)
{
    g_assert(ctx != NULL);
    zservice_destroy(ctx->zsrv);
    zenv_close(&ctx->zenv);
}

void
zclt_env_init(const gchar *typename, const gchar *target, struct zclt_env_s *ctx)
{
    g_assert(target != NULL);
    g_assert(ctx != NULL);

    zenv_init(&ctx->zenv);
    ctx->zsock = zenv_create_socket(&ctx->zenv, "client", typename);
    g_assert(ctx->zsock != NULL);

    zsock_connect(ctx->zsock, target, "all");
}

void
zclt_env_close(struct zclt_env_s *ctx)
{
    g_assert(ctx != NULL);
    zsock_destroy(ctx->zsock);
    zenv_close(&ctx->zenv);
}

