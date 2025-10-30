#!/usr/bin/python

import os
import sys
import glob
import time

from mininet.topo import Topo
from mininet.net import Mininet
from mininet.cli import CLI

script_deps = [ 'ethtool' ]

def check_scripts():
    dir = os.path.abspath(os.path.dirname(sys.argv[0]))
    
    for fname in glob.glob(dir + '/' + 'scripts/*.sh'):
        if not os.access(fname, os.X_OK):
            print('%s should be set executable by using `chmod +x $script_name`' % (fname))
            sys.exit(1)

    for program in script_deps:
        found = False
        for path in os.environ['PATH'].split(os.pathsep):
            exe_file = os.path.join(path, program)
            if os.path.isfile(exe_file) and os.access(exe_file, os.X_OK):
                found = True
                break
        if not found:
            print('`%s` is required but missing, which could be installed via `apt` or `aptitude`' % (program))
            sys.exit(2)

def clearIP(n):
    for iface in n.intfList():
        n.cmd('ifconfig %s 0.0.0.0' % (iface))

class RingTopo(Topo):
    def build(self):
        b1 = self.addHost('b1')
        b2 = self.addHost('b2')
        b3 = self.addHost('b3')
        b4 = self.addHost('b4')

        self.addLink(b1, b2)
        self.addLink(b1, b3)
        self.addLink(b2, b4)
        self.addLink(b3, b4)

if __name__ == '__main__':
    check_scripts()

    topo = RingTopo()
    net = Mininet(topo = topo, controller = None) 

    for idx in range(4):
        name = 'b' + str(idx+1)
        node = net.get(name)
        clearIP(node)
        node.cmd('./scripts/disable_offloading.sh')
        node.cmd('./scripts/disable_ipv6.sh')

        # set mac address for each interface
        for port in range(len(node.intfList())):
            intf = '%s-eth%d' % (name, port)
            mac = '00:00:00:00:0%d:0%d' % (idx+1, port+1)

            node.setMAC(mac, intf = intf)

        # node.cmd('./stp > %s-output.txt 2>&1 &' % name)
        # node.cmd('./stp-reference > %s-output.txt 2>&1 &' % name)

    net.start()
    
    # Start stp on four nodes
    b1, b2, b3, b4 = net.get('b1', 'b2', 'b3', 'b4')

    b1.cmd('./stp > b1-output.txt 2>&1 &')
    b2.cmd('./stp > b2-output.txt 2>&1 &')
    b3.cmd('./stp > b3-output.txt 2>&1 &')
    b4.cmd('./stp > b4-output.txt 2>&1 &')
    print("STP is running on four nodes, start 30 seconds wait...")

    time.sleep(10)


    b1.cmd('pkill -SIGTERM stp')
    b2.cmd('pkill -SIGTERM stp')
    b3.cmd('pkill -SIGTERM stp')
    b4.cmd('pkill -SIGTERM stp')

    os.system('./dump_output.sh 4 > output.txt')

    net.stop()
