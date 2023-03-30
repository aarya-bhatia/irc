#!/bin/sh

echo 'starting vm erwin on primary...'
ssh primary 'cd $HOME/irc && git pull && make clean && make && nohup build/server erwin'

