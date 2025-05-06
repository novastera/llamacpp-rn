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

type ModelMode = 'conversation' | 'tools' | 'embeddings';

interface ModelState {
  instance: any;
  info: any;
  mode: ModelMode;
}

// Model Loading Component
const ModelLoader: React.FC<{
  onModelLoaded: (state: ModelState) => void;
}> = ({ onModelLoaded }) => {
  const [loading, setLoading] = React.useState(false);
  const [error, setError] = React.useState<string | null>(null);

  const initializeModel = async (mode: ModelMode) => {
    setLoading(true);
    setError(null);
    
    try {
      const modelPath = await findModel();
      if (!modelPath) {
        throw new Error('No model found. Please add a model to your app.');
      }
      
      console.log('Initializing model from:', modelPath);
      
      // Get model info
      const info = await LlamaCppRn.loadLlamaModelInfo(modelPath);
      console.log('Model info:', info);
      
      // Initialize with mode-specific settings
      const initParams: any = {
        model: modelPath,
        n_ctx: 512,
        n_batch: 128,
        n_gpu_layers: 0,
        use_mlock: true,
      };

      // Add mode-specific parameters
      if (mode === 'tools') {
        initParams.use_jinja = true;
      }

      console.log('Initializing model with settings:', initParams);
      const modelInstance = await LlamaCppRn.initLlama(initParams);
      
      console.log('Model initialized successfully');
      
      onModelLoaded({
        instance: modelInstance,
        info,
        mode
      });
      
    } catch (err) {
      console.error('Model initialization failed:', err);
      setError(`Error: ${err instanceof Error ? err.message : String(err)}`);
    } finally {
      setLoading(false);
    }
  };

  return (
    <View style={styles.initContainer}>
      <Text style={styles.title}>Load for</Text>
      
      <View style={styles.modeButtons}>
        <TouchableOpacity 
          style={styles.modeButton}
          onPress={() => initializeModel('conversation')}
          disabled={loading}
        >
          <Text style={styles.modeButtonText}>Conversation</Text>
        </TouchableOpacity>
        
        <TouchableOpacity 
          style={styles.modeButton}
          onPress={() => initializeModel('tools')}
          disabled={loading}
        >
          <Text style={styles.modeButtonText}>Tools</Text>
        </TouchableOpacity>
        
        <TouchableOpacity 
          style={styles.modeButton}
          onPress={() => initializeModel('embeddings')}
          disabled={loading}
        >
          <Text style={styles.modeButtonText}>Embeddings</Text>
        </TouchableOpacity>
      </View>

      {loading && (
        <View style={styles.loaderContainer}>
          <ActivityIndicator size="large" color="#007bff" style={styles.loader} />
          <Text style={styles.loadingText}>Loading model...</Text>
        </View>
      )}

      {error && (
        <View style={styles.errorContainer}>
          <Text style={styles.error}>{error}</Text>
          {error.includes("No valid GGUF models found") && (
            <Text style={styles.errorHelper}>
              Please go to the basic test screen and use the "Prepare Model for Chat" button to ensure a valid model is available.
            </Text>
          )}
        </View>
      )}
    </View>
  );
};

// Model Header Component
const ModelHeader: React.FC<{
  modelInfo: any;
  mode: ModelMode;
  onUnload: () => void;
}> = ({ modelInfo, mode, onUnload }) => {
  return (
    <View style={styles.header}>
      <Text style={styles.modelName}>
        {modelInfo?.description || 'AI Assistant'} ({mode})
      </Text>
      <TouchableOpacity 
        style={styles.unloadButton} 
        onPress={onUnload}
      >
        <Text style={styles.unloadButtonText}>Unload Model</Text>
      </TouchableOpacity>
    </View>
  );
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
      'Mistral-7B-Instruct-v0.3.Q4_K_M.gguf',   // Mistral as fallback
      'Llama-3.2-1B-Instruct-Q4_K_M.gguf',     // Now prioritizing Llama-3.2-1B
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

// Main Component
export default function ModelChatTest() {
  const [modelState, setModelState] = React.useState<ModelState | null>(null);
  const [input, setInput] = React.useState('');
  const [messages, setMessages] = React.useState<Message[]>([
    { role: 'system', content: 'You are a helpful AI assistant.' }
  ]);
  const [generating, setGenerating] = React.useState(false);
  const [error, setError] = React.useState<string | null>(null);
  const [streamingTokens, setStreamingTokens] = React.useState<string[]>([]);
  
  const scrollViewRef = React.useRef<ScrollView>(null);
  
  // Define tool for weather function
  const weatherTool = {
    type: 'function',
    function: {
      name: 'weather',
      description: 'Gets the current weather information for a specific city',
      parameters: {
        type: 'object',
        properties: {
          city: {
            type: 'string',
            description: 'The city name for which to get weather information',
          },
        },
        required: ['city'],
      },
    },
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

  // Handle model loaded event
  const handleModelLoaded = (state: ModelState) => {
    // Set appropriate initial system message based on mode
    let initialMessages: Message[] = [];
    
    if (state.mode === 'tools') {
      // System message for the weather tool
      initialMessages = [{
        role: 'system',
        content: 'You are a helpful assistant that can access the current weather information. When a user asks about the weather in a specific city, use the weather tool to fetch that information. If no city is specified, ask the user which city they want to know about.'
      }];
    } else {
      // Default system message
      initialMessages = [{
        role: 'system',
        content: 'You are a helpful AI assistant.'
      }];
    }
    
    setMessages(initialMessages);
    setModelState(state);
  };

  // Handle model unload
  const handleUnload = async () => {
    if (modelState?.instance) {
      try {
        await modelState.instance.release();
        console.log('Model unloaded successfully');
        setModelState(null);
        setMessages([{ role: 'system', content: 'You are a helpful AI assistant.' }]);
      } catch (err) {
        console.error('Failed to unload model:', err);
        setError(`Release Error: ${err instanceof Error ? err.message : String(err)}`);
      }
    }
  };

  // Handle streaming token callback
  const handleStreamingToken = (token: string) => {
    console.log('Token received:', token);
    setStreamingTokens(prev => {
      const newTokens = [...prev, token];
      if (newTokens.length > 10) {
        return newTokens.slice(newTokens.length - 10);
      }
      return newTokens;
    });
  };

  // Send a message to the model
  const sendMessage = async () => {
    if (!modelState?.instance || !input.trim()) return;
    
    const userMessage: Message = { role: 'user', content: input.trim() };
    setMessages(prev => [...prev, userMessage]);
    setInput('');
    setGenerating(true);
    setError(null);
    setStreamingTokens([]);
    
    try {
      // Get current messages including the new user message
      const currentMessages = [...messages, userMessage];
      
      const completionOptions: any = {
        messages: currentMessages,
        temperature: 0.3,
        top_p: 0.85,
        top_k: 40,
        max_tokens: 400,
        stop: ["</s>", "<|im_end|>", "<|eot_id|>"],
      };
      
      // Add tools and tool configuration only in tools mode
      if (modelState.mode === 'tools') {
        completionOptions.tools = [weatherTool];
        completionOptions.tool_choice = "auto";
      }

      // Get the initial assistant response
      const response = await modelState.instance.completion(
        completionOptions,
        (data: { token: string }) => {
          handleStreamingToken(data.token);
        }
      );

      console.log('Response with tool calls:', response);

      // Add the assistant response to the messages
      const assistantMessage: Message = { 
        role: 'assistant', 
        content: response.text || 'Sorry, I couldn\'t generate a response.' 
      };
      
      // Check if there are tool calls to process
      if (response.tool_calls && response.tool_calls.length > 0) {
        // Add assistant message with the tool call request
        setMessages(prev => [...prev, assistantMessage]);
        
        // Process all tool calls sequentially
        const toolMessages: Message[] = [];
        
        for (const toolCall of response.tool_calls) {
          const toolResponse = await handleToolCall(toolCall);
          toolMessages.push(toolResponse);
        }
        
        // Add all tool responses to messages
        setMessages(prev => [...prev, ...toolMessages]);
        
        // Now get a follow-up response with the tool results included
        const allMessages = [
          ...currentMessages,
          assistantMessage,
          ...toolMessages
        ];
        
        // Make a second completion call with the tool results
        const finalResponse = await modelState.instance.completion({
          ...completionOptions,
          messages: allMessages,
        },
        (data: { token: string }) => {
          handleStreamingToken(data.token);
        });
        // Add the final assistant response
        const finalMessage: Message = { 
          role: 'assistant', 
          content: finalResponse.text || 'I couldn\'t process the tool response.'
        };
        setMessages(prev => [...prev, finalMessage]);
      } else {
        // No tool calls - just add the assistant message
        setMessages(prev => [...prev, assistantMessage]);
      }
    } catch (err) {
      console.error('Completion error:', err);
      setError(`Error generating response: ${err instanceof Error ? err.message : String(err)}`);
    } finally {
      setGenerating(false);
    }
  };

  // Test embedding
  const testEmbedding = async () => {
    if (!modelState?.instance) return;
    
    try {
      const testText = input || 'Hello, world';
      
      console.log(`Testing embedding with text: "${testText}"`);
      
      // First test: Simple embedding array with float format
      console.log('Generating embedding with float format...');
      const embedding = await modelState.instance.embedding(testText);
      
      // Log the embedding details
      console.log(`Generated embedding with ${embedding.length} dimensions`);
      console.log('Full embedding data:', JSON.stringify(embedding));
      
      // Verify each value is a valid number
      const allValid = embedding.every((val: number) => typeof val === 'number' && !isNaN(val));
      console.log('All values are valid numbers:', allValid);
      
      // Calculate some stats
      const min = Math.min(...embedding);
      const max = Math.max(...embedding);
      const avg = embedding.reduce((sum: number, val: number) => sum + val, 0) / embedding.length;
      console.log(`Min: ${min}, Max: ${max}, Avg: ${avg.toFixed(6)}`);
      
      // Second test: OpenAI-compatible format with float encoding
      console.log('Generating OpenAI format embedding with float encoding...');
      const openAIEmbedding = await modelState.instance.embedding(
        { 
          content: testText,
          add_bos_token: true,
          encoding_format: 'float'
        }, 
        true // Request OpenAI format
      );
      
      console.log('OpenAI format embedding result:', JSON.stringify(openAIEmbedding));
      
      // Third test: OpenAI-compatible format with base64 encoding
      console.log('Generating OpenAI format embedding with base64 encoding...');
      const openAIBase64Embedding = await modelState.instance.embedding(
        { 
          content: testText,
          add_bos_token: true,
          encoding_format: 'base64'
        }, 
        true // Request OpenAI format
      );
      
      console.log('OpenAI format with base64 encoding result:', JSON.stringify(openAIBase64Embedding));
      
      if (embedding && embedding.length) {
        const firstFew = embedding.slice(0, 5);
        const lastFew = embedding.slice(-5);
        
        let resultMessage = 
          `Embedding generated successfully!\n\n` +
          `Simple format:\n` +
          `- Vector dimension: ${embedding.length}\n` +
          `- First values: [${firstFew.map((v: number) => v.toFixed(6)).join(', ')}...]\n` +
          `- Last values: [...${lastFew.map((v: number) => v.toFixed(6)).join(', ')}]\n` +
          `- All values are valid numbers: ${allValid}\n` +
          `- Range: Min=${min.toFixed(6)}, Max=${max.toFixed(6)}, Avg=${avg.toFixed(6)}\n\n`;
          
        if (openAIEmbedding && 'data' in openAIEmbedding) {
          const oaiEmbedding = openAIEmbedding.data[0].embedding;
          resultMessage += 
            `OpenAI format (float):\n` +
            `- Token count: ${openAIEmbedding.usage.prompt_tokens}\n` +
            `- Vector dimension: ${Array.isArray(oaiEmbedding) ? oaiEmbedding.length : "N/A (base64)"}\n` +
            `- Model: ${openAIEmbedding.model}\n\n`;
        }
        
        if (openAIBase64Embedding && 'data' in openAIBase64Embedding) {
          const base64Data = openAIBase64Embedding.data[0].embedding;
          resultMessage += 
            `OpenAI format (base64):\n` + 
            `- Token count: ${openAIBase64Embedding.usage.prompt_tokens}\n` +
            `- Encoding: ${openAIBase64Embedding.data[0].encoding_format || 'float'}\n` +
            `- Base64 string length: ${typeof base64Data === 'string' ? base64Data.length : "N/A"}\n`;
        }
        
        setError(resultMessage);
      }
    } catch (err) {
      console.error('Embedding error:', err);
      setError(`Embedding Error: ${err instanceof Error ? err.message : String(err)}`);
    }
  };

  // Test tokenizer
  const testTokenizer = async () => {
    if (!modelState?.instance) return;
    
    try {
      console.log('Testing tokenizer with input:', input || 'Hello, how are you?');
      const testText = input || 'Hello, how are you?';
      
      const result = await modelState.instance.tokenize({
        content: testText,
        add_special: false,
        with_pieces: true
      });
      
      console.log('Tokenization result:', result);
      
      if (result && result.tokens) {
        setError(`Tokenized to ${result.tokens.length} tokens: ${JSON.stringify(result.tokens)}`);
      } else {
        setError('Tokenization returned empty result');
      }
    } catch (err) {
      console.error('Tokenization error:', err);
      setError(`Tokenization Error: ${err instanceof Error ? err.message : String(err)}`);
    }
  };
  
  // Test detokenizer
  const testDetokenize = async () => {
    if (!modelState?.instance) return;
    
    try {
      console.log('Testing detokenize with input:', input || 'Hello, how are you?');
      const testText = input || 'Hello, how are you?';
      
      // First tokenize the text
      const tokenizeResult = await modelState.instance.tokenize({
        content: testText,
        add_special: false
      });
      
      console.log('Tokenization for detokenize test:', tokenizeResult);
      
      if (!tokenizeResult || !tokenizeResult.tokens || !tokenizeResult.tokens.length) {
        setError('Tokenization returned empty result for detokenize test');
        return;
      }
      
      // Extract token IDs and detokenize
      const tokenIds = tokenizeResult.tokens.map((token: any) => 
        typeof token === 'number' ? token : token.id
      );
      
      // Call detokenize with the token IDs
      const detokenizeResult = await modelState.instance.detokenize({
        tokens: tokenIds
      });
      
      console.log('Detokenize result:', detokenizeResult);
      
      if (detokenizeResult && detokenizeResult.content) {
        setError(`Original: "${testText}"\nDetokenized: "${detokenizeResult.content}"`);
      } else {
        setError('Detokenize returned empty result');
      }
    } catch (err) {
      console.error('Detokenize error:', err);
      setError(`Detokenize Error: ${err instanceof Error ? err.message : String(err)}`);
    }
  };

  // Scroll to bottom of messages
  React.useEffect(() => {
    setTimeout(() => {
      scrollViewRef.current?.scrollToEnd({ animated: true });
    }, 100);
  }, [messages]);

  // Render the appropriate interface based on model state and mode
  const renderInterface = () => {
    if (!modelState) {
      return <ModelLoader onModelLoaded={handleModelLoaded} />;
    }

    return (
      <KeyboardAvoidingView
        behavior={Platform.OS === 'ios' ? 'padding' : 'height'}
        style={styles.chatContainer}
        keyboardVerticalOffset={100}
      >
        <ModelHeader 
          modelInfo={modelState.info}
          mode={modelState.mode}
          onUnload={handleUnload}
        />

        {modelState.mode === 'embeddings' ? (
          <View style={styles.testTabContainer}>
            <Text style={styles.testTabTitle}>Embedding Tests</Text>
            
            <TextInput
              style={styles.testInput}
              value={input}
              onChangeText={setInput}
              placeholder="Enter text to embed..."
              multiline
            />
            
            <View style={styles.testButtonContainer}>
              <Button 
                title="Generate Embedding" 
                onPress={testEmbedding} 
                disabled={!input.trim()}
              />
            </View>
          </View>
        ) : (
          <>
            {modelState.mode === 'tools' && (
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

            {modelState.mode === 'conversation' && (
              <View style={styles.tokenToolsContainer}>
                <TextInput
                  style={styles.testInput}
                  value={input}
                  onChangeText={setInput}
                  placeholder="Enter text to tokenize/detokenize..."
                  multiline
                />
                
                <View style={styles.testButtonContainer}>
                  <Button 
                    title="Tokenize" 
                    onPress={testTokenizer} 
                    disabled={!input.trim()}
                  />
                  <View style={styles.buttonSpacer} />
                  <Button 
                    title="Detokenize" 
                    onPress={testDetokenize} 
                    disabled={!input.trim()}
                  />
                </View>
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
          </>
        )}
      </KeyboardAvoidingView>
    );
  };

  return (
    <View style={styles.container}>
      {renderInterface()}
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
  testResultContainer: {
    marginVertical: 10,
    padding: 10,
    backgroundColor: '#e9ecef',
    borderRadius: 8,
    borderWidth: 1,
    borderColor: '#dee2e6',
    maxHeight: 150,
    overflow: 'scroll',
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
  modeButtons: {
    flexDirection: 'row',
    justifyContent: 'space-around',
    width: '100%',
    marginTop: 20,
  },
  modeButton: {
    backgroundColor: '#007bff',
    paddingHorizontal: 20,
    paddingVertical: 10,
    borderRadius: 8,
    minWidth: 120,
    alignItems: 'center',
  },
  modeButtonText: {
    color: 'white',
    fontWeight: 'bold',
    fontSize: 16,
  },
  testTabContainer: {
    flex: 1,
    padding: 16,
  },
  testTabTitle: {
    fontSize: 16,
    fontWeight: 'bold',
    marginBottom: 16,
  },
  testInput: {
    borderWidth: 1,
    borderColor: '#ced4da',
    borderRadius: 8,
    padding: 12,
    marginBottom: 16,
    minHeight: 100,
    backgroundColor: 'white',
  },
  testButtonContainer: {
    marginBottom: 16,
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
  toolName: {
    fontWeight: 'bold',
    fontSize: 14,
  },
  toolMessage: {
    alignSelf: 'flex-start',
    backgroundColor: '#f0f0f0',
  },
  tokenToolsContainer: {
    padding: 16,
    backgroundColor: '#f8f9fa',
    borderBottomWidth: 1,
    borderBottomColor: '#dee2e6',
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
  buttonSpacer: {
    height: 8,
  },
}); 