-- openai.lua
local openai = {}
local json = require("dkjson")

-- Get config from C++
openai.config = get_provider_config()

function openai.request_handler(config, input, options)
    print("Config: " .. json.encode(config))
    local headers = {
        ["Authorization"] = "Bearer " .. config.api_key,
        ["Content-Type"] = "application/json"
    }

    local request_body = {
        model = options.model or config.model,
        messages = {{role = "user", content = input}},
        temperature = options.temperature or 0.7
    }

    local body_str = json.encode(request_body)
    print("Sending request to OpenAI API...")
    print("URL: " .. config.api_url .. "/chat/completions")
    print("Headers: " .. json.encode(headers))
    print("Body: " .. body_str)

    local response = http_post(config.api_url, "/v1/chat/completions", headers, body_str)

    print("Raw response: " .. response)

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
manager:register_provider("openai", openai.config, openai.request_handler)
