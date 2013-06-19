#ifndef ZEROFLOWS_macros_h
# define ZEROFLOWS_macros_h 1

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

# ifdef HAVE_ASSERT
#  define ASSERT(X) g_assert(X)
# else
#  define ASSERT(X)
# endif

#ifndef GQUARK
# define GQUARK() g_quark_from_static_string(G_LOG_DOMAIN)
#endif

#ifndef NEWERROR
# define NEWERROR(Code,Fmt,...) g_error_new(GQUARK(), (Code), (Fmt), ##__VA_ARGS__)
#endif

#define ZK_DEBUG(FMT,...) g_log("ZK", G_LOG_LEVEL_DEBUG, FMT, ##__VA_ARGS__)

#endif // ZEROFLOWS_macros_h