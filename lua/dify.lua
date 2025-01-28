-- dify.lua
local dify = {}
local json = require("dkjson")

-- Get config from C++
dify.config = get_provider_config()

function dify.request_handler(config, input, options)
    print("Config: " .. (config.model))
    local headers = {
        ["Authorization"] = "Bearer " .. config.api_key,
        ["Content-Type"] = "application/json"
    }


    -- Build the request body for Dify
    local request_body = {
        query = input,
        response_mode = "blocking",
        conversation_id = options.conversation_id,
        user = options.user_id or "default_user"
    }

    -- Encode the request body as JSON
    local body_str = json.encode(request_body)

    -- Send the HTTP POST request
    local response = http_post(config.api_url, config.api_path, headers, body_str)
    print("Raw response from Dify API:", response)

    -- Check if the HTTP request failed
    if response == "HTTP request failed" then
        return {content = "HTTP request failed", metadata = {}}
    end

    -- Parse the JSON response
    local success, res = pcall(json.decode, response)
    if not success then
        print("JSON parse error:", res)
        return {content = "Error parsing response", metadata = {}}
    end
    print("Parsed JSON response:", json.encode(res))

    -- Validate the response structure for Dify
    if not res or not res.answer then
        print("Invalid response structure:", json.encode(res))
        return {content = "Invalid response format", metadata = {}}
    end

    -- Extract the content and metadata
    return {
        content = res.answer or "No content",
        metadata = {
            conversation_id = res.conversation_id,
            message_id = res.message_id
        }
    }
end

-- Register the provider with the manager
manager:register_provider("dify", dify.config, dify.request_handler)
