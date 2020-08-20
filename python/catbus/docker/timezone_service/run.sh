#!/usr/bin/env bash

sudo docker run -d --restart=always --network=host --name timezone_service sapphire_timezone_service:latest
