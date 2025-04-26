#!/bin/bash

# This script handles the cloning and syncing of llama.cpp
# It ensures a specific version is used for consistent builds

set -e

# Path to where llama.cpp should be
LLAMA_CPP_DIR="cpp/llama.cpp"

# Make the script executable
chmod +x "$(dirname "$0")/llama_cpp_version.sh"

# Use the version management script
"$(dirname "$0")/llama_cpp_version.sh" init

echo "llama.cpp setup completed!" 