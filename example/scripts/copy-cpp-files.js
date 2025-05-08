const fs = require('fs');
const path = require('path');

// Paths
const rootDir = path.resolve(__dirname, '../..');
const sourceDir = path.join(rootDir, 'cpp');
const llamaCppDir = path.join(rootDir, 'cpp', 'llama.cpp');
const targetBaseDir = path.join(__dirname, '../node_modules/@novastera-oss/llamacpp-rn');
const targetDir = path.join(targetBaseDir, 'cpp');
const targetIncludeDir = path.join(targetBaseDir, 'android/src/main/cpp/include');
const targetCommonDir = path.join(targetIncludeDir, 'common');
const targetMinjaDir = path.join(targetCommonDir, 'minja');
const targetGgmlDir = path.join(targetIncludeDir, 'ggml');

// Copy function
function copyFiles(source, target) {
    if (!fs.existsSync(source)) {
        console.error(`Source directory does not exist: ${source}`);
        process.exit(1);
    }

    const files = fs.readdirSync(source);
    
    files.forEach(file => {
        const sourcePath = path.join(source, file);
        const targetPath = path.join(target, file);
        
        const stat = fs.statSync(sourcePath);
        
        if (stat.isDirectory()) {
            if (file === '.git' || file === 'node_modules') {
                return; // Skip these directories
            }
            if (!fs.existsSync(targetPath)) {
                fs.mkdirSync(targetPath, { recursive: true });
            }
            copyFiles(sourcePath, targetPath);
        } else {
            if (file.match(/\.(c|cpp|h|hpp|cc|cxx|inl)$/i)) {
                try {
                    // Create target directory if it doesn't exist
                    const targetDir = path.dirname(targetPath);
                    if (!fs.existsSync(targetDir)) {
                        fs.mkdirSync(targetDir, { recursive: true });
                    }
                    fs.copyFileSync(sourcePath, targetPath);
                    console.log(`Copied: ${sourcePath} -> ${targetPath}`);
                } catch (error) {
                    console.warn(`Warning: Could not copy ${sourcePath}: ${error.message}`);
                }
            }
        }
    });
}

// Helper function to ensure directories exist
function ensureDirExists(dir) {
    if (!fs.existsSync(dir)) {
        fs.mkdirSync(dir, { recursive: true });
        console.log(`Created directory: ${dir}`);
    }
}

// Copy llama.cpp headers to the include directory
function copyLlamaCppHeaders() {
    console.log('Copying llama.cpp headers to include directory...');
    
    // Ensure target directories exist
    ensureDirExists(targetIncludeDir);
    ensureDirExists(targetCommonDir);
    ensureDirExists(targetMinjaDir);
    ensureDirExists(targetGgmlDir);
    
    // Copy main headers
    if (fs.existsSync(path.join(llamaCppDir, 'include'))) {
        const includeFiles = fs.readdirSync(path.join(llamaCppDir, 'include'));
        includeFiles.forEach(file => {
            if (file.endsWith('.h') || file.endsWith('.hpp')) {
                const srcPath = path.join(llamaCppDir, 'include', file);
                const destPath = path.join(targetIncludeDir, file);
                fs.copyFileSync(srcPath, destPath);
                console.log(`Copied include header: ${srcPath} -> ${destPath}`);
            }
        });
    } else {
        console.warn(`Warning: llama.cpp include directory not found: ${path.join(llamaCppDir, 'include')}`);
    }
    
    // Copy GGML headers - only to node_modules, not to the root
    if (fs.existsSync(path.join(llamaCppDir, 'ggml/include'))) {
        const ggmlFiles = fs.readdirSync(path.join(llamaCppDir, 'ggml/include'));
        ggmlFiles.forEach(file => {
            if (file.endsWith('.h') || file.endsWith('.hpp')) {
                const srcPath = path.join(llamaCppDir, 'ggml/include', file);
                const destPath = path.join(targetGgmlDir, file);
                fs.copyFileSync(srcPath, destPath);
                console.log(`Copied ggml header: ${srcPath} -> ${destPath}`);
            }
        });
    } else {
        console.warn(`Warning: llama.cpp ggml include directory not found: ${path.join(llamaCppDir, 'ggml/include')}`);
    }
    
    // Copy common headers
    if (fs.existsSync(path.join(llamaCppDir, 'common'))) {
        const commonFiles = fs.readdirSync(path.join(llamaCppDir, 'common'));
        commonFiles.forEach(file => {
            if (file === 'minja') {
                // Handle minja directory separately
                if (fs.existsSync(path.join(llamaCppDir, 'common', 'minja'))) {
                    const minjaFiles = fs.readdirSync(path.join(llamaCppDir, 'common', 'minja'));
                    minjaFiles.forEach(minjaFile => {
                        if (minjaFile.endsWith('.h') || minjaFile.endsWith('.hpp')) {
                            const srcPath = path.join(llamaCppDir, 'common', 'minja', minjaFile);
                            const destPath = path.join(targetMinjaDir, minjaFile);
                            fs.copyFileSync(srcPath, destPath);
                            console.log(`Copied minja header: ${srcPath} -> ${destPath}`);
                        }
                    });
                }
            } else if (file.endsWith('.h') || file.endsWith('.hpp')) {
                const srcPath = path.join(llamaCppDir, 'common', file);
                const destPath = path.join(targetCommonDir, file);
                fs.copyFileSync(srcPath, destPath);
                console.log(`Copied common header: ${srcPath} -> ${destPath}`);
            }
        });
        
        // Create a copy of common.h in the main include directory for easier access
        if (fs.existsSync(path.join(targetCommonDir, 'common.h'))) {
            const commonSrc = path.join(targetCommonDir, 'common.h');
            const commonDest = path.join(targetIncludeDir, 'common.h');
            fs.copyFileSync(commonSrc, commonDest);
            console.log(`Created copy of common.h: ${commonSrc} -> ${commonDest}`);
        }
    } else {
        console.warn(`Warning: llama.cpp common directory not found: ${path.join(llamaCppDir, 'common')}`);
    }
}

// Execute copy
try {
    console.log('Copying C++ files...');
    console.log(`From: ${sourceDir}`);
    console.log(`To: ${targetDir}`);
    
    // Create target directory if it doesn't exist
    if (!fs.existsSync(targetDir)) {
        fs.mkdirSync(targetDir, { recursive: true });
    }
    
    copyFiles(sourceDir, targetDir);
    
    // Copy llama.cpp headers
    copyLlamaCppHeaders();
    
    console.log('C++ files copied successfully!');
} catch (error) {
    console.error('Error copying C++ files:', error);
    process.exit(1);
} 