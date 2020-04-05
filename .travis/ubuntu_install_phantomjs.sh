#!/usr/bin/env bash
set -e

PHANTOM_VERSION="phantomjs-2.1.1"
ARCH=$(uname -m)
PHANTOM_JS="$PHANTOM_VERSION-linux-$ARCH"

sudo apt-get install -y build-essential chrpath libssl-dev libxft-dev libfreetype6 libfreetype6-dev libfontconfig1 libfontconfig1-dev

cd
wget https://bitbucket.org/ariya/phantomjs/downloads/$PHANTOM_JS.tar.bz2
sudo tar xvjf $PHANTOM_JS.tar.bz2

sudo mv $PHANTOM_JS /usr/local/share
sudo ln -sf /usr/local/share/$PHANTOM_JS/bin/phantomjs /usr/local/bin
