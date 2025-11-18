#!/bin/bash

set -e

make 
sudo python router_topo.py
