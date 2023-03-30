#!/bin/sh

ssh 192.168.64.4 'cd $HOME/irc && git pull && make clean && make && nohup build/server erwin'

ssh azure 'cd $HOME/irc && build/server azure-vm1'
echo 'ok'
