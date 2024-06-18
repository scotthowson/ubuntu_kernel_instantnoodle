#!/bin/bash

# Path to the kernel source directory
KERNEL_DIR="/home/howson/Documents/UbuntuTouch/OnePlus8/Android-11/ubuntu_kernel_instantnoodle"
KERNEL_OBJ_DIR="$KERNEL_DIR/../KERNEL_OBJ"

# Function to update the Makefile
update_makefile() {
    echo "Updating the Makefile for unsupported options and paths..."
    # Remove unsupported Clang/Polly flags
    sed -i 's/-Wno-compound-token-split-by-space/-Wno-unicode-whitespace/g' $KERNEL_DIR/Makefile
    sed -i '/-polly/d' $KERNEL_DIR/Makefile
    echo "Makefile updated."
}

# Function to ensure correct directory structure
fix_dtc_directory() {
    echo "Fixing dtc directory structure..."
    if [ ! -d "$KERNEL_DIR/scripts/dtc-aosp/dtc" ]; then
        mkdir -p $KERNEL_DIR/scripts/dtc-aosp/dtc
        cp $KERNEL_DIR/scripts/dtc/dtc.c $KERNEL_DIR/scripts/dtc-aosp/dtc/dtc.c
        echo "dtc.c file copied to dtc-aosp."
    else
        echo "dtc-aosp directory already exists."
    fi
}

# Ensure required packages are installed
install_dependencies() {
    echo "Installing required packages..."
    sudo apt update
    sudo apt install -y build-essential bc bison flex libssl-dev libncurses5-dev \
        libelf-dev gcc make clang llvm libclang-dev
}

# Clean build directory
clean_build() {
    echo "Cleaning build directory..."
    rm -rf $KERNEL_OBJ_DIR
}

# Build the kernel
build_kernel() {
    echo "Building the kernel..."
    (cd $KERNEL_DIR/../build_kernel_instantnoodle && ./build.sh)
}

# Main function
main() {
    install_dependencies
    update_makefile
    fix_dtc_directory
    clean_build
    build_kernel
    echo "Kernel build process completed!"
}

# Run the main function
main