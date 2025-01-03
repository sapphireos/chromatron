FROM grafana/grafana:11.4.0-ubuntu

USER root

RUN apt update

# for mDNS:
ARG DEBIAN_FRONTEND=noninteractive
RUN apt install -y avahi-utils 

USER 1000
