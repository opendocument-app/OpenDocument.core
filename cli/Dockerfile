FROM ubuntu:26.04@sha256:5e275723f82c67e387ba9e3c24baa0abdcb268917f276a0561c97bef9450d0b4

RUN mkdir -p /install/bin
ADD install/bin /install/bin

ENTRYPOINT /install/bin/translate
