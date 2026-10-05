FROM ubuntu:26.04@sha256:f3d28607ddd78734bb7f71f117f3c6706c666b8b76cbff7c9ff6e5718d46ff64

COPY install/bin /install/bin
RUN chmod +x /install/bin/*

ENTRYPOINT ["/install/bin/translate"]
