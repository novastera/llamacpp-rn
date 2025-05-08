const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

// Paths
const rootDir = path.resolve(__dirname, '../..');
const sourceBaseDir = path.join(rootDir);
const targetBaseDir = path.join(__dirname, '../node_modules/@novastera-oss/llamacpp-rn');

console.log('Preparing Android development environment...');

// 1. Clean previous build artifacts
try {
  console.log('Cleaning previous build artifacts...');
  
  // Clean in the main project
  if (fs.existsSync(path.join(rootDir, 'android/.cxx'))) {
    fs.rmSync(path.join(rootDir, 'android/.cxx'), { recursive: true, force: true });
    console.log('Removed android/.cxx directory');
  }
  
  if (fs.existsSync(path.join(rootDir, 'android/build'))) {
    fs.rmSync(path.join(rootDir, 'android/build'), { recursive: true, force: true });
    console.log('Removed android/build directory');
  }
  
  // Clean the entire module directory in node_modules for a fresh start
  if (fs.existsSync(targetBaseDir)) {
    fs.rmSync(targetBaseDir, { recursive: true, force: true });
    console.log('Removed entire module directory from node_modules');
    
    // Recreate the base directory
    fs.mkdirSync(targetBaseDir, { recursive: true });
    console.log('Recreated module directory in node_modules');
  }
} catch (error) {
  console.warn('Warning during cleanup:', error.message);
}

// 2. Fix include paths in source files
try {
  console.log('Fixing include paths in source files...');
  
  // Run the fix-include-paths.js script
  execSync('node ' + path.join(__dirname, 'fix-include-paths.js'), { stdio: 'inherit' });
  console.log('Include paths fixed successfully');
} catch (error) {
  console.error('Error fixing include paths:', error.message);
  process.exit(1);
}

// 3. Create temporary headers for direct inclusion
try {
  console.log('Creating temporary header files...');
  
  // Run the create-temp-headers.js script
  execSync('node ' + path.join(__dirname, 'create-temp-headers.js'), { stdio: 'inherit' });
  console.log('Temporary header files created successfully');
} catch (error) {
  console.error('Error creating temporary header files:', error.message);
  process.exit(1);
}

// 4. Copy C++ files
try {
  console.log('Copying C++ files...');
  
  // Run the copy-cpp-files.js script
  execSync('node ' + path.join(__dirname, 'copy-cpp-files.js'), { stdio: 'inherit' });
  console.log('C++ files copied successfully');
} catch (error) {
  console.error('Error copying C++ files:', error.message);
  process.exit(1);
}

// 5. Copy native libraries
try {
  console.log('Copying native libraries...');
  
  // Run the copy-libs.js script
  execSync('node ' + path.join(__dirname, 'copy-libs.js'), { stdio: 'inherit' });
  console.log('Native libraries copied successfully');
} catch (error) {
  console.error('Error copying native libraries:', error.message);
  process.exit(1);
}

// 6. Copy Android build files
try {
  console.log('Copying Android build files...');
  
  // Copy the CMakeLists.txt file
  const cmakeSrc = path.join(sourceBaseDir, 'android/src/main/jni/CMakeLists.txt');
  const cmakeDest = path.join(targetBaseDir, 'android/src/main/jni/CMakeLists.txt');
  
  // Make sure the destination directory exists
  if (!fs.existsSync(path.dirname(cmakeDest))) {
    fs.mkdirSync(path.dirname(cmakeDest), { recursive: true });
  }
  
  fs.copyFileSync(cmakeSrc, cmakeDest);
  console.log(`Copied: ${cmakeSrc} -> ${cmakeDest}`);
  
  // Copy OnLoad.cpp file
  const onloadSrc = path.join(sourceBaseDir, 'android/src/main/jni/OnLoad.cpp');
  const onloadDest = path.join(targetBaseDir, 'android/src/main/jni/OnLoad.cpp');
  
  fs.copyFileSync(onloadSrc, onloadDest);
  console.log(`Copied: ${onloadSrc} -> ${onloadDest}`);
} catch (error) {
  console.error('Error copying Android build files:', error.message);
  process.exit(1);
}

console.log('Android development environment prepared successfully!');
console.log('You can now run "npm run android" to build and run the Android app.'); 