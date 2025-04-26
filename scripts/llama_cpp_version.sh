#!/bin/bash
# This script manages the llama.cpp submodule version
# It ensures we use a specific, tested version of llama.cpp

set -e

# The specific llama.cpp release tag we want to use
# Using a release tag instead of a commit hash makes it more clear which version we're using
# Change this to update the llama.cpp version
LLAMA_CPP_VERSION="b5192"  # Latest release as of April 26, 2025

# Path to the llama.cpp submodule
LLAMA_CPP_DIR="cpp/llama.cpp"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# Function to initialize the submodule
init_submodule() {
  echo -e "${YELLOW}Initializing llama.cpp submodule...${NC}"
  
  # Check if git is available
  if ! command -v git &> /dev/null; then
    echo -e "${RED}Error: git is not installed${NC}"
    exit 1
  fi
  
  # Initialize the submodule if it's not already
  if [ ! -f "$LLAMA_CPP_DIR/.git" ]; then
    git submodule update --init --recursive
  fi
  
  # Go to the submodule directory
  cd $LLAMA_CPP_DIR
  
  # Fetch the latest changes
  git fetch --quiet
  
  # Check out the specific version (using tag)
  git checkout $LLAMA_CPP_VERSION --quiet
  
  echo -e "${GREEN}Successfully initialized llama.cpp to version: ${LLAMA_CPP_VERSION}${NC}"
  echo -e "${YELLOW}Release information can be found at: https://github.com/ggml-org/llama.cpp/releases/tag/${LLAMA_CPP_VERSION}${NC}"
  
  # Go back to the root directory
  cd ../../
}

# Function to display current submodule status
show_status() {
  echo -e "${YELLOW}Current llama.cpp submodule status:${NC}"
  
  if [ -d "$LLAMA_CPP_DIR/.git" ]; then
    cd $LLAMA_CPP_DIR
    CURRENT_COMMIT=$(git rev-parse HEAD)
    CURRENT_BRANCH=$(git branch --show-current || echo "detached HEAD")
    CURRENT_TAG=$(git describe --tags --exact-match 2>/dev/null || echo "no tag")
    
    echo -e "Branch: ${GREEN}$CURRENT_BRANCH${NC}"
    echo -e "Tag: ${GREEN}$CURRENT_TAG${NC}"
    echo -e "Commit: ${GREEN}$CURRENT_COMMIT${NC}"
    
    # Check if we're on the desired version
    if [ "$CURRENT_TAG" = "$LLAMA_CPP_VERSION" ]; then
      echo -e "${GREEN}✓ llama.cpp is at the correct version${NC}"
    else
      echo -e "${RED}✗ llama.cpp is NOT at the expected version${NC}"
      echo -e "${YELLOW}Expected tag: $LLAMA_CPP_VERSION${NC}"
      echo -e "${YELLOW}Current tag:  $CURRENT_TAG${NC}"
    fi
    cd ../../
  else
    echo -e "${RED}Submodule not initialized${NC}"
  fi
}

# Function to list available tags
list_tags() {
  echo -e "${YELLOW}Available llama.cpp release tags:${NC}"
  
  if [ -d "$LLAMA_CPP_DIR/.git" ]; then
    cd $LLAMA_CPP_DIR
    git fetch --tags --quiet
    # List the 10 most recent tags
    echo -e "${GREEN}Recent releases:${NC}"
    git tag -l "b*" --sort=-version:refname | head -10
    cd ../../
  else
    # If submodule not initialized, fetch tags directly from the remote
    echo -e "${YELLOW}Fetching tags from remote repository...${NC}"
    git ls-remote --tags https://github.com/ggml-org/llama.cpp.git | grep -o 'refs/tags/b[0-9]*' | sed 's/refs\/tags\///' | sort -rV | head -10
  fi
  
  echo -e "\n${YELLOW}To update llama.cpp version, edit LLAMA_CPP_VERSION in this script.${NC}"
}

# Main function
main() {
  case "$1" in
    init)
      init_submodule
      ;;
    status)
      show_status
      ;;
    list-tags)
      list_tags
      ;;
    *)
      echo "Usage: $0 {init|status|list-tags}"
      echo "  init       - Initialize and set the llama.cpp submodule to the correct version"
      echo "  status     - Show current llama.cpp submodule status"
      echo "  list-tags  - List available llama.cpp release tags"
      ;;
  esac
}

# Run the main function with the provided arguments
main "$@" 