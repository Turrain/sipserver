local Provider = require("provider")
local http = require "http.request"
local cjson = require "cjson"

local function map_history(history)
    local messages = {}
    for _, msg in ipairs(history or {}) do
        table.insert(messages, {
            role = msg.role,
            content = msg.content,
        })
    end
    return messages
end

local groq_provider = Provider.create({
    model = "mixtral-8x7b-32768",
    temperature = 0.7,
    max_tokens = 1024,
    top_p = 1.0,
    stream = false,
    stop = nil
}, function(params)
        -- Construct API request
        local messages = map_history(params.history)

        table.insert(messages, {
            role = "user",
            content = params.input
        })
        local request_body = {
            model = params.config.model,
            messages = messages,
            temperature = params.config.temperature,
            max_tokens = params.config.max_tokens,
            top_p = params.config.top_p,
            stream = params.config.stream,
        }

        if params.config.stop then
            request_body.stop = params.config.stop
        end
  
        local serialized_body = cjson.encode(request_body)
        print(serialized_body)
        -- Make API call (implementation would use C++ HTTP client)
        local req = http.new_from_uri("https://api.groq.com/openai/v1/chat/completions")
        local headers = {
            ["Content-Type"] = "application/json",
            ["Authorization"] = "Bearer " .. params.config.api_key,
            ["Content-Length"] = #serialized_body, -- Important for POST requests
        }

        req.headers:upsert(":method", "POST")
        req.headers:upsert("content-type", "application/json")
        req.headers:upsert("authorization", "Bearer " .. params.config.api_key)
        req:set_body(serialized_body)

        local headers, stream = assert(req:go())
        local status = headers:get(":status")
        if not status then
            return false, nil, "Missing HTTP status in response"
        end
        local status_code = tonumber(status)
        if not status_code then
            return false, nil, "Invalid HTTP status code: " .. tostring(status)
        end

        -- Check HTTP status code
        if status_code ~= 200 then
            local body = stream:get_body_as_string() or "No response body"
            local error_message = "unknown error"
            
            -- Proper pcall usage with two return values
            local decoded_ok, decoded_body = pcall(cjson.decode, body)
            
            if decoded_ok then  -- Only if decoding succeeded
                if decoded_body and decoded_body.error and decoded_body.error.message then
                    error_message = decoded_body.error.message
                end
            else  -- Handle JSON parse errors
                error_message = "Failed to parse error response: " .. decoded_body
            end
            
            return false, nil, string.format("Groq API error (status %d): %s", status_code, error_message)
        end

        -- Process successful response
        local body, err = stream:get_body_as_string()
        if err then
            return false, nil, "Error reading response body: " .. err
        end

        local response_data, parse_err = cjson.decode(body)
        if not response_data then
            return false, nil, "JSON parse error: " .. tostring(parse_err)
        end

        if not response_data.choices or #response_data.choices == 0 then
            return false, nil, "No choices in API response"
        end
       
        return true, {
            content = response_data.choices[1].message.content,
            metadata = {
                usage = response_data.usage,
                model = response_data.model,
            }
        }, nil
    end
)
function groq(params)
    return groq_provider.request(params)
end