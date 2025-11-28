#!/usr/bin/python

import os
import sys
import glob
import time

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

class MOSPFTopo(Topo):
    def build(self):
        h1 = self.addHost('h1')
        h2 = self.addHost('h2')
        r1 = self.addHost('r1')
        r2 = self.addHost('r2')
        r3 = self.addHost('r3')
        r4 = self.addHost('r4')

        self.addLink(h1, r1)
        self.addLink(r1, r2)
        self.addLink(r1, r3)
        self.addLink(r2, r4)
        self.addLink(r3, r4)
        self.addLink(r4, h2)

    def disable_link(self, n1, n2):
        for link in list(net.links):  # Iterate over a copy to allow safe removal
            if (link.intf1.node == n1 and link.intf2.node == n2) or \
            (link.intf1.node == n2 and link.intf2.node == n1):

                # Ensure we stop only once
                if getattr(link, '_disabled', False):
                    return True  # Already handled, skip safely

                try:
                    link.stop()  # Stop + delete happens here
                except Exception:
                    pass  # Suppress errors from partial deletion

                # Mark as disabled to avoid future stop/delete attempts
                link._disabled = True

                # Remove from network-managed link list to prevent double stop at net.stop()
                net.links.remove(link)
                return True
        return False


if __name__ == '__main__':
    check_scripts()

    topo = MOSPFTopo()
    net = Mininet(topo = topo, controller = None) 

    h1, h2, r1, r2, r3, r4 = net.get('h1', 'h2', 'r1', 'r2', 'r3', 'r4')
    h1.cmd('ifconfig h1-eth0 10.0.1.11/24')

    r1.cmd('ifconfig r1-eth0 10.0.1.1/24')
    r1.cmd('ifconfig r1-eth1 10.0.2.1/24')
    r1.cmd('ifconfig r1-eth2 10.0.3.1/24')

    r2.cmd('ifconfig r2-eth0 10.0.2.2/24')
    r2.cmd('ifconfig r2-eth1 10.0.4.2/24')

    r3.cmd('ifconfig r3-eth0 10.0.3.3/24')
    r3.cmd('ifconfig r3-eth1 10.0.5.3/24')

    r4.cmd('ifconfig r4-eth0 10.0.4.4/24')
    r4.cmd('ifconfig r4-eth1 10.0.5.4/24')
    r4.cmd('ifconfig r4-eth2 10.0.6.4/24')

    h2.cmd('ifconfig h2-eth0 10.0.6.22/24')

    h1.cmd('route add default gw 10.0.1.1')
    h2.cmd('route add default gw 10.0.6.4')

    for h in (h1, h2):
        h.cmd('./scripts/disable_offloading.sh')
        h.cmd('./scripts/disable_ipv6.sh')

    for r in (r1, r2, r3, r4):
        r.cmd('./scripts/disable_arp.sh')
        r.cmd('./scripts/disable_icmp.sh')
        r.cmd('./scripts/disable_ip_forward.sh')
        r.cmd('./scripts/disable_ipv6.sh')

    net.start()

    h1.cmd('echo Starting mOSPF daemon, waiting 40 seconds... > test.log')

    r1.cmd('tcpdump -i any -w r1.log.pcap &')
    r2.cmd('tcpdump -i any -w r2.log.pcap &')
    r3.cmd('tcpdump -i any -w r3.log.pcap &')
    r4.cmd('tcpdump -i any -w r4.log.pcap &')

    r1.cmd('./mospfd > r1.log 2>&1 &')
    r2.cmd('./mospfd > r2.log 2>&1 &')
    r3.cmd('./mospfd > r3.log 2>&1 &')
    r4.cmd('./mospfd > r4.log 2>&1 &')

    time.sleep(40)

    h1.cmd('echo Start connectivity test >> test.log')

    h1.cmd('echo [Test 1] h1 ping h2: >> test.log')
    h1.cmd('ping -c 3 10.0.6.22 >> test.log') # ping h2 from h1
    h1.cmd('echo >> test.log')

    h2.cmd('echo [Test 2]  h2 ping h1: >> test.log')
    h2.cmd('ping -c 3 10.0.1.11 >> test.log') # ping h1 from h2
    h2.cmd('echo >> test.log')

    h1.cmd('echo [Test 3] h1 traceroute to h2: >> test.log')
    h1.cmd('traceroute 10.0.6.22 >> test.log')
    h1.cmd('echo >> test.log')

    h1.cmd('echo Shutting down r1-r2 link, and waiting 10 seconds... >> test.log')
    topo.disable_link(r1, r2)
    time.sleep(10)

    h1.cmd('echo [Test 4] h1 ping h2: >> test.log')
    h1.cmd('ping -c 3 10.0.6.22 >> test.log')
    h1.cmd('echo >> test.log')

    h2.cmd('echo [Test 5] h2 ping h1: >> test.log')
    h2.cmd('ping -c 3 10.0.1.11 >> test.log')
    h2.cmd('echo >> test.log')

    h1.cmd('echo [Test 6] h1 traceroute to h2: >> test.log')
    h1.cmd('traceroute 10.0.6.22 >> test.log')
    h1.cmd('echo >> test.log')

    h2.cmd('echo [Test 7] h2 traceroute to h1: >> test.log')
    h2.cmd('traceroute 10.0.1.11 >> test.log')
    h2.cmd('echo >> test.log')


    h1.cmd('echo Tests completed, shutting down... >> test.log')

    net.stop()
