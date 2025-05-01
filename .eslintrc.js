module.exports = {
  extends: '@react-native/eslint-config',
  root: true,
  ignorePatterns: [
    'node_modules',
    'lib',
    'cpp/**',
    '*.cpp',
    '*.h',
    '*.mm',
    '*.m',
    '*.java',
    '*.kt',
    'example/**',
    'android/**',
    'ios/**',
    'scripts/**',
    'prebuilt/**'
  ],
  rules: {
    // Add any custom rules here
  }
}; 