const fs = require('fs');
const path = require('path');

// Paths
const rootDir = path.resolve(__dirname, '../..');
const sourceBaseDir = path.join(rootDir, 'android/src/main/jniLibs');
const targetBaseDir = path.join(__dirname, '../node_modules/@novastera-oss/llamacpp-rn');
const targetLibsDir = path.join(targetBaseDir, 'android/src/main/jniLibs');

// List of ABIs to copy
const abis = ['arm64-v8a', 'x86_64'];

// Copy function for native libraries
function copyLibraries() {
    if (!fs.existsSync(sourceBaseDir)) {
        console.error(`Source directory does not exist: ${sourceBaseDir}`);
        process.exit(1);
    }

    console.log('Copying native libraries...');
    console.log(`From: ${sourceBaseDir}`);
    console.log(`To: ${targetLibsDir}`);
    
    // Create target directory if it doesn't exist
    if (!fs.existsSync(targetLibsDir)) {
        fs.mkdirSync(targetLibsDir, { recursive: true });
    }
    
    // Copy each ABI directory
    for (const abi of abis) {
        const sourceAbiDir = path.join(sourceBaseDir, abi);
        const targetAbiDir = path.join(targetLibsDir, abi);
        
        if (!fs.existsSync(sourceAbiDir)) {
            console.warn(`Warning: ABI directory not found: ${sourceAbiDir}`);
            continue;
        }
        
        // Create target ABI directory if it doesn't exist
        if (!fs.existsSync(targetAbiDir)) {
            fs.mkdirSync(targetAbiDir, { recursive: true });
        }
        
        // Copy .so files
        const files = fs.readdirSync(sourceAbiDir);
        for (const file of files) {
            if (file.endsWith('.so')) {
                const sourcePath = path.join(sourceAbiDir, file);
                const targetPath = path.join(targetAbiDir, file);
                
                try {
                    fs.copyFileSync(sourcePath, targetPath);
                    console.log(`Copied: ${sourcePath} -> ${targetPath}`);
                } catch (error) {
                    console.warn(`Warning: Could not copy ${sourcePath}: ${error.message}`);
                }
            }
        }
    }
    
    console.log('Native libraries copied successfully!');
}

// Execute the copy
try {
    copyLibraries();
} catch (error) {
    console.error('Error copying native libraries:', error);
    process.exit(1);
} 