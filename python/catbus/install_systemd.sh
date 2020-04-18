#!/usr/bin/env bash

sudo cp catbus_directory.service /etc/systemd/system/catbus_directory.service
sudo chmod 644 /etc/systemd/system/catbus_directory.service
sudo systemctl enable catbus_directory