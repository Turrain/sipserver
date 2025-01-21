-- groq.lua
local groq = {}
local json = require("dkjson")  -- Ensure dkjson.lua is in the Lua path

groq.config = {
    api_key = "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",  -- Replace with your Groq API key
    api_url = "api.groq.com",  -- Groq API endpoint
    api_path = "/openai/v1/chat/completions",
    model = "mixtral-8x7b-32768"  -- Default model (can be overridden in config.lua)
}

function groq.request_handler(config, input, options)
    local headers = {
        ["Authorization"] = "Bearer " .. config.api_key,
        ["Content-Type"] = "application/json"
    }

    -- Build the request body
    local request_body = {
        model = options.model or config.model,
        messages = {{role = "user", content = input}},
        temperature = options.temperature or 0.7
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
    if not res or not res.choices or not res.choices[1] or not res.choices[1].message then
        return {content = "Invalid response format", metadata = {}}
    end

    -- Extract the content and metadata
    return {
        content = res.choices[1].message.content or "No content",
        metadata = {
            model = request_body.model,
            tokens_used = res.usage and res.usage.total_tokens or 0
        }
    }
end

-- Register the provider with the manager
manager:register_provider("groq", groq.config, groq.request_handler)