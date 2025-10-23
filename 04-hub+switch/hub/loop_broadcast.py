#!/usr/bin/python

import os
import sys
import glob
import time

from mininet.topo import Topo
from mininet.net import Mininet
from mininet.link import TCLink

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

# Mininet will assign an IP address for each interface of a node 
# automatically, but hub or switch does not need IP address.
def clearIP(n):
    for iface in n.intfList():
        n.cmd('ifconfig %s 0.0.0.0' % (iface))

class BroadcastTopo(Topo):
    def build(self):
        h1 = self.addHost('h1')
        h2 = self.addHost('h2')
        b1 = self.addHost('b1')
        b2 = self.addHost('b2')
        b3 = self.addHost('b3')

        self.addLink(h1, b1, bw=100)
        self.addLink(h2, b2, bw=100)
        self.addLink(b1, b3, bw=50)
        self.addLink(b2, b3, bw=50)
        self.addLink(b1, b2, bw=50)

if __name__ == '__main__':
    check_scripts()

    topo = BroadcastTopo()
    net = Mininet(topo = topo, link = TCLink, controller = None) 

    h1, h2, b1, b2, b3 = net.get('h1', 'h2', 'b1', 'b2', 'b3')
    h1.cmd('ifconfig h1-eth0 10.0.0.1/8')
    h2.cmd('ifconfig h2-eth0 10.0.0.2/8')
    clearIP(b1)
    clearIP(b2)
    clearIP(b3)

    for h in [ h1, h2, b1, b2, b3 ]:
        h.cmd('./scripts/disable_offloading.sh')
        h.cmd('./scripts/disable_ipv6.sh')

    net.start()

    # Start 3 hubs for broadcast
    b1.cmd('./hub &')
    b2.cmd('./hub &')
    b3.cmd('./hub &')

    # Start capture
    h1.cmd('tcpdump -i h1-eth0 -w h1_capture.pcap &')
    h2.cmd('tcpdump -i h2-eth0 -w h2_capture.pcap &')
    
    # Send a packet
    h1.cmd('ping -c 1 10.0.0.2')

    # Wait 0.1s and stop capture
    time.sleep(0.1)
    h1.cmd('pkill tcpdump')
    h2.cmd('pkill tcpdump')

    net.stop()
