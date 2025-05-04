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
  role: 'user' | 'assistant' | 'system' | 'tool';
  content: string;
  name?: string;
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
  const [toolMode, setToolMode] = React.useState(false);
  const [streamingTokens, setStreamingTokens] = React.useState<string[]>([]);
  
  const scrollViewRef = React.useRef<ScrollView>(null);
  
  // Define tool for weather function
  const weatherTool = {
    type: 'function',
    function: {
      name: 'weather',
      description: 'Gets the weather from a specific city',
      parameters: {
        type: 'object',
        properties: {
          city: {
            type: 'string',
            description: 'The city for the weather.',
          },
        },
        required: ['city'],
      },
    },
  };

  // Weather system prompt
  const weatherSystemPrompt = 'You are a helpful assistant that can give the weather in a specific city. If the user asks for weather and a city is not provided, you must ask which city they want. If the city is provided make the call to the weather tool.';
  
  // Reset to default mode
  const resetChat = () => {
    setMessages([
      { role: 'system', content: toolMode ? weatherSystemPrompt : 'You are a helpful AI assistant.' }
    ]);
    setTestResult(null);
  };
  
  // Toggle tool mode
  const toggleToolMode = () => {
    const newMode = !toolMode;
    setToolMode(newMode);
    
    // Update system prompt based on mode
    setMessages([
      { role: 'system', content: newMode ? weatherSystemPrompt : 'You are a helpful AI assistant.' }
    ]);
    
    setTestResult(newMode 
      ? 'Tool mode activated. Ask about the weather in a city.' 
      : 'Normal chat mode activated.');
  };
  
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
      console.log('Initializing model with optimal settings...', {
        model: modelPath,
        n_ctx: 512,       // Context size of 2048 is sufficient for most interactions
        n_batch: 128,      // Smaller batch size for better stability
        n_gpu_layers: 0,   // Force CPU only for reliability
        use_mlock: true,   // Keep model in RAM and prevent swapping to disk
      });
      const modelInstance = await LlamaCppRn.initLlama({
        model: modelPath,
        n_ctx: 512,       // Context size of 2048 is sufficient for most interactions
        n_batch: 128,      // Smaller batch size for better stability
        n_gpu_layers: 0,   // Force CPU only for reliability
        use_mlock: true,   // Keep model in RAM and prevent swapping to disk
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
  
  // Test basic completion with a simple prompt
  const testBasicCompletion = async () => {
    if (!model) return;
    setTestResult(null);
    
    try {
      console.log('Testing basic completion with a simple prompt');
      
      // Use a very simple prompt with minimal parameters
      const testPrompt = input || "Complete this sentence: The sky is";
      console.log('Test prompt:', testPrompt);
      
      // Use minimal parameters with safer defaults - avoid any complex options
      const response = await model.completion({
        prompt: testPrompt,  // Use prompt instead of messages
        temperature: 0.1,    // Very low temperature for more stability
        max_tokens: 20,      // Increased for better results
        n_gpu_layers: 0,
      });
      
      console.log('Basic completion result:', response);
      
      // Check for error in response
      if (response.finish_reason === 'error') {
        setTestResult(`Error: ${response.text}`);
        return;
      }
      
      setTestResult(`Basic completion result: "${response.text}"`);
    } catch (err) {
      console.error('Basic completion error:', err);
      
      // More detailed error information
      let errorMessage = 'Basic Completion Error: ';
      
      if (err instanceof Error) {
        errorMessage += err.message;
        
        // Log extended properties
        console.error('Error name:', err.name);
        console.error('Error stack:', err.stack);
        
        // Log additional properties
        for (const key in err) {
          if (Object.prototype.hasOwnProperty.call(err, key)) {
            console.error(`Error.${key}:`, (err as any)[key]);
          }
        }
      } else {
        errorMessage += String(err);
      }
      
      setTestResult(errorMessage);
    }
  };
  
  // Test streaming functionality
  const testStreaming = async () => {
    if (!model) return;
    setTestResult(null);
    setStreamingTokens([]);
    setGenerating(true);
    
    try {
      console.log('Testing streaming with simple prompt');
      const testPrompt = "Generate a short paragraph about streaming tokens";
      
      // Run a simple completion with streaming
      const response = await model.completion({
        prompt: testPrompt,
        temperature: 0.3,    
        top_p: 0.9,         
        top_k: 40,         
        max_tokens: 50,
        n_gpu_layers: 0,
      },
      // Add streaming callback function
      (data: { token: string }) => {
        handleStreamingToken(data.token);
      });
      
      setTestResult(`Streaming test complete. Generated: "${response.text}"`);
    } catch (err) {
      console.error('Streaming test error:', err);
      setTestResult(`Streaming Test Error: ${err instanceof Error ? err.message : String(err)}`);
    } finally {
      setGenerating(false);
    }
  };
  
  // Handle tool call
  const handleToolCall = async (toolCall: any) => {
    console.log('Tool call received:', toolCall);
    
    try {
      if (toolCall.function.name === 'weather') {
        // Parse the arguments
        const args = JSON.parse(toolCall.function.arguments);
        const city = args.city;
        
        // Simulate weather data
        const weatherData = {
          city,
          temperature: Math.floor(Math.random() * 30) + 5, // 5-35°C
          condition: ['Sunny', 'Cloudy', 'Rainy', 'Partly cloudy', 'Stormy'][Math.floor(Math.random() * 5)],
          humidity: Math.floor(Math.random() * 60) + 30, // 30-90%
        };
        
        // Create tool message
        const toolMessage: Message = {
          role: 'tool',
          content: JSON.stringify(weatherData),
          name: 'weather'
        };
        
        // Add the tool response to messages
        setMessages(prev => [...prev, toolMessage]);
        
        // Continue the conversation with the tool response
        return toolMessage;
      }
      
      throw new Error(`Unknown tool: ${toolCall.function.name}`);
    } catch (err) {
      console.error('Error handling tool call:', err);
      const errorMessage: Message = {
        role: 'tool',
        content: JSON.stringify({ error: `Error processing tool call: ${err}` }),
        name: toolCall.function.name
      };
      
      // Add the error message to messages
      setMessages(prev => [...prev, errorMessage]);
      return errorMessage;
    }
  };

  // Handle streaming token callback
  const handleStreamingToken = (token: string) => {
    console.log('Token received:', token);
    setStreamingTokens(prev => {
      const newTokens = [...prev, token];
      // Keep only the last 10 tokens to avoid overwhelming the UI
      if (newTokens.length > 10) {
        return newTokens.slice(newTokens.length - 10);
      }
      return newTokens;
    });
  };

  // Send a message to the model
  const sendMessage = async () => {
    if (!model || !input.trim()) return;
    
    const userMessage: Message = { role: 'user', content: input.trim() };
    setMessages(prev => [...prev, userMessage]);
    setInput('');
    setGenerating(true);
    setError(null); // Clear any previous errors
    setStreamingTokens([]); // Clear previous streaming tokens
    
    try {
      // Prepare the messages for completion
      const formattedMessages = [...messages, userMessage];
      
      console.log('Starting completion with', formattedMessages.length, 'messages');
      // Log message structure for debugging
      formattedMessages.forEach((msg, i) => {
        console.log(`Message ${i}: role=${msg.role}, content=${msg.content.substring(0, 50)}${msg.content.length > 50 ? '...' : ''}`);
      });
      
      // Prepare completion options with careful defaults
      const completionOptions: any = {
        messages: formattedMessages,
        temperature: 0.3,     // Lower temperature for more stability
        top_p: 0.85,          // More conservative top_p
        top_k: 40,            // Standard top_k value
        max_tokens: 400,      // Increased max tokens for better responses with tools
        stop: ["</s>", "<|im_end|>", "<|eot_id|>"], // Include <|eot_id|> for better compatibility
        n_gpu_layers: 0,
      };
      
      // Ensure system message exists at the beginning
      if (formattedMessages.length > 0 && formattedMessages[0].role !== 'system') {
        console.log('No system message found, adding default system message');
        completionOptions.system_prompt = 'You are a helpful AI assistant.';
      }
      
      // Add tools and jinja flag if in tool mode
      if (toolMode) {
        console.log('Tool mode enabled, adding weather tool');
        completionOptions.tools = [weatherTool];
        completionOptions.jinja = true;
        completionOptions.tool_choice = "auto";
      }
      
      console.log('Completion options:', JSON.stringify(completionOptions, null, 2));
      
      // Generate completion with streaming token callback
      console.log('Calling model.completion...');
      const response = await model.completion(
        completionOptions,
        // Add streaming callback function with enhanced logging
        (data: { token: string }) => {
          console.log('Streaming token received:', {
            token: data.token,
            timestamp: new Date().toISOString(),
            tokenLength: data.token.length
          });
          handleStreamingToken(data.token);
        }
      );
      
      console.log('Completion response:', response);
      
      // Check if we have tool calls
      if (response.tool_calls && response.tool_calls.length > 0) {
        console.log('Tool calls detected:', response.tool_calls);
        
        // Add assistant message with the tool call request
        const assistantMessage: Message = { 
          role: 'assistant', 
          content: response.text || 'I need to use a tool to answer that.'
        };
        setMessages(prev => [...prev, assistantMessage]);
        
        // Handle each tool call sequentially
        const toolResponses = [];
        for (const toolCall of response.tool_calls) {
          console.log('Processing tool call:', toolCall);
          const toolResponse = await handleToolCall(toolCall);
          toolResponses.push(toolResponse);
        }
        
        // Now get a follow-up response from the model with the tool results
        const allMessages = [
          ...messages, 
          userMessage, 
          assistantMessage,
          ...toolResponses
        ];
        
        console.log('Getting final response with tool results. Message count:', allMessages.length);
        
        // Get the final response
        const finalResponse = await model.completion({
          ...completionOptions,
          messages: allMessages,
        },
        // Add streaming callback function for final response
        (data: { token: string }) => {
          console.log('Final response token received:', {
            token: data.token,
            timestamp: new Date().toISOString(),
            tokenLength: data.token.length
          });
          handleStreamingToken(data.token);
        });
        
        // Add the final assistant response
        const finalMessage: Message = { 
          role: 'assistant', 
          content: finalResponse.text || 'I couldn\'t process the tool response.'
        };
        setMessages(prev => [...prev, finalMessage]);
      } else {
        // Standard response without tool calls
        const assistantMessage: Message = { 
          role: 'assistant', 
          content: response.text || 'Sorry, I couldn\'t generate a response.' 
        };
        setMessages(prev => [...prev, assistantMessage]);
      }
    } catch (err) {
      console.error('Completion error:', err);
      
      // Extract more detailed error information
      let errorMessage = err instanceof Error ? err.message : String(err);
      let diagnostics = '';
      
      // Check if there are detailed diagnostics in the error object
      if (err && typeof err === 'object') {
        console.log('Error object keys:', Object.keys(err));
        
        // Try to extract any useful properties
        try {
          const diagnosticEntries = [];
          for (const [key, value] of Object.entries(err)) {
            if (key !== 'message' && key !== 'stack') {
              diagnosticEntries.push(`- ${key}: ${JSON.stringify(value)}`);
            }
          }
          
          if (diagnosticEntries.length > 0) {
            diagnostics = '\n\nDiagnostics:\n' + diagnosticEntries.join('\n');
          }
          
          // Special handling for diagnostics property if it exists
          if ('diagnostics' in err) {
            const diag = (err as any).diagnostics;
            // Format diagnostics as a list
            diagnostics += '\n\nDetailed Diagnostics:\n';
            if (diag.messages_count) diagnostics += `- Messages: ${diag.messages_count}\n`;
            if (diag.message_roles) diagnostics += `- Roles: ${diag.message_roles.join(', ')}\n`;
            if (diag.temperature) diagnostics += `- Temperature: ${diag.temperature}\n`;
            if (diag.top_p) diagnostics += `- Top_p: ${diag.top_p}\n`;
            if (diag.top_k) diagnostics += `- Top_k: ${diag.top_k}\n`;
            if (diag.max_tokens) diagnostics += `- Max tokens: ${diag.max_tokens}\n`;
            if (diag.template_name) diagnostics += `- Template: ${diag.template_name}\n`;
          }
        } catch (e) {
          console.error('Error extracting diagnostics:', e);
          diagnostics = '\n\nError data available but could not be formatted.';
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
      
      // Try both models, but prioritize Llama-3.2-1B over Mistral
      const modelNames = [
        'Llama-3.2-1B-Instruct-Q4_K_M.gguf',     // Now prioritizing Llama-3.2-1B
        'Mistral-7B-Instruct-v0.3.Q4_K_M.gguf'   // Mistral as fallback
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
  
  // Format tool response JSON for display
  const formatToolResponse = (content: string): string => {
    try {
      // Try to parse as JSON for better display
      const data = JSON.parse(content);
      
      // Format weather data specially
      if (data.city && data.temperature !== undefined) {
        return `Weather in ${data.city}:\n• Temperature: ${data.temperature}°C\n• Condition: ${data.condition}\n• Humidity: ${data.humidity}%`;
      }
      
      // Format other JSON responses
      return Object.entries(data)
        .map(([key, value]) => `${key}: ${JSON.stringify(value)}`)
        .join('\n');
    } catch (e) {
      // If not valid JSON, return as is
      return content;
    }
  };
  
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
                style={[styles.testButton, toolMode && styles.activeToolButton]} 
                onPress={toggleToolMode}
              >
                <Text style={styles.testButtonText}>
                  {toolMode ? 'Tool Mode ✓' : 'Tool Mode'}
                </Text>
              </TouchableOpacity>
              <TouchableOpacity 
                style={styles.testButton} 
                onPress={resetChat}
              >
                <Text style={styles.testButtonText}>Reset Chat</Text>
              </TouchableOpacity>
              <TouchableOpacity 
                style={styles.testButton} 
                onPress={testTokenizer}
              >
                <Text style={styles.testButtonText}>Test Tokenizer</Text>
              </TouchableOpacity>
              <TouchableOpacity 
                style={styles.testButton} 
                onPress={testBasicCompletion}
              >
                <Text style={styles.testButtonText}>Test Basic Completion</Text>
              </TouchableOpacity>
              <TouchableOpacity 
                style={styles.testButton} 
                onPress={testStreaming}
              >
                <Text style={styles.testButtonText}>Test Streaming</Text>
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
          
          {toolMode && (
            <View style={styles.toolModeContainer}>
              <Text style={styles.toolModeText}>
                <Text style={styles.toolModeHighlight}>Tool Mode Active:</Text> Ask about the weather in a city
              </Text>
              <TouchableOpacity
                style={styles.exampleButton}
                onPress={() => setInput("What's the weather in New York?")}
              >
                <Text style={styles.exampleButtonText}>Try Example</Text>
              </TouchableOpacity>
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
                  msg.role === 'user' ? styles.userMessage : 
                  msg.role === 'assistant' ? styles.assistantMessage :
                  styles.toolMessage
                ]}
              >
                {msg.role === 'tool' && (
                  <Text style={styles.toolName}>
                    {msg.name || 'Tool Response'}
                  </Text>
                )}
                <Text style={styles.messageText}>
                  {msg.role === 'tool' ? formatToolResponse(msg.content) : msg.content}
                </Text>
              </View>
            ))}
            {generating && (
              <View style={[styles.message, styles.assistantMessage]}>
                <ActivityIndicator size="small" color="#333" />
                {streamingTokens.length > 0 && (
                  <View style={styles.streamingContainer}>
                    <Text style={styles.streamingTitle}>Latest tokens:</Text>
                    <Text style={styles.streamingTokens}>
                      {streamingTokens.map((token, i) => (
                        <Text key={i} style={styles.streamingToken}>
                          {token.replace(/\n/g, '⏎')}
                        </Text>
                      )).reverse()}
                    </Text>
                  </View>
                )}
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
  activeToolButton: {
    backgroundColor: '#28a745',
  },
  toolMessage: {
    alignSelf: 'flex-start',
    backgroundColor: '#f0f0f0',
  },
  toolName: {
    fontWeight: 'bold',
    fontSize: 14,
  },
  toolModeContainer: {
    marginVertical: 10,
    padding: 10,
    backgroundColor: '#e9ecef',
    borderRadius: 8,
    borderWidth: 1,
    borderColor: '#dee2e6',
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
  },
  toolModeText: {
    color: '#212529',
    fontSize: 14,
    flex: 1,
  },
  toolModeHighlight: {
    fontWeight: 'bold',
  },
  exampleButton: {
    backgroundColor: '#007bff',
    paddingHorizontal: 10,
    paddingVertical: 5,
    borderRadius: 4,
    marginLeft: 10,
  },
  exampleButtonText: {
    color: 'white',
    fontWeight: 'bold',
    fontSize: 11,
  },
  streamingContainer: {
    marginTop: 10,
    padding: 10,
    backgroundColor: '#e9ecef',
    borderRadius: 8,
    borderWidth: 1,
    borderColor: '#dee2e6',
  },
  streamingTitle: {
    fontWeight: 'bold',
    marginBottom: 8,
  },
  streamingTokens: {
    color: '#212529',
    fontSize: 14,
  },
  streamingToken: {
    marginBottom: 4,
  },
}); 