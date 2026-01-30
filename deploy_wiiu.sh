#!/bin/bash

SOURCE_FILE="smb_wiiu/smb_wiiu.wuhb"
VOLUME_NAME="SDWIIU"
MOUNT_POINT="/Volumes/$VOLUME_NAME"
DEST_DIR="$MOUNT_POINT/wiiu/apps"

# Check if source file exists
if [ ! -f "$SOURCE_FILE" ]; then
    echo "Error: Source file '$SOURCE_FILE' not found."
    exit 1
fi

# Check if SD card is mounted
if [ ! -d "$MOUNT_POINT" ]; then
    echo "Error: SD card '$VOLUME_NAME' is not mounted at '$MOUNT_POINT'."
    echo "Please insert the SD card."
    exit 1
fi

# Create destination directory
echo "Creating destination directory..."
mkdir -p "$DEST_DIR"

# Copy file
echo "Copying '$SOURCE_FILE' to '$DEST_DIR'..."
cp "$SOURCE_FILE" "$DEST_DIR/"

if [ $? -eq 0 ]; then
    echo "Copy successful."
else
    echo "Error: Copy failed."
    exit 1
fi

# Eject SD card
echo "Ejecting '$VOLUME_NAME'..."
diskutil eject "$MOUNT_POINT"

echo "Done."
