-- ollama.lua
local ollama = {}
local json = require("dkjson")  -- Ensure dkjson.lua is in the Lua path

ollama.config = {
    api_url = "http://localhost:11434",  -- Default Ollama API endpoint
    api_path = "/api/generate",          -- Ollama API path for text generation
    model = "llama3.2:1b"                     -- Default model (can be overridden in config.lua)
}

function ollama.request_handler(config, input, options)
    local headers = {
        ["Content-Type"] = "application/json"
    }

    -- Build the request body
    local request_body = {
        model = options.model or config.model,
        prompt = input,
        stream = false  -- Set to false for a single response
    }

    -- Encode the request body as JSON
    local body_str = json.encode(request_body)

    -- Send the HTTP POST request
    local response = http_post(config.api_url, config.api_path, headers, body_str)

    -- Check if the HTTP request failed
    if response == "HTTP request failed" then
        return {content = "HTTP request failed", metadata = {}}
    end

    -- Parse the JSON response
    local success, res = pcall(json.decode, response)
    if not success then
        return {content = "Error parsing response", metadata = {}}
    end

    -- Validate the response structure
    if not res or not res.response then
        return {content = "Invalid response format", metadata = {}}
    end

    -- Extract the content and metadata
    return {
        content = res.response or "No content",
        metadata = {
            model = request_body.model,
            done = res.done or false
        }
    }
end

-- Register the provider with the manager
manager:register_provider("ollama", ollama.config, ollama.request_handler)