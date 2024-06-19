#!/bin/bash

# Function to install dependencies on Ubuntu
install_dependencies_ubuntu() {
    sudo apt-get update
    sudo apt-get install -y libgtk2.0-dev libglib2.0-dev libglade2-dev
    sudo apt-get install -y qt5-qmake qtbase5-dev qtbase5-dev-tools
}

# Function to install dependencies on Fedora
install_dependencies_fedora() {
    sudo dnf install -y gtk2-devel glib2-devel libglade2-devel
    sudo dnf install -y qt5-qtbase-devel
}

# Detect the OS and install dependencies accordingly
if [ -f /etc/os-release ]; then
    . /etc/os-release
    case "$ID" in
        ubuntu)
            install_dependencies_ubuntu
            ;;
        fedora)
            install_dependencies_fedora
            ;;
        *)
            echo "Unsupported OS: $ID"
            exit 1
            ;;
    esac
else
    echo "Unable to detect the operating system."
    exit 1
fi

# Set PKG_CONFIG_PATH for Ubuntu
if [ "$ID" == "ubuntu" ]; then
    export PKG_CONFIG_PATH=/usr/lib/pkgconfig:/usr/share/pkgconfig
fi

# Choose configuration method
CONFIG_METHOD=$1

if [ "$CONFIG_METHOD" == "gconfig" ]; then
    ARCH=arm64 make gconfig
elif [ "$CONFIG_METHOD" == "xconfig" ]; then
    ARCH=arm64 make xconfig
else
    echo "Please specify either 'gconfig' or 'xconfig' as an argument."
    exit 1
fi
