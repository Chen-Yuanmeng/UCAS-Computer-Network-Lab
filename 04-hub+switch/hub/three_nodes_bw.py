#!/usr/bin/python

import os
import sys
import glob

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
        h3 = self.addHost('h3')
        b1 = self.addHost('b1')

        self.addLink(h1, b1, bw=20)
        self.addLink(h2, b1, bw=10)
        self.addLink(h3, b1, bw=10)

if __name__ == '__main__':
    check_scripts()

    topo = BroadcastTopo()
    net = Mininet(topo = topo, link = TCLink, controller = None) 

    h1, h2, h3, b1 = net.get('h1', 'h2', 'h3', 'b1')
    h1.cmd('ifconfig h1-eth0 10.0.0.1/8')
    h2.cmd('ifconfig h2-eth0 10.0.0.2/8')
    h3.cmd('ifconfig h3-eth0 10.0.0.3/8')
    clearIP(b1)

    for h in [ h1, h2, h3, b1 ]:
        h.cmd('./scripts/disable_offloading.sh')
        h.cmd('./scripts/disable_ipv6.sh')

    net.start()

    # Start Hub
    print("Starting Hub...", end=' ')
    b1.cmd('./hub &')
    print("Hub started")
    
    # PING Test
    
    h1.cmd('ping -c 4 10.0.0.2 > output/ping/h1_h2.txt')
    h1.cmd('ping -c 4 10.0.0.3 > output/ping/h1_h3.txt')
    h2.cmd('ping -c 4 10.0.0.1 > output/ping/h2_h1.txt')
    h2.cmd('ping -c 4 10.0.0.3 > output/ping/h2_h3.txt')
    h3.cmd('ping -c 4 10.0.0.1 > output/ping/h3_h1.txt')
    h3.cmd('ping -c 4 10.0.0.2 > output/ping/h3_h2.txt')
    print("PING tests completed")

    # IPERF Test
    print("Starting IPERF tests...")

    # Test 1: h1 (client) -> h2, h3 (servers)
    print("Test 1: h1 (client) -> h2, h3 (servers)")
    h2.cmd('iperf -s &')
    h3.cmd('iperf -s &')
    h1.cmd('iperf -c 10.0.0.2 -t 30 > output/iperf/1.h2.txt &')
    h1.cmd('iperf -c 10.0.0.3 -t 30 > output/iperf/1.h3.txt')  # No & to wait for completion

    h2.cmd('pkill iperf')
    h3.cmd('pkill iperf')

    # Test 2: h2, h3 (clients) -> h1 (server)
    print("Test 2: h2, h3 (clients) -> h1 (server)")
    h1.cmd('iperf -s &')
    h2.cmd('iperf -c 10.0.0.1 -t 30 > output/iperf/2.h2.txt &')
    h3.cmd('iperf -c 10.0.0.1 -t 30 > output/iperf/2.h3.txt')  # No & to wait for completion

    h1.cmd('pkill iperf')

    print("IPERF tests completed")

    net.stop()
