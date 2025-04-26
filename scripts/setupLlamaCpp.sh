#!/bin/bash

# This script handles the cloning and syncing of llama.cpp
# It's similar to what llama.rn does

# Path to where llama.cpp should be
LLAMA_CPP_DIR="cpp/llama.cpp"

# Function to clone llama.cpp
clone_llama_cpp() {
  echo "Cloning llama.cpp..."
  git submodule add https://github.com/ggerganov/llama.cpp.git $LLAMA_CPP_DIR
  git submodule update --init --recursive
}

# Function to update llama.cpp
update_llama_cpp() {
  echo "Updating llama.cpp..."
  git submodule update --remote $LLAMA_CPP_DIR
}

# Function to apply any needed patches to llama.cpp
patch_llama_cpp() {
  echo "Applying patches to llama.cpp..."
  # Example of applying a patch if needed:
  # cd $LLAMA_CPP_DIR && git apply ../../patches/some-patch.patch
}

# Main function
main() {
  # Check if llama.cpp directory exists
  if [ ! -d "$LLAMA_CPP_DIR" ]; then
    clone_llama_cpp
  else
    update_llama_cpp
  fi

  # Apply patches if needed
  patch_llama_cpp

  echo "llama.cpp setup completed!"
}

# Run the main function
main 