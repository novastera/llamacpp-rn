import * as React from 'react';
import { 
  View, 
  Text, 
  StyleSheet, 
  Button, 
  ActivityIndicator, 
  TextInput, 
  ScrollView,
  TouchableOpacity,
  KeyboardAvoidingView,
  Platform
} from 'react-native';
import * as FileSystem from 'expo-file-system';

// Using require instead of import to avoid TypeScript errors
const LlamaCppRn = require('@novastera-oss/llamacpp-rn');

interface Message {
  role: 'user' | 'assistant' | 'system';
  content: string;
}

export default function ModelChatTest() {
  const [model, setModel] = React.useState<any>(null);
  const [loading, setLoading] = React.useState(false);
  const [error, setError] = React.useState<string | null>(null);
  const [input, setInput] = React.useState('');
  const [messages, setMessages] = React.useState<Message[]>([
    { role: 'system', content: 'You are a helpful AI assistant.' }
  ]);
  const [generating, setGenerating] = React.useState(false);
  const [modelInfo, setModelInfo] = React.useState<any>(null);
  
  const scrollViewRef = React.useRef<ScrollView>(null);
  
  // Find and initialize model
  const initializeModel = async () => {
    setLoading(true);
    setError(null);
    
    try {
      // Find a model to use
      const modelPath = await findModel();
      if (!modelPath) {
        throw new Error('No model found. Please add a model to your app.');
      }
      
      console.log('Initializing model from:', modelPath);
      
      // First get model info
      const info = await LlamaCppRn.loadLlamaModelInfo(modelPath);
      setModelInfo(info);
      console.log('Model info:', info);
      
      // Initialize with optimal settings - the C++ module will handle GPU fallback gracefully
      console.log('Initializing model with optimal settings...');
      const modelInstance = await LlamaCppRn.initLlama({
        model: modelPath,
        n_ctx: 2048,
        n_batch: 512,
        n_gpu_layers: 42, // Use 42 GPU layers, the module will check support and fall back if needed
        use_mlock: true, // Keep model in RAM and prevent swapping to disk
      });
      
      console.log('Model initialized successfully');
      setModel(modelInstance);
      
      // Store initialization notes
      setModelInfo((prev: any) => ({
        ...prev,
        initNotes: ['Model initialized with optimal settings']
      }));
      
    } catch (err) {
      console.error('Model initialization failed:', err);
      
      // Get detailed error properties
      if (err instanceof Error) {
        console.error('Error name:', err.name);
        console.error('Error message:', err.message);
        console.error('Error stack:', err.stack);
        
        // Log any additional error properties
        const anyErr = err as any;
        Object.keys(anyErr).forEach(key => {
          if (key !== 'name' && key !== 'message' && key !== 'stack') {
            console.error(`Error.${key}:`, anyErr[key]);
          }
        });
      }
      
      setError(`Error: ${err instanceof Error ? err.message : String(err)}`);
    } finally {
      setLoading(false);
    }
  };
  
  // Unload the model
  const unloadModel = async () => {
    if (model) {
      try {
        await model.release();
        console.log('Model unloaded successfully');
        setModel(null);
      } catch (err) {
        console.error('Failed to unload model:', err);
      }
    }
  };
  
  // Send a message to the model
  const sendMessage = async () => {
    if (!model || !input.trim()) return;
    
    const userMessage: Message = { role: 'user', content: input.trim() };
    setMessages(prev => [...prev, userMessage]);
    setInput('');
    setGenerating(true);
    
    try {
      // Prepare the messages for completion
      const formattedMessages = [...messages, userMessage];
      
      // Generate completion
      const response = await model.completion({
        messages: formattedMessages,
        temperature: 0.7,
        top_p: 0.95,
        top_k: 40,
        max_tokens: 500,
        stop: ["</s>", "<|im_end|>"]
      });
      
      // Add the response to messages
      const assistantMessage: Message = { 
        role: 'assistant', 
        content: response.text || 'Sorry, I couldn\'t generate a response.' 
      };
      setMessages(prev => [...prev, assistantMessage]);
    } catch (err) {
      console.error('Completion error:', err);
      setError(`Error generating response: ${err instanceof Error ? err.message : String(err)}`);
    } finally {
      setGenerating(false);
    }
  };
  
  // Find a suitable model to use
  const findModel = async (): Promise<string | null> => {
    try {
      const bundleDir = FileSystem.bundleDirectory || '';
      const documentsDir = FileSystem.documentDirectory || '';
      
      console.log('Bundle directory:', bundleDir);
      console.log('Documents directory:', documentsDir);
      
      // Always prioritize the specific 1B model
      const llamaModelName = 'Llama-3.2-1B-Instruct-Q4_K_M.gguf';
      const documentsModelPath = `${documentsDir}${llamaModelName}`;
      
      // First check if the model exists in the documents directory
      const documentsModelExists = await FileSystem.getInfoAsync(documentsModelPath);
      if (documentsModelExists.exists) {
        console.log('Found Llama 1B model in documents directory:', documentsModelPath);
        return documentsModelPath;
      }
      
      // Check in example/assets folder directly (used during development)
      try {
        // Try checking in the assets directory from the example project
        const localAssetsPath = '../assets/Llama-3.2-1B-Instruct-Q4_K_M.gguf';
        console.log('Checking for model in local assets:', localAssetsPath);
        await FileSystem.downloadAsync(
          localAssetsPath,
          documentsModelPath
        ).catch(err => console.log('Download error (expected):', err.message));
      } catch (e) {
        console.log('Local assets check failed (expected):', e);
      }
      
      // Check documents directory again in case download succeeded
      const checkAgain = await FileSystem.getInfoAsync(documentsModelPath);
      if (checkAgain.exists) {
        console.log('Found model in documents directory after download attempt');
        return documentsModelPath;
      }
      
      // Try to find the model in various locations
      const possibleModelPaths = [
        // Check in bundle root
        `${bundleDir}${llamaModelName}`,
        
        // Check in assets directory - handle error directly here
        bundleDir && `${bundleDir}assets/${llamaModelName}`,
        
        // Check in example/assets
        `${bundleDir}example/assets/${llamaModelName}`,
      ].filter(Boolean) as string[]; // Remove any undefined entries
      
      // Log all paths we're checking
      console.log('Checking possible model paths:', possibleModelPaths);
      
      // Check each specific path
      for (const path of possibleModelPaths) {
        try {
          const info = await FileSystem.getInfoAsync(path);
          if (info.exists) {
            console.log('Found Llama 1B model at:', path);
            
            // Copy to documents directory for reliability
            try {
              console.log(`Copying model from ${path} to ${documentsModelPath}...`);
              await FileSystem.copyAsync({
                from: path,
                to: documentsModelPath
              });
              
              // Verify the copy
              const copiedInfo = await FileSystem.getInfoAsync(documentsModelPath);
              if (copiedInfo.exists) {
                console.log('Successfully copied model to documents directory');
                return documentsModelPath;
              }
            } catch (copyError) {
              console.error('Error copying model to documents directory:', copyError);
              // Continue with the original path if copy fails
              return path;
            }
            
            return path;
          }
        } catch (pathError) {
          console.log(`Error checking path ${path}:`, pathError);
          // Continue to next path
        }
      }
      
      console.log('Specific Llama model not found, looking for any GGUF file...');
      
      // Try to find any GGUF model as backup
      let allGgufFiles: string[] = [];
      
      // Check for models in the bundle directory
      try {
        console.log('Checking for GGUF files in bundle directory...');
        const bundleFiles = await getGgufFilesInDir(bundleDir);
        allGgufFiles = [...allGgufFiles, ...bundleFiles];
      } catch (e) {
        console.log('Error listing bundle directory:', e);
      }
      
      // Check for models in documents directory
      try {
        console.log('Checking for GGUF files in documents directory...');
        const docFiles = await getGgufFilesInDir(documentsDir);
        allGgufFiles = [...allGgufFiles, ...docFiles];
      } catch (e) {
        console.log('Error listing documents directory:', e);
      }
      
      console.log('All possible GGUF files:', allGgufFiles);
      
      // Check each location to see if the file exists
      for (const path of allGgufFiles) {
        try {
          const info = await FileSystem.getInfoAsync(path);
          if (info.exists) {
            console.log('Found GGUF model at:', path);
            
            // Try to validate this is a real model by loading info
            try {
              const modelInfo = await LlamaCppRn.loadLlamaModelInfo(path);
              console.log('Successfully loaded model info:', modelInfo);
              // If we get here, it's a valid model
              return path;
            } catch (validateError) {
              console.log(`Model at ${path} is not valid:`, validateError);
              // Continue to next model
            }
          }
        } catch (e) {
          console.log(`Error checking path ${path}:`, e);
        }
      }
      
      // If we get here, no real models were found
      throw new Error('No valid GGUF models found. Please use the "Prepare Model for Chat" button in the basic test to ensure a model is available.');
    } catch (error) {
      console.error('Error finding model:', error);
      return null;
    }
  };
  
  // Get all .gguf files in a directory
  const getGgufFilesInDir = async (dirPath: string): Promise<string[]> => {
    try {
      const files = await FileSystem.readDirectoryAsync(dirPath);
      return files
        .filter(file => file.endsWith('.gguf'))
        .map(file => `${dirPath}${file}`);
    } catch {
      return [];
    }
  };
  
  // Scroll to bottom of messages
  React.useEffect(() => {
    setTimeout(() => {
      scrollViewRef.current?.scrollToEnd({ animated: true });
    }, 100);
  }, [messages]);
  
  return (
    <View style={styles.container}>
      <Text style={styles.title}>Model Chat Test</Text>
      
      {!model ? (
        <View style={styles.initContainer}>
          <Button 
            title="Initialize Model"
            onPress={initializeModel}
            disabled={loading} 
          />
          {loading && (
            <View style={styles.loaderContainer}>
              <ActivityIndicator size="large" color="#007bff" style={styles.loader} />
              <Text style={styles.loadingText}>
                {modelInfo ? 'Initializing model...' : 'Loading model information...'}
              </Text>
            </View>
          )}
          {error && (
            <View style={styles.errorContainer}>
              <Text style={styles.error}>{error}</Text>
              {error && error.includes("No valid GGUF models found") && (
                <Text style={styles.errorHelper}>
                  Please go to the basic test screen and use the "Prepare Model for Chat" button to ensure a valid model is available.
                </Text>
              )}
            </View>
          )}
          {modelInfo && !loading && (
            <View style={styles.infoBox}>
              <Text style={styles.infoTitle}>Model Information:</Text>
              <Text>Parameters: {modelInfo.n_params.toLocaleString()}</Text>
              <Text>Context: {modelInfo.n_context}</Text>
              <Text>Vocab Size: {modelInfo.n_vocab}</Text>
              <Text>Description: {modelInfo.description}</Text>
              <Text>GPU Support: {modelInfo.gpuSupported ? 'Available' : 'Not available'}</Text>
              
              {modelInfo.initNotes && (
                <View style={styles.notesSection}>
                  <Text style={styles.notesTitle}>Initialization Notes:</Text>
                  {modelInfo.initNotes.map((note: string, index: number) => (
                    <Text key={index} style={styles.noteItem}>• {note}</Text>
                  ))}
                </View>
              )}
            </View>
          )}
        </View>
      ) : (
        <KeyboardAvoidingView
          behavior={Platform.OS === 'ios' ? 'padding' : 'height'}
          style={styles.chatContainer}
          keyboardVerticalOffset={100}
        >
          <View style={styles.header}>
            <Text style={styles.modelName}>
              {modelInfo?.description || 'AI Assistant'}
            </Text>
            <TouchableOpacity 
              style={styles.unloadButton} 
              onPress={unloadModel}
            >
              <Text style={styles.unloadButtonText}>Unload</Text>
            </TouchableOpacity>
          </View>
          
          <ScrollView 
            style={styles.messagesContainer}
            ref={scrollViewRef}
            contentContainerStyle={styles.messagesContent}
          >
            {messages.filter(msg => msg.role !== 'system').map((msg, index) => (
              <View 
                key={index}
                style={[
                  styles.message,
                  msg.role === 'user' ? styles.userMessage : styles.assistantMessage
                ]}
              >
                <Text style={styles.messageText}>{msg.content}</Text>
              </View>
            ))}
            {generating && (
              <View style={[styles.message, styles.assistantMessage]}>
                <ActivityIndicator size="small" color="#333" />
              </View>
            )}
          </ScrollView>
          
          <View style={styles.inputContainer}>
            <TextInput
              style={styles.input}
              value={input}
              onChangeText={setInput}
              placeholder="Type a message..."
              multiline
              maxLength={500}
              editable={!generating}
            />
            <TouchableOpacity
              style={[
                styles.sendButton,
                (!input.trim() || generating) && styles.sendButtonDisabled
              ]}
              onPress={sendMessage}
              disabled={!input.trim() || generating}
            >
              <Text style={styles.sendButtonText}>Send</Text>
            </TouchableOpacity>
          </View>
        </KeyboardAvoidingView>
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    padding: 16,
    backgroundColor: '#f8f9fa',
    borderRadius: 8,
    marginTop: 20,
    minHeight: 400,
  },
  title: {
    fontSize: 18,
    fontWeight: 'bold',
    marginBottom: 16,
  },
  initContainer: {
    alignItems: 'center',
    paddingVertical: 20,
  },
  loader: {
    marginTop: 16,
  },
  error: {
    color: 'red',
    marginTop: 16,
    textAlign: 'center',
  },
  errorContainer: {
    marginTop: 16,
    padding: 12,
    backgroundColor: '#ffeeee',
    borderRadius: 8,
    borderWidth: 1,
    borderColor: '#ffcccc',
  },
  errorHelper: {
    marginTop: 8,
    fontSize: 14,
    color: '#555',
    textAlign: 'center',
  },
  infoBox: {
    marginTop: 20,
    padding: 16,
    backgroundColor: '#e9ecef',
    borderRadius: 8,
    width: '100%',
  },
  infoTitle: {
    fontWeight: 'bold',
    marginBottom: 8,
  },
  chatContainer: {
    flex: 1,
    height: 500,
  },
  header: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingVertical: 10,
    marginBottom: 10,
    borderBottomWidth: 1,
    borderBottomColor: '#dee2e6',
  },
  modelName: {
    fontWeight: 'bold',
    fontSize: 16,
  },
  unloadButton: {
    backgroundColor: '#dc3545',
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: 4,
  },
  unloadButtonText: {
    color: 'white',
    fontWeight: 'bold',
    fontSize: 12,
  },
  messagesContainer: {
    flex: 1,
    marginBottom: 10,
  },
  messagesContent: {
    paddingVertical: 10,
  },
  message: {
    maxWidth: '80%',
    borderRadius: 12,
    padding: 12,
    marginVertical: 4,
  },
  userMessage: {
    alignSelf: 'flex-end',
    backgroundColor: '#007bff',
  },
  assistantMessage: {
    alignSelf: 'flex-start',
    backgroundColor: '#e9ecef',
  },
  messageText: {
    color: '#212529',
    fontSize: 16,
  },
  inputContainer: {
    flexDirection: 'row',
    marginTop: 8,
  },
  input: {
    flex: 1,
    borderWidth: 1,
    borderColor: '#ced4da',
    borderRadius: 20,
    paddingHorizontal: 16,
    paddingVertical: 8,
    maxHeight: 100,
    backgroundColor: 'white',
  },
  sendButton: {
    backgroundColor: '#007bff',
    borderRadius: 20,
    width: 60,
    justifyContent: 'center',
    alignItems: 'center',
    marginLeft: 8,
  },
  sendButtonDisabled: {
    backgroundColor: '#6c757d',
    opacity: 0.7,
  },
  sendButtonText: {
    color: 'white',
    fontWeight: 'bold',
  },
  notesSection: {
    marginTop: 10,
    padding: 10,
    backgroundColor: '#f0f0f0',
    borderRadius: 8,
  },
  notesTitle: {
    fontWeight: 'bold',
    marginBottom: 8,
  },
  noteItem: {
    marginBottom: 4,
  },
  loaderContainer: {
    marginTop: 16,
    alignItems: 'center',
    justifyContent: 'center',
  },
  loadingText: {
    marginTop: 8,
    fontSize: 16,
    color: '#333',
  },
}); 