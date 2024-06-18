#!/bin/bash
set -xe

# Path to the kernel source directory
KERNEL_DIR="/Android-11/OnePlus/Instantnoodle/downloads/ubuntu_kernel_instantnoodle"
KERNEL_OBJ_DIR="$KERNEL_DIR/../KERNEL_OBJ"
BUILD_SCRIPT_DIR="/Android-11/OnePlus/Instantnoodle"

# Define branches for adaptation tools and overlay
ADAPTATION_TOOLS_BRANCH=main
ADAPTATION_OVERLAY_BRANCH=android-10

# Function to update the Makefile
update_makefile() {
    echo "Updating the Makefile for unsupported options and paths..."
    # Remove unsupported Clang/Polly flags
    sed -i 's/-Wno-compound-token-split-by-space/-Wno-unicode-whitespace/g' $KERNEL_DIR/Makefile
    sed -i '/-polly/d' $KERNEL_DIR/Makefile

    # Update architecture flags if necessary
    sed -i 's/-mcpu=cortex-a55/-mcpu=cortex-a76/g' $KERNEL_DIR/Makefile  # Update to the desired supported CPU

    # Ensure the correct path for dtc files
    sed -i 's|scripts/dtc-aosp/dtc/|scripts/dtc/|g' $KERNEL_DIR/Makefile

    echo "Makefile updated."
}

# Function to ensure correct directory structure
fix_dtc_directory() {
    echo "Fixing dtc directory structure..."
    if [ ! -d "$KERNEL_DIR/scripts/dtc-aosp/dtc" ]; then
        mkdir -p $KERNEL_DIR/scripts/dtc-aosp/dtc
        cp $KERNEL_DIR/scripts/dtc/* $KERNEL_DIR/scripts/dtc-aosp/dtc/
        echo "dtc files copied to dtc-aosp."
    else
        echo "dtc-aosp directory already exists."
    fi
}

# Ensure required packages are installed
install_dependencies() {
    echo "Installing required packages..."
    sudo apt update
    sudo apt install -y build-essential bc bison flex libssl-dev libncurses5-dev \
        libelf-dev gcc-11 g++-11 clang-13 llvm-13 libclang-dev
    if [ $? -ne 0 ]; then
        echo "Package installation failed"
        exit 1
    fi
}

# Clean build directory
clean_build() {
    echo "Cleaning build directory..."
    rm -rf $KERNEL_OBJ_DIR
}

# Function to download a file using wget or curl
download_file() {
    local url=$1
    local file=$2
    local option=$3
    local option_url=$4

    echo "Downloading: $file"
    if [[ $option == "wget" ]]; then
        wget -O "$file" "$url" &> /dev/null
    elif [[ $option == "curl" ]]; then
        curl --referer "$option_url" -k -o "$file" "$url" &> /dev/null
    fi
    echo "✅ Downloaded: $file"
}

# Function to download multiple files
download_files() {
    local files=(
        "recovery/OrangeFox_R11.1-InstantNoodle-Recovery.img|wget|https://github.com/Wishmasterflo/android_device_oneplus_kebab/releases/download/V15/OrangeFox-R11.1-Unofficial-OnePlus8T_9R-V15.img"
        "recovery/TWRP-InstantNoodle-Recovery.img|wget|https://github.com/scotthowson/twrp_device_oneplus_instantnoodle/releases/download/v1.0.12/twrp-howsondev-v1.0.12.img"
        "recovery/LineageOS-18.1-Recovery.img|wget|https://github.com/IllSaft/los18.1-recovery/releases/download/0.1/LineageOS-18.1-Recovery.img"
    )

    for entry in "${files[@]}"; do
        IFS='|' read -r file option url option_url <<< "$entry"
        download_file "$url" "$file" "$option" "$option_url"
    done

    echo "🎉 All recovery files downloaded successfully."
}

# Build the kernel
build_kernel() {
    echo "Building the kernel..."
    if [ -f "$BUILD_SCRIPT_DIR/build.sh" ]; then
        (cd $BUILD_SCRIPT_DIR && ./build.sh -b Instantnoodle)
    else
        echo "build.sh not found in $BUILD_SCRIPT_DIR. Please check the path."
        exit 1
    fi
}

# Main function
main() {
    install_dependencies
    update_makefile
    fix_dtc_directory
    clean_build
    
    # Clone the build tools repository if the build directory doesn't exist
    if [ ! -d "$BUILD_SCRIPT_DIR/build" ]; then
      echo "Cloning build tools..."
      git clone -b $ADAPTATION_TOOLS_BRANCH https://gitlab.com/ubports/community-ports/halium-generic-adaptation-build-tools "$BUILD_SCRIPT_DIR/build"
    fi

    # Clone the overlay repository into a temporary directory if the overlay directory doesn't exist
    if [ ! -d "$BUILD_SCRIPT_DIR/overlay" ]; then
      echo "Cloning overlay repository..."
      TEMP_DIR=$(mktemp -d)
      git clone -b $ADAPTATION_OVERLAY_BRANCH https://github.com/scotthowson/oneplus8_ubuntu_adaptation $TEMP_DIR

      # Move all files and directories, including hidden ones, excluding .git to prevent conflicts
      echo "Moving files from temporary directory..."
      shopt -s dotglob
      mv $TEMP_DIR/* $TEMP_DIR/.* $BUILD_SCRIPT_DIR/overlay/ || true
      shopt -u dotglob

      # Remove the temporary directory
      echo "Removing temporary directory..."
      rm -rf $TEMP_DIR
    fi

    # Insert HAS_DYNAMIC_PARTITIONS=true at line 43 in make-bootimage.sh
    echo "Inserting HAS_DYNAMIC_PARTITIONS=true in make-bootimage.sh..."
    sed -i '43 i\    HAS_DYNAMIC_PARTITIONS=true' "$BUILD_SCRIPT_DIR/build/make-bootimage.sh"

    # Disable set -x for recovery downloads
    set +x
    # Create the recovery directory and download recovery images if it doesn't exist
    if [ ! -d "$BUILD_SCRIPT_DIR/recovery" ]; then
      echo "Creating recovery directory and downloading recovery images..."
      mkdir $BUILD_SCRIPT_DIR/recovery
      download_files
    fi

    # Modify line 57 in build-kernel.sh
    echo "Modifying line 57 in build-kernel.sh..."
    sed -i '57s/.*/make O="$OUT" $MAKEOPTS -j16/' "$BUILD_SCRIPT_DIR/build/build-kernel.sh"

    # Re-enable set -x
    set -x

    build_kernel
    echo "Kernel build process completed!"
}

# Run the main function
main
