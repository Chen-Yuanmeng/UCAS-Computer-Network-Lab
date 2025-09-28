from mininet.net import Mininet
from mininet.topo import Topo
from mininet.link import TCLink
from mininet.node import OVSBridge
import subprocess
import time

class MyTopo(Topo):
    def build(self, **kwargs):
        bw = kwargs.get('bw', 10)
        delay = kwargs.get('delay', '5ms')

        h1 = self.addHost('h1')
        h2 = self.addHost('h2')
        self.addLink(h1, h2, bw=bw, delay=delay)  # 可调节参数，bw单位为Mbps

bws = [10, 30, 50, 100, 300, 500, 800, 1000]  # 增加了一些带宽值
delays = ['100ms']
sizes = ['1M', '10M', '100M']

for size in sizes:
    subprocess.run(f'dd if=/dev/zero of={size}B.dat bs={size} count=1', shell=True)

for retry in range(1, 7):
    for bw in bws:
        for delay in delays:
            for size in sizes:
                topo = MyTopo(bw=bw, delay=delay)
                net = Mininet(topo=topo, switch=OVSBridge, link=TCLink, controller=None)

                net.start()
                h1 = net.get('h1')
                h2 = net.get('h2')
                h2.cmd('python3 -m http.server 80 &')
                h1.cmd(f'wget http://10.0.0.2/{size}B.dat -o {bw}_{delay}_{size}_{str(retry)}.log -O /dev/null')
                h2.cmd('kill %python3')
                net.stop()

                time.sleep(1)
