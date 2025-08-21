#!/bin/bash

echo "Testing word wrap functionality..."
echo ""
echo "Instructions:"
echo "1. Navigate to the 'Test Note with Long Lines'"
echo "2. Press 'w' to toggle word wrap"
echo "3. You should see the status message toggle"
echo "4. Long lines should wrap when enabled"
echo "5. Press 'q' to quit"
echo ""
echo "Expected behavior:"
echo "- Status shows: '📝 Word wrap enabled' or '📄 Word wrap disabled'"
echo "- Long lines break at word boundaries when enabled"
echo ""

read -p "Press Enter to start TUI..."
./build-debug/nx ui