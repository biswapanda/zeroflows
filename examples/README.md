# Testing


## User's HOWTO

With **Zero-Flows**, we would be things to be as simple as :

1. **Develop the logic** for each *service type*, e.g. consuming the inputs and produce some output
2. **Configure the service types** in [Zookeeper][ZK] : associate a service name to named *endpoints*, and for each *endpoint* tell how it behaves on the wetwork (e.g. bind and/or connect).
3. **Deploy services**, while associating them to their *service type* as named in [Zookeeper][ZK]. The configuration in 


## Pre-requisites

Ensure you have at least the libraries required by **Zero-Flows** itself (see ../README.md).
In addition, if you follow this guide, you will need the [Zookeeper][ZK] python binding,
as provided as a contribution with the official distribution).

For this test, we will use [Monython](https://github.com/jfsmig/monython) to ease the deployment.
In a few words, 2 CLI tools are provided: **monythond** as a monitoring script or non-daemons,
and **monythonc** to manage the **monythond** behavior.

Eventually, you will need at least a standalone instance of [Zookeeper][ZK].
Let's consider there is one up and running, bond to 127.0.0.1:2181.


## Let's test

Let's buld a straight workflow as ' zpush >> pull2pub << sub2push << zpull ''.
The arrows in that description do not represent the flow direction but the active/passive roles in the connections
(the pointed service type is the server).

### Develop

This has been done for you in the current directory. 5 CLI tools are proposed.

* zpush / zpub $target : read standard input and produce messages in a connected zsocket.
* zpull / zsub $target : consume messages from a connected socket and dump them to the standard output.
* zservice $type : consume messages from the input named "in", and replicate them to the output named "out". 

### Configure

Now it is time to declare service types in [Zookeeper][ZK]. We prepared 2 configuration files.

    { "name": "pull2pub",
      "sockets": [
        { "name": "in",
          "type": "zmq:PULL",
          "bind": [ "tcp://*:0" ]
        },
        { "name": "out",
          "type": "zmq:PUB",
          "bind": [ "tcp://*:0" ]
        }
      ]}

And

    { "name": "sub2push",
      "sockets": [
        { "name": "in",
          "type": "zmq:SUB",
          "connect": { "pull2pub.out": "all" },
          "bind": [ "tcp://*:0" ]
        },
        { "name": "out",
          "type": "zmq:PUSH",
          "bind": [ "tcp://*:0" ]
        }
      ]}

Now, we declare those types in our [Zookeeper][ZK] with the following script (here provided):

    # python ./zk-bootstrap.py pull2pub.json sub2push.json
    OK pull2pub.json
    OK sub2push.json
    # cli_st 127.0.0.1:2181
    Watcher SESSION_EVENT state = CONNECTED_STATE
    Got a new session id: 0x13f632a6b01003c
    ls /services
    time = 0 msec
    /services: rc = 0
        sub2push
        pull2pub
    time = 0 msec


### Deploy

First, we start the monythond
    
    # monythond -D ipc:///tmp/monythond.sock
    
Now, we declare our *services types*.

    # echo '{"cmd":"zservice pull2pub","env":{},"uid":1000,"gid":1000}' | monythonc DEFINE pull2pub
    
    # echo '{"cmd":"zservice sub2push","env":{},"uid":1000,"gid":1000}' | monythonc DEFINE sub2push
    
    # monythonc PROGS
    {"status": 200}
    {"args": ["zservice", "pull2pub"], "command": "zservice pull2pub", "env": {}, "gid": "1000", "key": "pull2pub", "uid": "1000"}
    {"args": ["zservice", "sub2push"], "command": "zservice sub2push", "env": {}, "gid": "1000", "key": "sub2push", "uid": "1000"}

Then, it is time to really spawn services processes.

    # for i in $(seq 5) ; do monythonc DEPLOY pull2pub ; done
    
    # for i in $(seq 5) ; do monythonc DEPLOY sub2push ; done
    
    # monythonc LIST
    {"status": 200}
    {"descr": "pull2pub", "key": "7160116A744D45AF81613D7EC69F3BA0", "ready": false, "type": "ProcessPersistant", "up": true}
    {"descr": "pull2pub", "key": "A8FC82B8C81A482D96AFC2DD51A5E88B", "ready": false, "type": "ProcessPersistant", "up": true}
    {"descr": "pull2pub", "key": "664C9C3BA199487EB5727000A9442F88", "ready": false, "type": "ProcessPersistant", "up": true}
    {"descr": "sub2push", "key": "9873B74F4F7A432B9C566B0EDBBCA0DB", "ready": false, "type": "ProcessPersistant", "up": true}
    {"descr": "pull2pub", "key": "1D2B52BD81884174A1F46E4B7883A8BD", "ready": false, "type": "ProcessPersistant", "up": true}
    {"descr": "pull2pub", "key": "C187B06048184095B4A989B5E00CBF2B", "ready": false, "type": "ProcessPersistant", "up": true}
    {"descr": "sub2push", "key": "7076CBF192AB41E69F33543DC8481EBC", "ready": false, "type": "ProcessPersistant", "up": true}
    {"descr": "sub2push", "key": "14BF75A10F7D4383955861A397CB56F6", "ready": false, "type": "ProcessPersistant", "up": true}
    {"descr": "sub2push", "key": "A814178883BA47DCAF36A70E1FE9DBB7", "ready": false, "type": "ProcessPersistant", "up": true}
    {"descr": "sub2push", "key": "136224A1DC2847AA8573D23B952230E3", "ready": false, "type": "ProcessPersistant", "up": true}
    
    # ps fs
    30454 pts/1    Sl+    0:02              |   \_ /usr/bin/monythond ipc:///tmp/monythond.sock
    30521 pts/1    Sl+    0:00              |       \_ zservice pull2pub
    30531 pts/1    Sl+    0:00              |       \_ zservice pull2pub
    30541 pts/1    Sl+    0:00              |       \_ zservice pull2pub
    30551 pts/1    Sl+    0:00              |       \_ zservice pull2pub
    30560 pts/1    Sl+    0:00              |       \_ zservice pull2pub
    30572 pts/1    Sl+    0:00              |       \_ zservice sub2push
    30582 pts/1    Sl+    0:00              |       \_ zservice sub2push
    30592 pts/1    Sl+    0:00              |       \_ zservice sub2push
    30602 pts/1    Sl+    0:00              |       \_ zservice sub2push
    30611 pts/1    Sl+    0:00              |       \_ zservice sub2push

### Test!



[ZK]: http://zookeeper.apache.org "Apache Zookeeper"
