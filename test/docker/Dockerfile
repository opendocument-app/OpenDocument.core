FROM ubuntu:22.04

ENV FIREFOX_VERSION="120.0"
ENV CHROME_VERSION="119.0.6045.159-1"
ENV PHANTOMJS_VERSION="2.1.1"
ENV GECKODRIVER_VERSION="0.33.0"
ENV CHROMEDRIVER_VERSION="119.0.6045.105"

ENV INSTALL="apt-get install -y --no-install-recommends"

RUN apt-get update

# install download and unpack utilities
RUN $INSTALL wget ca-certificates bzip2 unzip

# firefox setup
RUN $INSTALL libgtk-3-0 libasound2 libx11-xcb1

RUN wget https://download-installer.cdn.mozilla.net/pub/firefox/releases/${FIREFOX_VERSION}/linux-x86_64/en-US/firefox-${FIREFOX_VERSION}.tar.bz2
RUN tar -xf firefox-${FIREFOX_VERSION}.tar.bz2
RUN mv firefox /usr/local/share
RUN ln -s /usr/local/share/firefox/firefox /usr/local/bin
RUN rm firefox-${FIREFOX_VERSION}.tar.bz2

RUN firefox --version

# geckodriver setup
RUN wget https://github.com/mozilla/geckodriver/releases/download/v${GECKODRIVER_VERSION}/geckodriver-v${GECKODRIVER_VERSION}-linux64.tar.gz
RUN tar -xf geckodriver-v${GECKODRIVER_VERSION}-linux64.tar.gz
RUN mv geckodriver /usr/local/bin
RUN rm geckodriver-v${GECKODRIVER_VERSION}-linux64.tar.gz

RUN geckodriver --version

# chrome setup
RUN wget https://dl.google.com/linux/deb/pool/main/g/google-chrome-stable/google-chrome-stable_${CHROME_VERSION}_amd64.deb
RUN $INSTALL ./google-chrome-stable_${CHROME_VERSION}_amd64.deb
RUN rm google-chrome-stable_${CHROME_VERSION}_amd64.deb

RUN google-chrome --version

# chromedirver setup
RUN wget https://edgedl.me.gvt1.com/edgedl/chrome/chrome-for-testing/${CHROMEDRIVER_VERSION}/linux64/chromedriver-linux64.zip
RUN unzip chromedriver-linux64.zip
RUN mv chromedriver-linux64/chromedriver /usr/local/bin
RUN rm -rf chromedriver-linux64
RUN rm chromedriver-linux64.zip

RUN geckodriver --version

# phantomjs setup
RUN $INSTALL build-essential chrpath libssl-dev libxft-dev
RUN $INSTALL libfreetype6 libfreetype6-dev libfontconfig1 libfontconfig1-dev

RUN wget https://bitbucket.org/ariya/phantomjs/downloads/phantomjs-${PHANTOMJS_VERSION}-linux-x86_64.tar.bz2
RUN tar -xf phantomjs-${PHANTOMJS_VERSION}-linux-x86_64.tar.bz2
RUN mv phantomjs-${PHANTOMJS_VERSION}-linux-x86_64/bin/phantomjs /usr/local/bin
RUN rm -rf phantomjs-${PHANTOMJS_VERSION}-linux-x86_64
RUN rm phantomjs-${PHANTOMJS_VERSION}-linux-x86_64.tar.bz2

# fix weird phantomjs issue
ENV OPENSSL_CONF=/etc/ssl

RUN phantomjs --version

# install python dependencies
ADD requirements.txt .
RUN $INSTALL python3 python3-pip
RUN pip install --upgrade pip
RUN pip install -r requirements.txt
RUN rm requirements.txt
