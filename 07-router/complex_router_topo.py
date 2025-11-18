#!/usr/bin/python

import os
import sys
import glob

from mininet.topo import Topo
from mininet.net import Mininet
from mininet.cli import CLI

script_deps = [ 'ethtool', 'arptables', 'iptables' ]

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

class ComplexRouterTopo(Topo):
    def build(self):
        # h1 -- r1 -- r2 -- r3 -- h2
        h1 = self.addHost('h1')
        h2 = self.addHost('h2')
        r1 = self.addHost('r1')
        r2 = self.addHost('r2')
        r3 = self.addHost('r3')

        self.addLink(h1, r1)
        self.addLink(r1, r2)
        self.addLink(r2, r3)
        self.addLink(r3, h2)

if __name__ == '__main__':
    check_scripts()

    topo = ComplexRouterTopo()
    net = Mininet(topo = topo, controller = None) 

    h1, h2, r1, r2, r3 = net.get('h1', 'h2', 'r1', 'r2', 'r3')

    # Configure IP addresses and routes
    # h1 --10.0.1.x-- r1 --10.0.3.x-- r2 --10.0.4.x-- r3 --10.0.2.x-- h2

    h1.cmd('ifconfig h1-eth0 10.0.1.11/24')
    h2.cmd('ifconfig h2-eth0 10.0.2.22/24')

    h1.cmd('route add default gw 10.0.1.1')
    h2.cmd('route add default gw 10.0.2.1')

    r1.cmd('ifconfig r1-eth0 10.0.1.1/24')
    r1.cmd('ifconfig r1-eth1 10.0.3.1/24')

    r2.cmd('ifconfig r2-eth0 10.0.3.2/24')
    r2.cmd('ifconfig r2-eth1 10.0.4.2/24')

    r3.cmd('ifconfig r3-eth0 10.0.4.3/24')
    r3.cmd('ifconfig r3-eth1 10.0.2.1/24')

    for n in (h1, h2, r1, r2, r3):
        n.cmd('./scripts/disable_offloading.sh')
        n.cmd('./scripts/disable_ipv6.sh')

    for n in (r1, r2, r3):
        n.cmd('./scripts/disable_arp.sh')
        n.cmd('./scripts/disable_icmp.sh')
        n.cmd('./scripts/disable_ip_forward.sh')

    net.start()

    # Manually configure routing rules on routers
    # h1 --10.0.1.x-- r1 --10.0.3.x-- r2 --10.0.4.x-- r3 --10.0.2.x-- h2
    r1.cmd('route add -net 10.0.4.0 netmask 255.255.255.0 gw 10.0.3.2')
    r1.cmd('route add -net 10.0.2.0 netmask 255.255.255.0 gw 10.0.3.2')
    r2.cmd('route add -net 10.0.2.0 netmask 255.255.255.0 gw 10.0.4.3')
    r2.cmd('route add -net 10.0.1.0 netmask 255.255.255.0 gw 10.0.3.1')
    r3.cmd('route add -net 10.0.1.0 netmask 255.255.255.0 gw 10.0.4.2')
    r3.cmd('route add -net 10.0.3.0 netmask 255.255.255.0 gw 10.0.4.2')

    # Start test

    # Clear previous log files
    r1.cmd('echo > router-r1.log')
    r2.cmd('echo > router-r2.log')
    r3.cmd('echo > router-r3.log')
    h1.cmd('echo > test2.log')

    r1.cmd('./router > router-r1.log 2>&1 &')
    r2.cmd('./router > router-r2.log 2>&1 &')
    r3.cmd('./router > router-r3.log 2>&1 &')

    h1.cmd('echo -e "[Test] h1 ping h2 (10.0.2.22)" > test2.log 2>&1')
    h1.cmd('ping 10.0.2.22 -c 4 >> test2.log 2>&1')

    h1.cmd('echo -e "\n\n=======\n[Test] h2 ping h1 (10.0.1.11)" >> test2.log 2>&1')
    h2.cmd('ping 10.0.1.11 -c 4 >> test2.log 2>&1')

    h1.cmd('echo -e "\n\n=======\n[Test] h1 traceroute h2 (10.0.2.22)" >> test2.log 2>&1')
    h1.cmd('traceroute 10.0.2.22 >> test2.log 2>&1')

    h1.cmd('echo -e "\n\n=======\n[Test] h2 traceroute h1 (10.0.1.11)" >> test2.log 2>&1')
    h2.cmd('traceroute 10.0.1.11 >> test2.log 2>&1')

    net.stop()
