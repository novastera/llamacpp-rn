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
  const [testResult, setTestResult] = React.useState<string | null>(null);
  
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
        n_ctx: 2048,       // Context size of 2048 is sufficient for most interactions
        n_batch: 128,      // Smaller batch size for better stability
        n_gpu_layers: 0,   // Force CPU only for reliability
        use_mlock: true,   // Keep model in RAM and prevent swapping to disk
        n_threads: 4,      // Explicitly set thread count to 4 for stability
      });
      
      console.log('Model initialized successfully');
      setModel(modelInstance);
      
      // Store initialization notes
      setModelInfo((prev: any) => ({
        ...prev,
        initNotes: ['Model initialized with conservative settings (CPU-only)']
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
        setTestResult('Model released successfully');
      } catch (err) {
        console.error('Failed to unload model:', err);
        setTestResult(`Release Error: ${err instanceof Error ? err.message : String(err)}`);
      }
    }
  };
  
  // Test tokenizer
  const testTokenizer = async () => {
    if (!model) return;
    setTestResult(null);
    
    try {
      console.log('Testing tokenizer with input:', input || 'Hello, how are you?');
      const testText = input || 'Hello, how are you?';
      const tokens = await model.tokenize(testText);
      console.log('Tokenization result:', tokens);
      setTestResult(`Tokenized to ${tokens.length} tokens: ${JSON.stringify(tokens)}`);
    } catch (err) {
      console.error('Tokenization error:', err);
      setTestResult(`Tokenization Error: ${err instanceof Error ? err.message : String(err)}`);
    }
  };
  
  // Test template detection
  const testTemplateDetection = async () => {
    if (!model) return;
    setTestResult(null);
    
    try {
      console.log('Testing template detection');
      const template = await model.detectTemplate(messages);
      console.log('Template detection result:', template);
      setTestResult(`Detected template: ${template}`);
    } catch (err) {
      console.error('Template detection error:', err);
      setTestResult(`Template Detection Error: ${err instanceof Error ? err.message : String(err)}`);
    }
  };
  
  // Test get built-in templates
  const testBuiltinTemplates = async () => {
    if (!model) return;
    setTestResult(null);
    
    try {
      console.log('Getting built-in templates');
      const templates = await model.getBuiltinTemplates();
      console.log('Built-in templates:', templates);
      setTestResult(`Built-in templates (${templates.length}): ${templates.join(', ')}`);
    } catch (err) {
      console.error('Get built-in templates error:', err);
      setTestResult(`Get Built-in Templates Error: ${err instanceof Error ? err.message : String(err)}`);
    }
  };
  
  // Test basic completion with a simple prompt
  const testBasicCompletion = async () => {
    if (!model) return;
    setTestResult(null);
    
    try {
      console.log('Testing basic completion with a simple prompt');
      
      // Use a very simple prompt with minimal parameters
      const testPrompt = input || "Complete this sentence: The sky is";
      console.log('Test prompt:', testPrompt);
      
      // Use minimal parameters with safer defaults
      const response = await model.completion({
        prompt: testPrompt,  // Use prompt instead of messages
        temperature: 0.1,    // Very low temperature for more stability
        top_p: 0.9,          // More conservative filtering 
        top_k: 40,           // Standard filtering
        max_tokens: 20,      // Increased for better results
      });
      
      console.log('Basic completion result:', response.text);
      setTestResult(`Basic completion result: "${response.text}"`);
    } catch (err) {
      console.error('Basic completion error:', err);
      setTestResult(`Basic Completion Error: ${err instanceof Error ? err.message : String(err)}`);
    }
  };
  
  // Send a message to the model
  const sendMessage = async () => {
    if (!model || !input.trim()) return;
    
    const userMessage: Message = { role: 'user', content: input.trim() };
    setMessages(prev => [...prev, userMessage]);
    setInput('');
    setGenerating(true);
    setError(null); // Clear any previous errors
    
    try {
      // Prepare the messages for completion
      const formattedMessages = [...messages, userMessage];
      
      console.log('Starting completion with', formattedMessages.length, 'messages');
      
      // Generate completion
      const response = await model.completion({
        messages: formattedMessages,
        temperature: 0.3,     // Lower temperature for more stability
        top_p: 0.85,          // More conservative top_p
        top_k: 40,            // Standard top_k value
        max_tokens: 100,      // Reduced max tokens for better stability
        stop: ["</s>", "<|im_end|>", "<|eot_id|>"] // Include <|eot_id|> for better compatibility
      });
      // Add the response to messages
      const assistantMessage: Message = { 
        role: 'assistant', 
        content: response.text || 'Sorry, I couldn\'t generate a response.' 
      };
      setMessages(prev => [...prev, assistantMessage]);
    } catch (err) {
      console.error('Completion error:', err);
      
      // Extract more detailed error information
      let errorMessage = err instanceof Error ? err.message : String(err);
      let diagnostics = '';
      
      // Check if there are detailed diagnostics in the error object
      if (err && typeof err === 'object' && 'diagnostics' in err) {
        const diag = (err as any).diagnostics;
        try {
          // Format diagnostics as a list
          diagnostics = '\n\nDiagnostics:\n';
          if (diag.messages_count) diagnostics += `- Messages: ${diag.messages_count}\n`;
          if (diag.message_roles) diagnostics += `- Roles: ${diag.message_roles.join(', ')}\n`;
          if (diag.temperature) diagnostics += `- Temperature: ${diag.temperature}\n`;
          if (diag.top_p) diagnostics += `- Top_p: ${diag.top_p}\n`;
          if (diag.top_k) diagnostics += `- Top_k: ${diag.top_k}\n`;
          if (diag.max_tokens) diagnostics += `- Max tokens: ${diag.max_tokens}\n`;
          if (diag.template_name) diagnostics += `- Template: ${diag.template_name}\n`;
        } catch (e) {
          diagnostics = '\n\nDiagnostics available but could not be formatted.';
        }
      }
      
      setError(`Error generating response: ${errorMessage}${diagnostics}`);
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
      
      // Try both models, but prioritize Mistral which is more stable
      const modelNames = [
        'Mistral-7B-Instruct-v0.3.Q4_K_M.gguf', // Prioritize Mistral for better stability
        'Llama-3.2-1B-Instruct-Q4_K_M.gguf'     // Fallback to Llama
      ];
      
      // Try each model in our preferred order
      for (const modelName of modelNames) {
        console.log(`Looking for ${modelName}...`);
        const documentsModelPath = `${documentsDir}${modelName}`;
        
        // First check if the model exists in the documents directory
        const documentsModelExists = await FileSystem.getInfoAsync(documentsModelPath);
        if (documentsModelExists.exists) {
          console.log(`Found ${modelName} in documents directory:`, documentsModelPath);
          return documentsModelPath;
        }
        
        // Try to find the model in various locations
        const possibleModelPaths = [
          // Check in bundle root
          `${bundleDir}${modelName}`,
          
          // Check in assets directory
          bundleDir && `${bundleDir}assets/${modelName}`,
          
          // Check in example/assets
          `${bundleDir}example/assets/${modelName}`,
        ].filter(Boolean) as string[]; // Remove any undefined entries
        
        // Log all paths we're checking
        console.log('Checking possible model paths:', possibleModelPaths);
        
        // Check each specific path
        for (const path of possibleModelPaths) {
          try {
            const info = await FileSystem.getInfoAsync(path);
            if (info.exists) {
              console.log(`Found ${modelName} at:`, path);
              
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
      }
      
      // If we get here, neither of our preferred models was found, so look for any GGUF file
      console.log('Specific models not found, looking for any GGUF file...');
      
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
            <View style={styles.headerButtons}>
              <TouchableOpacity 
                style={styles.testButton} 
                onPress={testTokenizer}
              >
                <Text style={styles.testButtonText}>Test Tokenizer</Text>
              </TouchableOpacity>
              <TouchableOpacity 
                style={styles.testButton} 
                onPress={testTemplateDetection}
              >
                <Text style={styles.testButtonText}>Test Template</Text>
              </TouchableOpacity>
              <TouchableOpacity 
                style={styles.testButton} 
                onPress={testBuiltinTemplates}
              >
                <Text style={styles.testButtonText}>Test Built-in Templates</Text>
              </TouchableOpacity>
              <TouchableOpacity 
                style={styles.testButton} 
                onPress={testBasicCompletion}
              >
                <Text style={styles.testButtonText}>Test Basic Completion</Text>
              </TouchableOpacity>
              <TouchableOpacity 
                style={styles.unloadButton} 
                onPress={unloadModel}
              >
                <Text style={styles.unloadButtonText}>Unload</Text>
              </TouchableOpacity>
            </View>
          </View>
          
          {testResult && (
            <View style={styles.testResultContainer}>
              <Text style={styles.testResultText}>{testResult}</Text>
            </View>
          )}
          
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
    flex: 1, // Take available space
  },
  headerButtons: {
    flexDirection: 'row',
    alignItems: 'center',
    flexWrap: 'wrap', // Allow buttons to wrap if needed
    justifyContent: 'flex-end',
  },
  testButton: {
    backgroundColor: '#007bff',
    paddingHorizontal: 10,
    paddingVertical: 5,
    borderRadius: 4,
    marginLeft: 6,
    marginBottom: 4,
  },
  testButtonText: {
    color: 'white',
    fontWeight: 'bold',
    fontSize: 11,
  },
  unloadButton: {
    backgroundColor: '#dc3545',
    paddingHorizontal: 10,
    paddingVertical: 5,
    borderRadius: 4,
    marginLeft: 6,
  },
  unloadButtonText: {
    color: 'white',
    fontWeight: 'bold',
    fontSize: 11,
  },
  testResultContainer: {
    marginVertical: 10,
    padding: 10,
    backgroundColor: '#e9ecef',
    borderRadius: 8,
    borderWidth: 1,
    borderColor: '#dee2e6',
    maxHeight: 120,
  },
  testResultText: {
    color: '#212529',
    fontSize: 14,
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