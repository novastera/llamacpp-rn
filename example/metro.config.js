const path = require('path');
const { getDefaultConfig } = require('expo/metro-config');
const exclusionList = require('metro-config/src/defaults/exclusionList');

// Define our own escape function since the 'escape-string-regexp' package is causing issues
function escapeStringRegexp(string) {
  // Escape characters with special meaning in RegExp
  return string.replace(/[|\\{}()[\]^$+*?.]/g, '\\$&');
}

const root = path.resolve(__dirname, '..');
const pak = require('../package.json');

const defaultConfig = getDefaultConfig(__dirname);

/**
 * Metro configuration for a React Native app that links to a local package
 * https://facebook.github.io/metro/docs/configuration
 */
module.exports = {
  ...defaultConfig,
  projectRoot: __dirname,
  watchFolders: [root],

  // We need to make sure that only one version is loaded for peerDependencies
  resolver: {
    ...defaultConfig.resolver,
    blacklistRE: exclusionList(
      [].concat(
        // Exclude all node_modules from the parent directory
        // except for the parent package itself
        new RegExp(
          `^${escapeStringRegexp(path.resolve(root, 'node_modules'))}/(?!${escapeStringRegexp(pak.name)}/.*)`
        )
      )
    ),

    extraNodeModules: {
      // Resolve all react-native dependencies from the project node_modules
      'react': path.resolve(__dirname, 'node_modules/react'),
      'react-native': path.resolve(__dirname, 'node_modules/react-native'),
      // Resolve the package from the parent directory
      '@novastera-oss/llamacpp-rn': root,
    },
  },
}; 