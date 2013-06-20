#ifndef G_LOG_DOMAIN
# define G_LOG_DOMAIN "zs.cli"
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

#include "./common_pipe_in.h"

int
main(int argc, char **argv)
{
    freopen("/dev/null", "r", stdin);
    return main_common_pipe_in("zmq:SUB", argc, argv);
}

