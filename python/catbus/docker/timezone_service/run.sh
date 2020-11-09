#!/usr/bin/env bash

sudo docker run -d --restart=always -v /etc/timezone:/etc/timezone:ro --network=host --name timezone_service sapphire_timezone_service:latest
