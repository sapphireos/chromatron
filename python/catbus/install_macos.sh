#!/usr/bin/env bash
 
sudo cp catbus_directory.plist ~/Library/LaunchAgents/catbus_directory.plist
launchctl load ~/Library/LaunchAgents/catbus_directory.plist