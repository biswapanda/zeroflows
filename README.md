## Summary

### Versatile, distributed & scalable messages flows.

This is what **ZeroFlows** brings us!

* **Messages flows** : we build directed graphs (potentially cyclic), and each step of the flow is managed by a *service type*. [ZeroMQ](http://zeromq.org) is the key component for the transport part.
* **Scalable** : each step of the flow is managed by several *services* of the same *service type*, and you can deploy as many *services* you need to afford the load at this step.
* **Versatile** : the interconnection is continuously monitored by each *service*, for each *service type*, and adapted if a change is noticed.

A *service* is a set of public *endpoints*, some bond to network adresses, some connected to other endpoints. In facts it does not matter the number of threads / processes / hosts hosting a *service*.

This is not a competitor for Actors frameworks as [Akka](http://akka.io) (even if it achieves the same goal), because it is much more raw and simplistic.

![Banner](http://jfsmig.github.io/zeroflows/images/banner.svg)

## License

**Zero-Flows** belongs to Jean-François SMIGIELSKI and all the contributors to the project, and is distributed under the terms of the [GNU Lesser General Public License](http://www.gnu.org/licenses/agpl.html).

![License](http://www.gnu.org/graphics/license-logos-by-christian-candena-cc-by.svg)


## Third-party dependencies

* [ZeroMQ](http://zeromq.org) >= 3.2.3
* [Apache ZooKeeper](http://zookeeper.apache.org) >= 3.4
* [Jansson](http://www.digip.org/jansson/) >= 2.4
* [Gnome library 2](https://developer.gnome.org/glib/) >= 2.30

[ZeroMQ](http://zeromq.org) provides a taste of a brokerless BUS, the elementary transport unit is a message.
**Zero-Flows** lets you benefit of PUSH/PULL (1 to Any) and PUB/SUB (1 to Any) patterns of communcation.
[Apache ZooKeeper](http://zookeeper.apache.org) is used to configure the nodes and discover the peer nodes.
We only use the single-threaded C client API provided with the official Zookeeper distribution.
[Jansson](http://www.digip.org/jansson/) is used to parse the JSON format used into Zookeeper.
The [Gnome library 2](https://developer.gnome.org/glib/) provides easy-to-use data structures and a lot of necessary features.


## Installation

**Zero-Flows** is built with the cmake and make, and relies on pkg-config to locate ZeroMQ and Jansson.

    [nobody@localhost ~/public_git]$ git clone https://github.com/jfsmig/zeroflows.git
    [nobody@localhost ~/public_git]$ cd /tmp
    [nobody@localhost /tmp]$ cmake \
            -DZK_LIBDIR=${ZK_BASEDIR}/lib64 -DZK_INCDIR=${ZK_BASEDIR}/include \
            -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_INSTALL_PREFIX=/tmp \
            ~/public_git/zeroflows
    [nobody@localhost /tmp]$ make install

Feel free to read the [cmake documentation](http://cmake.org) if you want to a complete information about the configuration directives currently available.

