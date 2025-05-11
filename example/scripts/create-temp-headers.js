const fs = require('fs');
const path = require('path');

// Paths
const rootDir = path.resolve(__dirname, '../..');
const targetBaseDir = path.join(__dirname, '../node_modules/@novastera-oss/llamacpp-rn');
const targetDir = path.join(targetBaseDir, 'tm');
const llamaCppDir = path.join(rootDir, 'tm', 'llama.cpp');

// Create temporary headers in the node_modules directory
console.log('Creating temporary header files in node_modules directory...');

// Helper function to ensure directories exist
function ensureDirExists(dir) {
    if (!fs.existsSync(dir)) {
        fs.mkdirSync(dir, { recursive: true });
        console.log(`Created directory: ${dir}`);
    }
}

// Copy llama.cpp headers to the node_modules cpp directory
function createTempHeaders() {
    // Ensure target directory exists
    ensureDirExists(targetDir);
    
    // Main headers from llama.cpp/include
    if (fs.existsSync(path.join(llamaCppDir, 'include'))) {
        const includeFiles = fs.readdirSync(path.join(llamaCppDir, 'include'));
        includeFiles.forEach(file => {
            if (file.endsWith('.h') || file.endsWith('.hpp')) {
                const srcPath = path.join(llamaCppDir, 'include', file);
                const destPath = path.join(targetDir, file);
                fs.copyFileSync(srcPath, destPath);
                console.log(`Created temporary header: ${destPath}`);
            }
        });
    } else {
        console.warn(`Warning: llama.cpp include directory not found: ${path.join(llamaCppDir, 'include')}`);
    }
    
    // GGML headers from llama.cpp/ggml/include
    if (fs.existsSync(path.join(llamaCppDir, 'ggml/include'))) {
        const ggmlFiles = fs.readdirSync(path.join(llamaCppDir, 'ggml/include'));
        ggmlFiles.forEach(file => {
            if (file.endsWith('.h') || file.endsWith('.hpp')) {
                const srcPath = path.join(llamaCppDir, 'ggml/include', file);
                const destPath = path.join(targetDir, file);
                fs.copyFileSync(srcPath, destPath);
                console.log(`Created temporary GGML header: ${destPath}`);
            }
        });
    } else {
        console.warn(`Warning: llama.cpp ggml include directory not found: ${path.join(llamaCppDir, 'ggml/include')}`);
    }
    
    // Common headers from llama.cpp/common
    if (fs.existsSync(path.join(llamaCppDir, 'common'))) {
        const commonFiles = fs.readdirSync(path.join(llamaCppDir, 'common'));
        commonFiles.forEach(file => {
            if (file === 'minja') {
                // Skip directories for direct inclusion
                return;
            } else if (file.endsWith('.h') || file.endsWith('.hpp')) {
                const srcPath = path.join(llamaCppDir, 'common', file);
                const destPath = path.join(targetDir, file);
                fs.copyFileSync(srcPath, destPath);
                console.log(`Created temporary header: ${destPath}`);
            }
        });
    } else {
        console.warn(`Warning: llama.cpp common directory not found: ${path.join(llamaCppDir, 'common')}`);
    }
    
    console.log('Temporary header files created successfully!');
}

// Execute
try {
    createTempHeaders();
} catch (error) {
    console.error('Error creating temporary header files:', error);
    process.exit(1);
} 