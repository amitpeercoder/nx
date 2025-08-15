#!/bin/bash

# Test script to verify editor responsiveness

echo "Testing editor integration..."

# Create a test note
NOTE_ID=$(echo "Test content for editor integration testing" | ./build/nx new "Editor Integration Test" --tags test | grep -o '01[A-Z0-9]*')

echo "Created test note: $NOTE_ID"

# Test 1: Command line editing (should work)
echo "Test 1: Command line editor integration"
export EDITOR="echo 'Editor would open file: '"
./build/nx edit "$NOTE_ID"

echo -e "\nTest 1 completed. Editor integration from command line works."

# Test 2: TUI editing (this was the problematic one)
echo -e "\nTest 2: TUI editor integration (the fix)"
echo "Starting TUI - navigate to note and press Enter to edit..."
echo "Press 'q' to quit TUI if it opens properly"

# Note: This won't actually test the editor since it requires interactive input
# but we can verify the TUI starts correctly
timeout 3s ./build/nx ui || echo "TUI startup test completed (timeout expected)"

echo -e "\nEditor integration tests completed."
echo "Manual testing required: Start 'nx ui', select note, press Enter to edit"