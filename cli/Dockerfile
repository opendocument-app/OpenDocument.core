FROM ubuntu:24.04@sha256:186072bba1b2f436cbb91ef2567abca677337cfc786c86e107d25b7072feef0c

RUN mkdir -p /install/bin
ADD install/bin /install/bin

ENTRYPOINT /install/bin/translate
