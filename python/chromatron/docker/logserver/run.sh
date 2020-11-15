#!/usr/bin/env bash

sudo docker run -d --restart=always --network=host --name logserver sapphire_logserver:latest
