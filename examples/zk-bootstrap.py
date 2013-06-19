#!/usr/bin/python

# Zero-Flows, actors plumbing with ZeroMQ & ZooKeeper
# Copyright (C) 2013 Jean-Francois SMIGIELSKI and all the contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import zookeeper
import sys
import json

def new_acl_openbar():
    return [{'perms':zookeeper.PERM_ALL, 'scheme':'world', 'id':'anyone'}]

def get_config(f):
    with open(f, 'r') as fh:
        content = fh.read()
        return json.loads(content)

def validate_config(cfg):
    if 'name' not in cfg:
        raise Exception('type has no name')
    if 'sockets' not in cfg:    
        raise Exception('no socket defined')
    for s in cfg['sockets']:
        if 'name' not in s:
            raise Exception('socket has no name')
        if 'type' not in s:
            raise Exception('socket has no type')
        if 'bind' not in s and 'connect' not in s:
            raise Exception('socket has no connect/bind')
    return cfg

def ensure_node(zh, path, content):
    try: # Ensure the node exists
        zookeeper.create(zh, path, content, new_acl_openbar(), 0)
    except zookeeper.NodeExistsException as e:
        zookeeper.set(zh, path, content)

def manage_type(zh, f):
    cfg = validate_config(get_config(f))
    ensure_node(zh, '/services/'+str(cfg['name']), json.dumps(cfg))
    for s in cfg['sockets']: # Ensure each socket exist
        ensure_node(zh, '/listen/'+str(cfg['name'])+'.'+str(s['name']), '')

def ensure_basedirs(zh):
    ensure_node(zh, '/services', '')
    ensure_node(zh, '/listen', '')

def main():
    zookeeper.set_debug_level(zookeeper.LOG_LEVEL_WARN)
    zh = zookeeper.init('127.0.0.1:2181')
    ensure_basedirs(zh)
    for f in sys.argv[1:]:
        try:
            manage_type(zh, f)
            print "OK", f
        except Exception as e:
            print "KO", f, e
    zookeeper.close(zh)

if __name__ == '__main__':
    main()

