Zero-Flows
==========

**Zero-Flows** is thin wrapper around **ZeroMQ**, **Apache ZooKeeper** and
**Jansson**. It helps building versatile distributed workflows with a central
configuration.

The considered workflows are chains of interconnected "steps", when each
step is managed by several instances of the same service type. It is
designed to scale gracefully because each "step" can be scaled independently.

At a **Zero-Flows** point of view, a service is a set of public *endpoints*,
some bond to network adresses, some connected to other endpoints. It does not
matter the service is in fact a {single,multi}-threaded (group of) process(es).

The configuration of the whole chain is centralized in the **ZooKeeper**, so
that the administrator's main task is to provision enough services to hold
the load for each step.

This is not a competitor for Actors frameworks as Akka (even though it
achieves the same goal), because it is much more raw and simplistic.


License
-------

**Zero-Flows** belongs to Jean-François SMIGIELSKI and all the contributors
to the project.

**Zero-Flows** is distributed under the terms of the GNU Affero GPL
(http://www.gnu.org/licenses/agpl.html).


Third-party libraries
---------------------

* **ZeroMQ** 3.2.3 (http://zeromq.org) provides a taste a brokerless BUS.
The elementary transport unit is a ZeroMQ message, and **Zero-Flows**
lets you benefit of PUSH/PULL and PUB/SUB patterns of ZeroMQ.

* **Apache ZooKeeper** 3.4.5 (http://zookeper.apache.org) is used to configure the
nodes and discover the peer nodes. We onlty use the single-threaded C client
API provided with the official Zookeeper distribution.

* **Jansson** to parse the JSON format used into **Zookeeper**.

* **Gnome Library** (https://developer.gnome.org/glib/). It provides
easy-to-use data structures and a lot of necessary features.


Installation
------------

**Zero-Flows** is built with the *cmake* and *make*, and relies on *pkg-config* to
locate **ZeroMQ** and **Jansson**.

    [nobody@localhost ~/public_git]$ git clone https://github.com/jfsmig/zeroflows.git
    [nobody@localhost ~/public_git]$ cd /tmp
    [nobody@localhost /tmp]$ cmake \
            -DZK_LIBDIR=${ZK_BASEDIR}/lib64 -DZK_INCDIR=${ZK_BASEDIR}/include \
            -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_INSTALL_PREFIX=/tmp \
            ~/public_git/zeroflows
    [nobody@localhost /tmp]$ make install

Feel free to read the *cmake* documentation at http://cmake.org if you want to
a complete information about the configuration directives currently available.


TODO
----

This is still an early release. There is a lot to do to make it easy to use.

