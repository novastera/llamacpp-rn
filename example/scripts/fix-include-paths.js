const fs = require('fs');
const path = require('path');

// Paths
const rootDir = path.resolve(__dirname, '../..');
const sourceDir = path.join(rootDir, 'cpp');
const targetBaseDir = path.join(__dirname, '../node_modules/@novastera-oss/llamacpp-rn');
const targetDir = path.join(targetBaseDir, 'cpp');

// Pattern to look for and replace
const includePattern = /#include\s+["<](.*?)[">]/g;

// Headers that need to use direct inclusion (not path-based)
const directIncludeHeaders = [
  'common.h',
  'llama.h',
  'llama-cpp.h',
  'sampling.h',
  'chat.h',
  'log.h',
  'ngram-cache.h',
  'json-schema-to-grammar.h',
  'speculative.h',
  'base64.hpp',
  'json.hpp'
];

// Function to fix include paths in a file
function fixIncludePaths(filePath) {
  if (!fs.existsSync(filePath)) {
    console.warn(`Warning: File not found: ${filePath}`);
    return;
  }
  
  console.log(`Fixing include paths in: ${filePath}`);
  let content = fs.readFileSync(filePath, 'utf8');
  
  // Replace include paths for specific headers
  content = content.replace(includePattern, (match, includePath) => {
    // Check if this is one of our direct include headers
    if (directIncludeHeaders.includes(path.basename(includePath))) {
      return `#include "${path.basename(includePath)}"`;
    }
    return match;
  });
  
  fs.writeFileSync(filePath, content);
  console.log(`Updated: ${filePath}`);
}

// Process all C++ files in the cpp directory
function processAllFiles(dir) {
  const entries = fs.readdirSync(dir, { withFileTypes: true });
  
  for (const entry of entries) {
    const fullPath = path.join(dir, entry.name);
    
    if (entry.isDirectory()) {
      // Skip specific directories
      if (entry.name === 'llama.cpp' || entry.name === 'node_modules') {
        continue;
      }
      processAllFiles(fullPath);
    } else if (entry.name.match(/\.(c|cpp|h|hpp|cc|cxx)$/i)) {
      fixIncludePaths(fullPath);
    }
  }
}

// Execute
try {
  console.log('Fixing include paths in C++ files...');
  processAllFiles(sourceDir);
  console.log('Include paths fixed successfully!');
} catch (error) {
  console.error('Error fixing include paths:', error);
  process.exit(1);
} 