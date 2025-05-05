#!/bin/bash
# setupLlamaCpp.sh - Main script to setup llama.cpp for React Native
# 1. Initializes llama.cpp submodule and sets to correct version
# 2. Sets up iOS framework using llama_cpp_ios.sh

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PACKAGE_DIR="$( cd "$SCRIPT_DIR/.." && pwd )"
LLAMA_CPP_DIR="$PACKAGE_DIR/cpp/llama.cpp"

# Import version settings
. "$SCRIPT_DIR/used_version.sh"

# Check if required tools are available
check_requirements() {
  # Check if git is available
  if ! command -v git &> /dev/null; then
    echo -e "${RED}Error: git is required but not installed${NC}"
    return 1
  fi
  
  # Ensure scripts are executable
  chmod +x "$SCRIPT_DIR/llama_cpp_ios.sh"
  
  return 0
}

# Initialize and setup llama.cpp submodule
setup_llama_cpp_submodule() {
  echo -e "${YELLOW}Setting up llama.cpp submodule (commit: $LLAMA_CPP_COMMIT)...${NC}"
  
  # Create directory structure if it doesn't exist
  mkdir -p "$(dirname "$LLAMA_CPP_DIR")"
  
  # Check if the directory already exists as a git repository
  if [ -d "$LLAMA_CPP_DIR/.git" ]; then
    echo -e "${YELLOW}llama.cpp repository exists, updating...${NC}"
    
    # Get current commit hash
    cd "$LLAMA_CPP_DIR"
    CURRENT_COMMIT=$(git rev-parse HEAD 2>/dev/null || echo "none")
    
    # If already at the correct commit, no need to update
    if [ "$CURRENT_COMMIT" = "$LLAMA_CPP_COMMIT" ]; then
      echo -e "${GREEN}llama.cpp is already at the correct commit: ${LLAMA_CPP_COMMIT}${NC}"
      cd "$PACKAGE_DIR"
      return 0
    fi
    
    # Otherwise, fetch and checkout the correct commit
    echo -e "${YELLOW}llama.cpp is at commit $CURRENT_COMMIT, updating to $LLAMA_CPP_COMMIT${NC}"
    git fetch origin --quiet
    git checkout "$LLAMA_CPP_COMMIT" --quiet
    cd "$PACKAGE_DIR"
  else
    # Properly handle submodule initialization
    echo -e "${YELLOW}Initializing llama.cpp repository...${NC}"
    
    # Check if the directory exists but is not a git repository
    if [ -d "$LLAMA_CPP_DIR" ]; then
      echo -e "${YELLOW}Directory exists but is not a git repository. Removing...${NC}"
      rm -rf "$LLAMA_CPP_DIR"
    fi
    
    # Clone the repository directly (safer than trying to use submodule commands)
    echo -e "${YELLOW}Cloning llama.cpp repository...${NC}"
    git clone https://github.com/ggml-org/llama.cpp.git "$LLAMA_CPP_DIR"
    
    # Checkout the specific commit
    cd "$LLAMA_CPP_DIR"
    git checkout "$LLAMA_CPP_COMMIT" --quiet
    cd "$PACKAGE_DIR"
    
    # Recommend setting up as a proper submodule for future use
    echo -e "${YELLOW}Repository cloned and configured. For proper development workflow:${NC}"
    echo -e "${YELLOW}1. If you want to register this as a git submodule, run:${NC}"
    echo -e "${YELLOW}   git submodule add https://github.com/ggml-org/llama.cpp.git cpp/llama.cpp${NC}"
    echo -e "${YELLOW}   cd cpp/llama.cpp && git checkout $LLAMA_CPP_COMMIT && cd -${NC}"
    echo -e "${YELLOW}   git add .gitmodules cpp/llama.cpp${NC}"
    echo -e "${YELLOW}   git commit -m \"Add llama.cpp as submodule at version $LLAMA_CPP_COMMIT\"${NC}"
  fi
  
  # Setup directories for Android
  mkdir -p "android/src/main/cpp/includes"
  mkdir -p "android/src/main/jniLibs/arm64-v8a"
  mkdir -p "android/src/main/jniLibs/armeabi-v7a"
  mkdir -p "android/src/main/jniLibs/x86"
  mkdir -p "android/src/main/jniLibs/x86_64"
  
  echo -e "${GREEN}Successfully set up llama.cpp repository to commit: ${LLAMA_CPP_COMMIT}${NC}"
  return 0
}

# Check if llama.cpp repository is set up correctly
check_llama_cpp_repository() {
  echo -e "${YELLOW}Checking llama.cpp repository...${NC}"
  
  if [ ! -d "$LLAMA_CPP_DIR" ]; then
    echo -e "${RED}llama.cpp repository not found at: $LLAMA_CPP_DIR${NC}"
    return 1
  fi
  
  if [ ! -d "$LLAMA_CPP_DIR/.git" ]; then
    echo -e "${RED}llama.cpp exists but is not a git repository at: $LLAMA_CPP_DIR${NC}"
    return 1
  fi
  
  # Check if we're on the correct commit
  cd "$LLAMA_CPP_DIR"
  CURRENT_COMMIT=$(git rev-parse HEAD 2>/dev/null || echo "none")
  
  echo -e "Commit: ${GREEN}$CURRENT_COMMIT${NC}"
  
  # Check if we're on the desired version
  if [ "$CURRENT_COMMIT" = "$LLAMA_CPP_COMMIT" ]; then
    echo -e "${GREEN}✓ llama.cpp is at the correct commit${NC}"
    cd "$PACKAGE_DIR"
    return 0
  else
    echo -e "${RED}✗ llama.cpp is NOT at the expected commit${NC}"
    echo -e "${YELLOW}Expected commit: $LLAMA_CPP_COMMIT${NC}"
    echo -e "${YELLOW}Current commit:  $CURRENT_COMMIT${NC}"
    cd "$PACKAGE_DIR"
    return 1
  fi
}

# Clean everything
clean_all() {
  echo -e "${YELLOW}Cleaning all llama.cpp related files and directories...${NC}"
  
  # Clean iOS framework using the iOS script
  "$SCRIPT_DIR/llama_cpp_ios.sh" clean
  
  # For the git repository, reset to the correct commit
  if [ -d "$LLAMA_CPP_DIR/.git" ]; then
    echo -e "${YELLOW}Resetting llama.cpp repository to clean state...${NC}"
    cd "$LLAMA_CPP_DIR"
    git fetch origin --quiet
    git checkout "$LLAMA_CPP_COMMIT" --quiet --force
    git clean -fdx --quiet
    cd "$PACKAGE_DIR"
  fi
  
  echo -e "${GREEN}All cleaned successfully${NC}"
  return 0
}

# Show version information
show_version() {
  echo -e "${YELLOW}llama.cpp version information:${NC}"
  echo -e "Commit: ${GREEN}$LLAMA_CPP_COMMIT${NC}"
  echo -e "Tag: ${GREEN}$LLAMA_CPP_TAG${NC}"
  
  # Check actual repository version
  if [ -d "$LLAMA_CPP_DIR/.git" ]; then
    cd "$LLAMA_CPP_DIR"
    CURRENT_COMMIT=$(git rev-parse HEAD 2>/dev/null || echo "none")
    CURRENT_TAG=$(git describe --tags --exact-match 2>/dev/null || echo "no tag")
    
    echo -e "Current commit: ${GREEN}$CURRENT_COMMIT${NC}"
    echo -e "Current tag: ${GREEN}$CURRENT_TAG${NC}"
    
    if [ "$CURRENT_COMMIT" != "$LLAMA_CPP_COMMIT" ]; then
      echo -e "${YELLOW}Warning: Current commit doesn't match expected commit${NC}"
    fi
    cd "$PACKAGE_DIR"
  else
    echo -e "${YELLOW}llama.cpp repository not initialized${NC}"
  fi
  
  # Show iOS framework version
  "$SCRIPT_DIR/llama_cpp_ios.sh" version
  
  return 0
}

# Main function
main() {
  if [ $# -eq 0 ]; then
    echo "Usage: $0 {init|check|clean|version} [--force]"
    echo "  init    - Initialize llama.cpp repository and iOS framework"
    echo "  check   - Check if repository and iOS framework are present and valid"
    echo "  clean   - Clean everything (reset repository and remove iOS framework)"
    echo "  version - Show version information"
    echo ""
    echo "Options:"
    echo "  --force - Force redownload even if files exist"
    exit 1
  fi
  
  if ! check_requirements; then
    echo -e "${RED}Error: Required tools are missing${NC}"
    exit 1
  fi
  
  local command="$1"
  shift
  
  case "$command" in
    "init")
      setup_llama_cpp_submodule
      # Pass remaining arguments to iOS script
      "$SCRIPT_DIR/llama_cpp_ios.sh" init "$@"
      ;;
    "check")
      check_llama_cpp_repository
      source_result=$?
      
      "$SCRIPT_DIR/llama_cpp_ios.sh" check
      ios_result=$?
      
      if [ $source_result -eq 0 ] && [ $ios_result -eq 0 ]; then
        echo -e "${GREEN}All checks passed successfully${NC}"
        return 0
      else
        echo -e "${RED}Some checks failed${NC}"
        return 1
      fi
      ;;
    "clean")
      clean_all
      ;;
    "version")
      show_version
      ;;
    *)
      echo "Unknown command: $command"
      echo "Usage: $0 {init|check|clean|version} [--force]"
      exit 1
      ;;
  esac
}

# Run main with all arguments
main "$@"
