#!/usr/bin/env node

const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

// Paths
const SOURCE_MODEL = path.join(__dirname, '../assets/Mistral-7B-Instruct-v0.3.Q4_K_M.gguf');
const IOS_DEST_DIR = path.join(__dirname, '../ios/example/models');
const ANDROID_DEST_DIR = path.join(__dirname, '../android/app/src/main/assets/models');

// Check if source model exists
if (!fs.existsSync(SOURCE_MODEL)) {
  console.error(`❌ Model file not found at: ${SOURCE_MODEL}`);
  process.exit(1);
}

// Create directories if they don't exist
function ensureDirectoryExists(directory) {
  if (!fs.existsSync(directory)) {
    console.log(`📁 Creating directory: ${directory}`);
    fs.mkdirSync(directory, { recursive: true });
  }
}

// Copy to iOS
function copyToIOS() {
  ensureDirectoryExists(IOS_DEST_DIR);
  
  const targetFile = path.join(IOS_DEST_DIR, path.basename(SOURCE_MODEL));
  
  console.log(`📦 Copying model to iOS: ${targetFile}`);
  fs.copyFileSync(SOURCE_MODEL, targetFile);
  
  console.log('✅ Model copied to iOS project directory');
  console.log('NOTE: You will need to add the model file to your Xcode project manually:');
  console.log('1. Open example/ios/example.xcworkspace in Xcode');
  console.log('2. Right-click on the example project in the Navigator');
  console.log('3. Select "Add Files to example..."');
  console.log('4. Navigate to the "models" directory and select the model file');
  console.log('5. Make sure "Create folder references" is selected and click "Add"');
}

// Copy to Android
function copyToAndroid() {
  ensureDirectoryExists(ANDROID_DEST_DIR);
  
  const targetFile = path.join(ANDROID_DEST_DIR, path.basename(SOURCE_MODEL));
  
  console.log(`📦 Copying model to Android: ${targetFile}`);
  fs.copyFileSync(SOURCE_MODEL, targetFile);
  console.log('✅ Model added to Android project');
}

// Add to Info.plist to make the model accessible
function updateInfoPlist() {
  const infoPath = path.join(__dirname, '../ios/example/Info.plist');
  if (!fs.existsSync(infoPath)) {
    console.error('❌ Info.plist not found');
    return;
  }
  
  let content = fs.readFileSync(infoPath, 'utf8');
  
  // Check if the key already exists
  if (!content.includes('UIFileSharingEnabled')) {
    console.log('📝 Adding file sharing to Info.plist');
    
    // Insert before the closing </dict>
    const insertPosition = content.lastIndexOf('</dict>');
    
    // Add the file sharing capability
    const newContent = 
      content.slice(0, insertPosition) +
      '\t<key>UIFileSharingEnabled</key>\n' +
      '\t<true/>\n' +
      '\t<key>LSSupportsOpeningDocumentsInPlace</key>\n' +
      '\t<true/>\n' +
      content.slice(insertPosition);
    
    fs.writeFileSync(infoPath, newContent);
    console.log('✅ Added file sharing capabilities to Info.plist');
  } else {
    console.log('✓ File sharing already enabled in Info.plist');
  }
}

// Run
console.log('🚀 Starting model copy process...');
copyToIOS();
copyToAndroid();
updateInfoPlist();
console.log('✅ Done!'); 