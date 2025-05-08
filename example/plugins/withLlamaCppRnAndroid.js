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
      
      console.log('LlamaCppRn Android setup completed (minimal mode)');
      return config;
    },
  ]);
};

module.exports = withLlamaCppRnAndroid;