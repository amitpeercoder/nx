#!/bin/bash
# Quick test script to check word wrap functionality

echo "Testing word wrap functionality..."
echo "1. Launching nx TUI..."
echo "2. Use arrow keys to navigate to 'Test Note with Long Lines'"
echo "3. Press Alt+W (or ESC then w) to toggle word wrap"
echo "4. Press q to quit"
echo ""
echo "Starting TUI in 3 seconds..."
sleep 3

./build-debug/nx ui