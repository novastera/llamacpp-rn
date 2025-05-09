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
      
      // Add FBJNI dependency to llamacpp-rn build.gradle
      const moduleBuildGradlePath = path.join(
        config.modRequest.platformProjectRoot,
        '../../android/build.gradle'
      );
      
      if (fs.existsSync(moduleBuildGradlePath)) {
        let buildGradleContent = fs.readFileSync(moduleBuildGradlePath, 'utf8');
        
        // Add fbjni dependency if not present
        if (!buildGradleContent.includes("implementation 'com.facebook.fbjni:fbjni")) {
          const dependenciesPos = buildGradleContent.indexOf('dependencies {');
          if (dependenciesPos !== -1) {
            const afterDependencies = buildGradleContent.indexOf('\n', dependenciesPos) + 1;
            
            const beforeDep = buildGradleContent.substring(0, afterDependencies);
            const afterDep = buildGradleContent.substring(afterDependencies);
            
            buildGradleContent = 
              beforeDep + 
              "    // Add fbjni dependency\n" +
              "    implementation 'com.facebook.fbjni:fbjni:0.7.0'\n" +
              "\n" +
              afterDep;
            
            fs.writeFileSync(moduleBuildGradlePath, buildGradleContent);
            console.log('Added fbjni dependency to llamacpp-rn build.gradle');
          }
        }
      }
      
      // Ensure jniLibs directory exists in app
      const appJniLibsPath = path.join(
        config.modRequest.platformProjectRoot,
        'app/src/main/jniLibs'
      );
      const expoJniLibsPath = path.join(
        config.modRequest.platformProjectRoot,
        '../../node_modules/llamacpp-rn/android/src/main/jniLibs'
      );
      
      // Create the jniLibs directory if it doesn't exist
      if (!fs.existsSync(appJniLibsPath)) {
        try {
          fs.mkdirSync(appJniLibsPath, { recursive: true });
          console.log('Created jniLibs directory in app');
        } catch (error) {
          console.warn('Failed to create jniLibs directory:', error.message);
        }
      }
      
      // Check if the prebuilt libraries directory exists
      if (fs.existsSync(expoJniLibsPath)) {
        const archDirs = ['arm64-v8a', 'x86_64'];
        
        // Copy prebuilt libraries for each architecture
        for (const arch of archDirs) {
          const sourceArchDir = path.join(expoJniLibsPath, arch);
          const targetArchDir = path.join(appJniLibsPath, arch);
          
          if (fs.existsSync(sourceArchDir)) {
            // Create architecture directory if it doesn't exist
            if (!fs.existsSync(targetArchDir)) {
              try {
                fs.mkdirSync(targetArchDir, { recursive: true });
              } catch (error) {
                console.warn(`Failed to create ${arch} directory:`, error.message);
                continue;
              }
            }
            
            // Copy libllama.so if it exists
            const sourceLlamaLib = path.join(sourceArchDir, 'libllama.so');
            const targetLlamaLib = path.join(targetArchDir, 'libllama.so');
            
            if (fs.existsSync(sourceLlamaLib) && !fs.existsSync(targetLlamaLib)) {
              try {
                fs.copyFileSync(sourceLlamaLib, targetLlamaLib);
                console.log(`Copied libllama.so for ${arch}`);
              } catch (error) {
                console.warn(`Failed to copy libllama.so for ${arch}:`, error.message);
              }
            }
          }
        }
      } else {
        console.warn('Prebuilt libraries directory not found at:', expoJniLibsPath);
      }
      
      console.log('LlamaCppRn Android setup completed');
      return config;
    },
  ]);
};

module.exports = withLlamaCppRnAndroid;