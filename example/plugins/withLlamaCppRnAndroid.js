const { withDangerousMod } = require('@expo/config-plugins');
const fs = require('fs');
const path = require('path');

/**
 * Config plugin to ensure LlamaCppRn is properly registered in Android Expo builds
 * - Only handles the necessary setup for local development
 * - Avoids conflicts with build.gradle and CMakeLists.txt
 * - IMPORTANT: This plugin ONLY sets up paths for local development. It does not touch any C++
 *   or native module registration code, as that's handled by your existing CMakeLists.txt
 */
const withLlamaCppRnAndroid = (config) => {
  return withDangerousMod(config, [
    'android',
    async (config) => {
      // Update settings.gradle to include the module
      const settingsGradlePath = path.join(
        config.modRequest.platformProjectRoot,
        'settings.gradle'
      );
      
      if (fs.existsSync(settingsGradlePath)) {
        let settingsContent = fs.readFileSync(settingsGradlePath, 'utf8');
        
        // Add include statement for the local module if not present
        if (!settingsContent.includes("include ':llamacpp-rn'")) {
          const includeAppLine = settingsContent.indexOf("include ':app'");
          if (includeAppLine !== -1) {
            const newLine = settingsContent.indexOf('\n', includeAppLine);
            if (newLine !== -1) {
              const beforeInclude = settingsContent.substring(0, newLine + 1);
              const afterInclude = settingsContent.substring(newLine + 1);
              
              settingsContent = 
                beforeInclude + 
                "include ':llamacpp-rn'\n" +
                "project(':llamacpp-rn').projectDir = new File(rootProject.projectDir, '../../android')\n" +
                afterInclude;
              
              fs.writeFileSync(settingsGradlePath, settingsContent);
              console.log('Added llamacpp-rn module to settings.gradle');
            }
          }
        }
      }
      
      // Add dependency to app build.gradle if needed
      const appBuildGradlePath = path.join(
        config.modRequest.platformProjectRoot,
        'app/build.gradle'
      );
      
      if (fs.existsSync(appBuildGradlePath)) {
        let buildGradleContent = fs.readFileSync(appBuildGradlePath, 'utf8');
        
        // Add implementation project line if not present
        if (!buildGradleContent.includes("implementation project(':llamacpp-rn')")) {
          const dependenciesPos = buildGradleContent.indexOf('dependencies {');
          if (dependenciesPos !== -1) {
            const afterDependencies = buildGradleContent.indexOf('\n', dependenciesPos) + 1;
            
            const beforeDep = buildGradleContent.substring(0, afterDependencies);
            const afterDep = buildGradleContent.substring(afterDependencies);
            
            buildGradleContent = 
              beforeDep + 
              "    // Add llamacpp-rn dependency\n" +
              "    implementation project(':llamacpp-rn')\n\n" +
              afterDep;
            
            fs.writeFileSync(appBuildGradlePath, buildGradleContent);
            console.log('Added llamacpp-rn dependency to app/build.gradle');
          }
        }
      }
      
      // --- FBJNI HEADER PATHS ---
      // All possible locations for fbjni headers
      const possibleFbjniPaths = [
        // node_modules (various nesting levels)
        path.join(config.modRequest.platformProjectRoot, 'node_modules/react-native/ReactAndroid/src/main/jni/first-party/fbjni/headers'),
        path.join(config.modRequest.platformProjectRoot, '../node_modules/react-native/ReactAndroid/src/main/jni/first-party/fbjni/headers'),
        path.join(config.modRequest.platformProjectRoot, '../../node_modules/react-native/ReactAndroid/src/main/jni/first-party/fbjni/headers'),
        path.join(config.modRequest.platformProjectRoot, '../../../node_modules/react-native/ReactAndroid/src/main/jni/first-party/fbjni/headers'),
        // Gradle cache (common for RN 0.71+)
        path.join(process.env.HOME || '', '.gradle/caches/modules-2/files-2.1/com.facebook.fbjni/fbjni'),
        path.join(process.env.HOME || '', '.gradle/caches/transforms-3'),
      ];
      // Only keep existing directories
      const existingFbjniPaths = possibleFbjniPaths.filter(p => fs.existsSync(p) && fs.lstatSync(p).isDirectory());
      console.log(`Found ${existingFbjniPaths.length} existing FBJNI header paths:`);
      existingFbjniPaths.forEach(p => console.log(`  - ${p}`));
      
      // --- REACT NATIVE PATHS ---
      // Find React Native path
      const possibleReactNativePaths = [
        path.join(config.modRequest.platformProjectRoot, 'node_modules/react-native'),
        path.join(config.modRequest.platformProjectRoot, '../node_modules/react-native'),
        path.join(config.modRequest.platformProjectRoot, '../../node_modules/react-native'),
        path.join(config.modRequest.platformProjectRoot, '../../../node_modules/react-native'),
      ];
      
      let reactNativePath = null;
      for (const rnPath of possibleReactNativePaths) {
        if (fs.existsSync(path.join(rnPath, 'package.json'))) {
          reactNativePath = rnPath;
          break;
        }
      }
      
      if (!reactNativePath) {
        console.log('Could not find React Native path. Using default path.');
        reactNativePath = path.join(config.modRequest.platformProjectRoot, 'node_modules/react-native');
      }
      
      console.log(`Using React Native path: ${reactNativePath}`);
      
      // --- JSI HEADERS ---
      // Ensure JSI headers are available
      const jsiHeaders = path.join(reactNativePath, 'ReactCommon/jsi');
      console.log(`Checking JSI headers at: ${jsiHeaders}`);
      
      if (fs.existsSync(path.join(jsiHeaders, 'jsi.h'))) {
        console.log('Found JSI headers.');
      } else {
        console.log('Could not find JSI headers. You may encounter build issues.');
      }
      
      // --- JNI DIR SETUP ---
      const jniDir = path.join(config.modRequest.platformProjectRoot, 'app/src/main/jni');
      if (!fs.existsSync(jniDir)) {
        fs.mkdirSync(jniDir, { recursive: true });
        console.log('Created JNI directory for header shimming');
      }
      
      // Create fbjni directory for header shims
      const fbjniDir = path.join(jniDir, 'fbjni');
      if (!fs.existsSync(fbjniDir)) {
        fs.mkdirSync(fbjniDir, { recursive: true });
        console.log('Created fbjni directory for header shims');
      }
      
      // Copy FBJNI headers to the shim directory
      if (existingFbjniPaths.length > 0) {
        const sourceDir = existingFbjniPaths[0];
        const headerFiles = fs.readdirSync(sourceDir).filter(file => file.endsWith('.h'));
        console.log(`Found ${headerFiles.length} FBJNI header files to copy`);
        
        headerFiles.forEach(headerFile => {
          const sourcePath = path.join(sourceDir, headerFile);
          const targetPath = path.join(fbjniDir, headerFile);
          fs.copyFileSync(sourcePath, targetPath);
          console.log(`Copied ${headerFile} to ${fbjniDir}`);
        });
      }
      
      // --- CMake FBJNI INCLUDE FILE ---
      const fbjniPathsCMakePath = path.join(jniDir, 'fbjni_paths.cmake');
      let fbjniPathsCMakeContent = '# Auto-generated by Expo plugin\n# FBJNI include paths\n';
      
      // Add the local fbjni directory first
      fbjniPathsCMakeContent += `include_directories("${fbjniDir.replace(/\\/g, '/')}")\n`;
      
      // Then add all other found paths
      existingFbjniPaths.forEach(p => {
        fbjniPathsCMakeContent += `include_directories("${p.replace(/\\/g, '/')}")\n`;
      });
      
      fs.writeFileSync(fbjniPathsCMakePath, fbjniPathsCMakeContent);
      
      // --- CMakeLists.txt WRAPPER ---
      const mainCMakeListsPath = path.join(config.modRequest.platformProjectRoot, '../../android/src/main/jni/CMakeLists.txt');
      const wrapperCMakeListsPath = path.join(jniDir, 'CMakeLists.txt');
      const wrapperCMakeContent = `
# Auto-generated by Expo plugin
cmake_minimum_required(VERSION 3.13)
project(llamacpp_wrapper)

# Add the JNI directory to the include path
include_directories("${jniDir.replace(/\\/g, '/')}")

# Include the FBJNI paths file
include("${fbjniPathsCMakePath.replace(/\\/g, '/')}")

# Set up important paths
set(TM_DIR "${mainCMakeListsPath.replace(/\/src\/main\/jni\/CMakeLists.txt/, "").replace(/\\/g, '/')}/../../tm")
message(STATUS "TM directory: \${TM_DIR}")

# Define React Android directory for ReactNative-application.cmake
set(REACT_ANDROID_DIR "${reactNativePath.replace(/\\/g, '/')}/ReactAndroid")
message(STATUS "Using React Android directory: \${REACT_ANDROID_DIR}")

# Ensure we have proper JSI and TurboModule includes before including the main CMakeLists
include_directories(
  "\${REACT_ANDROID_DIR}/src/main/jni/react/turbomodule"
  "\${REACT_ANDROID_DIR}/src/main/java/com/facebook/react/turbomodule/core/jni"
  "${reactNativePath.replace(/\\/g, '/')}/ReactCommon"
  "${reactNativePath.replace(/\\/g, '/')}/ReactCommon/callinvoker"
  "${reactNativePath.replace(/\\/g, '/')}/ReactCommon/jsi"
  "${reactNativePath.replace(/\\/g, '/')}/ReactCommon/hermes"
  "${reactNativePath.replace(/\\/g, '/')}/ReactCommon/react/nativemodule/core"
  "\${REACT_ANDROID_DIR}/src/main/jni/first-party/fbjni/headers"
)

# Now include the main CMakeLists.txt
include("${mainCMakeListsPath.replace(/\\/g, '/')}")
`;
      fs.writeFileSync(wrapperCMakeListsPath, wrapperCMakeContent);
      console.log('Created wrapper CMakeLists.txt for include path fixing');
      
      // --- JNI build.gradle ---
      const jniBuildGradlePath = path.join(jniDir, 'build.gradle');
      const escapedJniDir = jniDir.replace(/\\/g, '/').replace(/'/g, "\\'");
      const escapedFbjniDir = fbjniDir.replace(/\\/g, '/').replace(/'/g, "\\'");
      
      // Create a list of all possible FBJNI include paths for Gradle
      const gradleFbjniIncludePaths = existingFbjniPaths
        .map(p => p.replace(/\\/g, '/').replace(/'/g, "\\'"))
        .join(' -I');
      
      const jniBuildGradleContent = `
android {
  defaultConfig {
    externalNativeBuild {
      cmake {
        arguments "-DANDROID_STL=c++_shared",
                 "-DCMAKE_BUILD_TYPE=Release",
                 "-DREACT_ANDROID_DIR=${reactNativePath.replace(/\\/g, '/')}/ReactAndroid",
                 "-DRELY_ON_REACTNATIVE_APPLICATION_CMAKE=YES"
        cppFlags "-std=c++17 -frtti -fexceptions -DRCT_NEW_ARCH_ENABLED=1 -DFOLLY_NO_CONFIG -DFOLLY_MOBILE=1 -DFOLLY_USE_LIBCPP=1 -I${escapedJniDir} -I${escapedFbjniDir}${gradleFbjniIncludePaths ? ' -I' + gradleFbjniIncludePaths : ''}"
      }
    }
    ndk {
      abiFilters 'arm64-v8a', 'x86_64'
    }
  }
  externalNativeBuild {
    cmake {
      path "CMakeLists.txt"
    }
  }
}
`;
      fs.writeFileSync(jniBuildGradlePath, jniBuildGradleContent);
      console.log('Created custom build.gradle for JNI directory');
      
      // --- llama.cpp path file ---
      const llamaCppPathFile = path.join(jniDir, 'llama_cpp_path.txt');
      const originalLlamacppDir = path.join(config.modRequest.platformProjectRoot, '../../tm/llama.cpp');
      fs.writeFileSync(llamaCppPathFile, originalLlamacppDir);
      console.log(`Created llama.cpp path file pointing to ${originalLlamacppDir}`);
      
      // --- jniLibs directory setup (optional, for prebuilt .so files) ---
      const appJniLibsPath = path.join(config.modRequest.platformProjectRoot, 'app/src/main/jniLibs');
      if (!fs.existsSync(appJniLibsPath)) {
          fs.mkdirSync(appJniLibsPath, { recursive: true });
          console.log('Created jniLibs directory in app');
      }
      
      // --- local.properties for Android SDK (optional) ---
      const localPropertiesPath = path.join(config.modRequest.platformProjectRoot, 'local.properties');
      if (!fs.existsSync(localPropertiesPath) && process.env.ANDROID_SDK_ROOT) {
        const localPropsContent = `sdk.dir=${process.env.ANDROID_SDK_ROOT.replace(/\\/g, '\\\\')}\n`;
        fs.writeFileSync(localPropertiesPath, localPropsContent);
        console.log('Created local.properties with Android SDK path');
      }
      
      console.log('LlamaCppRn Android setup completed with robust FBJNI header injection');
      return config;
    },
  ]);
};

module.exports = withLlamaCppRnAndroid;