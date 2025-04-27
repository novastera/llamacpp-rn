#!/usr/bin/env node

const { execSync } = require('child_process');
const fs = require('fs');
const path = require('path');

console.log('🚀 Setting up LlamaCpp model for development...');

// Check if Xcode project needs repair
const xcodeProjectPath = path.join(__dirname, '../ios/example.xcodeproj');
const pbxprojPath = path.join(xcodeProjectPath, 'project.pbxproj');

// First, check if we need to reset the Xcode project
let needsReset = false;
try {
  console.log('Checking Xcode project status...');
  if (fs.existsSync(pbxprojPath)) {
    const pbxprojContent = fs.readFileSync(pbxprojPath, 'utf8');
    if (pbxprojContent.includes('D966DE9A2BA1FEDCBA543210')) {
      console.log('⚠️ Detected previous modifications to Xcode project. Resetting...');
      needsReset = true;
    }
  }
} catch (error) {
  console.log('⚠️ Error checking Xcode project, assuming it needs reset:', error.message);
  needsReset = true;
}

// If needed, reset the Expo project to regenerate Xcode files
if (needsReset) {
  console.log('\n🔄 Resetting Expo project to fix Xcode project...');
  try {
    execSync('node ./scripts/reset-project.js', { stdio: 'inherit' });
    console.log('✅ Project reset successful');
  } catch (error) {
    console.error('❌ Error resetting project:', error.message);
    console.log('Continuing anyway...');
  }
}

// Step 1: Run the copy models script
console.log('\n📦 Step 1: Copying model files to iOS and Android projects...');
try {
  execSync('node ./scripts/copy-models.js', { stdio: 'inherit' });
} catch (error) {
  console.error('❌ Error running copy-models script:', error.message);
  process.exit(1);
}

// Step 2: Prebuild the project to ensure all plugins are applied
console.log('\n🔧 Step 2: Running prebuild to apply all plugins...');
try {
  execSync('npx expo prebuild --clean', { stdio: 'inherit' });
} catch (error) {
  console.error('❌ Error running prebuild:', error.message);
  console.log('Continuing anyway...');
}

// Step 3: Install pods for iOS
if (process.platform === 'darwin') {
  console.log('\n📱 Step 3: Installing pods for iOS...');
  try {
    execSync('cd ios && pod install', { stdio: 'inherit' });
  } catch (error) {
    console.error('❌ Error installing pods:', error.message);
    console.log('Continuing anyway...');
  }
}

// Step 4: Verify all required files are in place
console.log('\n🔍 Step 4: Verifying files...');

const iosModelPath = path.join(__dirname, '../ios/example/models/Mistral-7B-Instruct-v0.3.Q4_K_M.gguf');
const androidModelPath = path.join(__dirname, '../android/app/src/main/assets/models/Mistral-7B-Instruct-v0.3.Q4_K_M.gguf');

if (fs.existsSync(iosModelPath)) {
  console.log(`✅ iOS model found at: ${iosModelPath}`);
} else {
  console.error(`❌ iOS model NOT found at: ${iosModelPath}`);
}

if (fs.existsSync(androidModelPath)) {
  console.log(`✅ Android model found at: ${androidModelPath}`);
} else {
  console.error(`❌ Android model NOT found at: ${androidModelPath}`);
}

console.log('\n⚠️ IMPORTANT: You need to add the model file to Xcode manually:');
console.log('1. Open example/ios/example.xcworkspace in Xcode');
console.log('2. Right-click on the example project in the Navigator');
console.log('3. Select "Add Files to example..."');
console.log('4. Navigate to the "models" directory and select the model file');
console.log('5. Make sure "Create folder references" is selected and click "Add"');
console.log('6. Run the project from Xcode, or close Xcode and use "npm run ios"');

console.log('\n✨ Setup complete! You can now run:');
console.log('- npm run ios    (after adding the model in Xcode)');
console.log('- npm run android');
console.log('\nIf you encounter any issues, try:');
console.log('1. Checking the logs above for errors');
console.log('2. Running "npx expo prebuild --clean" to recreate the projects');
console.log('3. For iOS, run "cd ios && pod install" to reinstall the pods'); 