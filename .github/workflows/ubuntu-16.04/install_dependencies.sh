#!/bin/bash
sudo apt-get update
sudo apt-get install -y libboost-all-dev cmake g++ libssl-dev libsqlite3-dev software-properties-common libssl-dev
sudo apt-get install -y libnetcdf-dev libcurl4-openssl-dev libcpprest-dev doxygen graphviz
sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 314DF160
sudo add-apt-repository ppa:ubuntugis/ppa
sudo apt-get update
sudo apt-get install -y libxml2-dev libopenjp2-7-dev libhdf4-alt-dev libgdal-dev gdal-bin libproj-dev
