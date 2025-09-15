#!/bin/bash

# Fetch relays CSV from bitchat repository
# Downloads the latest online relays with GPS coordinates

RELAY_URL="https://raw.githubusercontent.com/permissionlesstech/bitchat/refs/heads/main/relays/online_relays_gps.csv"
OUTPUT_FILE="relays.csv"
TEMP_FILE="relays.tmp"

echo "🌐 Fetching relay list from bitchat repository..."
echo "URL: $RELAY_URL"
echo

# Check if curl is available
if ! command -v curl &> /dev/null; then
    echo "❌ Error: curl is required but not installed"
    echo "Please install curl to download the relay list"
    exit 1
fi

# Download the file to a temporary location first
echo "📥 Downloading relay list..."
if curl -fsSL "$RELAY_URL" -o "$TEMP_FILE"; then
    # Check if the downloaded file is valid (not empty and contains CSV data)
    if [ -s "$TEMP_FILE" ]; then
        # Quick validation - check if it looks like CSV with at least one comma per line
        if head -5 "$TEMP_FILE" | grep -q ","; then
            mv "$TEMP_FILE" "$OUTPUT_FILE"
            echo "✅ Successfully downloaded relay list to $OUTPUT_FILE"

            # Count the number of relays
            RELAY_COUNT=$(wc -l < "$OUTPUT_FILE" | tr -d ' ')
            echo "📊 Found $RELAY_COUNT relays in the list"

            # Show first few lines as a sample
            echo
            echo "📋 Sample relays:"
            head -5 "$OUTPUT_FILE" | while IFS=, read -r url lat lon; do
                echo "  $url ($lat, $lon)"
            done

            echo
            echo "🎯 You can now use the geohash relay finder:"
            echo "   ./geohash_relay_finder <geohash> $OUTPUT_FILE"

        else
            echo "❌ Error: Downloaded file doesn't appear to be valid CSV format"
            rm -f "$TEMP_FILE"
            exit 1
        fi
    else
        echo "❌ Error: Downloaded file is empty"
        rm -f "$TEMP_FILE"
        exit 1
    fi
else
    echo "❌ Error: Failed to download relay list"
    echo "Please check your internet connection and try again"
    rm -f "$TEMP_FILE"
    exit 1
fi