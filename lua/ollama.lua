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

local ollama_provider = Provider.create({
    model = "llama3.2:1b", -- Default Ollama model
    temperature = 0.7,
    max_tokens = 1024,
    top_p = 1.0,
    stream = false,
    stop = nil
}, function(params)
    -- Construct API request for Ollama
    local messages = map_history(params.history)

    table.insert(messages, {
        role = "user",
        content = params.input
    })

    local request_body = {
        model = params.config.model,
        messages = messages,
        stream = false,
        options = {
            temperature = params.config.temperature,
            top_p = params.config.top_p,
            stop = params.config.stop,
            num_predict = params.config.max_tokens,

        }
    }

    local serialized_body = cjson.encode(request_body)

    -- Make API call to local Ollama instance
    local req = http.new_from_uri("http://ollama:11434/api/chat")
    req.headers:upsert(":method", "POST")
    req.headers:upsert("content-type", "application/json")
    req:set_body(serialized_body)

    local headers, stream = assert(req:go())
    local status = headers:get(":status")
    local status_code = tonumber(status)

    if status_code ~= 200 then
        local body = stream:get_body_as_string() or "No response body"
        local error_message = "unknown error"

        local decoded_ok, decoded_body = pcall(cjson.decode, body)

        if decoded_ok then
            error_message = decoded_body.error or error_message
        else
            error_message = "JSON parse error: " .. decoded_body
        end

        return false, nil, string.format("Ollama API error (status %d): %s", status_code, error_message)
    end

    -- Process successful response
    local body, err = stream:get_body_as_string()
    if err then
        return false, nil, "Error reading response body: " .. err
    end
    print(body)
    local response_data, parse_err = cjson.decode(body)
    if not response_data then
        return false, nil, "JSON parse error: " .. tostring(parse_err)
    end

    if not response_data.message then
        return false, nil, "No message in API response"
    end

    return true, {
        content = response_data.message.content,
        metadata = {
            model = response_data.model,
            done = response_data.done,
            timing = {
                total_duration = response_data.total_duration,
                load_duration = response_data.load_duration,
                prompt_eval_count = response_data.prompt_eval_count,
                eval_count = response_data.eval_count
            }
        }
    }, nil
end
)

function ollama(params)
    return ollama_provider.request(params)
end
