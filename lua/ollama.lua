-- ollama.lua
local ollama = {}
local json = require("dkjson")

-- Get config from C++
ollama.config = get_provider_config()

function ollama.request_handler(config, input, options)
    local headers = {
        ["Content-Type"] = "application/json"
    }

    -- Build the request body
    local request_body = {
        model = options.model or config.model,
        messages = {{role = "user", content = input}},
        stream = options.stream or false
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

    -- Extract the content and metadata
    return {
        content = res.message and res.message.content or "No content",
        metadata = {
            model = request_body.model,
            total_duration = res.total_duration
        }
    }
end

-- Register the provider with the manager
manager:register_provider("ollama", ollama.config, ollama.request_handler)
