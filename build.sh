#!/bin/bash
# build.sh - Compile all components of the treasure hunt system

CC="gcc"
CFLAGS="-Wall -Wextra -std=c99 -pedantic"
LDFLAGS=""

echo "Building treasure hunt system..."

# Build treasure_manager
echo "Compiling treasure_manager..."
$CC $CFLAGS -o treasure_manager treasure_manager.c $LDFLAGS
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile treasure_manager"
    exit 1
fi

# Build treasure_monitor
echo "Compiling treasure_monitor..."
$CC $CFLAGS -o treasure_monitor treasure_monitor.c $LDFLAGS
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile treasure_monitor"
    exit 1
fi

# Build treasure_hub
echo "Compiling treasure_hub..."
$CC $CFLAGS -o treasure_hub treasure_hub.c $LDFLAGS
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile treasure_hub"
    exit 1
fi

echo "Build completed successfully!"
echo "You can now run the treasure hunt system with './treasure_hub'"

# Make the script executable
chmod +x treasure_hub
chmod +x treasure_monitor
