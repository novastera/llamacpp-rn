# Assets Directory

This directory contains assets used by the app, including the LLaMA.cpp model file.

## Model File

The model file `Mistral-7B-Instruct-v0.3.Q4_K_M.gguf` needs to be placed in this directory for the app to work properly. The model file is used by LLaMA.cpp to run text generation.

Due to the large size of the model file (typically 4GB+), it is not included in the repository. You will need to download it from a source like Hugging Face or other model repositories.

## Setup Instructions

1. Download the `Mistral-7B-Instruct-v0.3.Q4_K_M.gguf` model (or another compatible GGUF model)
2. Place the model file in this directory (example/assets/)
3. Run `npm run setup-model` from the example directory to copy the model to the iOS and Android projects
4. For iOS, you will need to manually add the model to the Xcode project:
   - Open example/ios/example.xcworkspace in Xcode
   - Right-click on the example project in the Navigator
   - Select "Add Files to example..."
   - Navigate to example/ios/example/models and select the model file
   - Make sure "Create folder references" is selected and click "Add"

## Testing Without the Model

If you just want to test the basic functionality of the module without loading a model, you can use the "JSON Schema to GBNF" test which doesn't require a model file. 