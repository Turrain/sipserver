local M = {}

--[[
    Recursively prints a table structure for debugging purposes.
    @param tbl {table} - The table to print
    @param indent {string} - (Optional) Current indentation level
]]
function print_table(tbl, indent)
    indent = indent or ""
    for key, value in pairs(tbl) do
        local formatted_key = tostring(key)
        if type(value) == "table" then
            print(indent .. formatted_key .. ":")
            print_table(value, indent .. "  ")
        else
            print(indent .. formatted_key .. " = " .. tostring(value))
        end
    end
end

--[[
    Deep merges multiple tables with specified collision strategy
    @param mode {'force'|'keep'|'error'} - Merge strategy
    @param target {table} - Target table to merge into
    @param ... {table} - Source tables
    @returns {table} - Merged table
]]
local function deep_merge(mode, target, ...)
    local sources = { ... }
    
    for _, source in ipairs(sources) do
        if type(source) ~= "table" then
            error(string.format("Invalid source type: expected table, got %s", type(source)))
        end

        for key, value in pairs(source) do
            if mode == 'force' then
                if type(value) == "table" and type(target[key]) == "table" then
                    deep_merge(mode, target[key], value)
                else
                    target[key] = value
                end
            elseif mode == 'keep' then
                if target[key] == nil then
                    target[key] = value
                end
            elseif mode == 'error' then
                if target[key] ~= nil then
                    error(string.format("Key collision at '%s'", key))
                end
                target[key] = value
            end
        end
    end

    return target
end

--[[
    Creates a new provider instance
    @param defaults {table} - Default configuration values
    @param request_handler {function} - Request processing function
    @returns {table} - Provider instance
]]
function M.create(defaults, request_handler)
    -- Argument validation
    if type(request_handler) ~= "function" then
        error("Request handler must be a function")
    end

    local provider = {
        config = {
            parameters = {},
            defaults = defaults or {}
        },
        metadata = {}
    }

    --[[
        Executes a provider request with validation and error handling
        @param params {table} - Request parameters
        @returns {bool, any, string} - Success status, result, error message
    ]]
    function provider.request(params)
        -- Parameter validation
        if type(params) ~= "table" then
            return false, nil, "Invalid parameters: expected table"
        end

        -- Merge configurations with priority: params.options > params.config > parameters > defaults
        local config = deep_merge('force', 
            {},
            provider.config.defaults,
            provider.config.parameters,
            params.config or {},
            params.options or {}
        )

        -- Validate API key presence and format
        if not config.api_key or type(config.api_key) ~= "string" or config.api_key == "" then
            return false, nil, "Invalid or missing API key"
        end

        -- Prepare handler arguments
        local handler_args = {
            input = tostring(params.input or ""),
            history = type(params.history) == "table" and params.history or {},
            config = config,
            metadata = provider.metadata
        }

        -- Execute handler with protected call
        local ok, success, result, err = xpcall(
            function()
                return request_handler(handler_args)
            end,
            function(error_message)
                return debug.traceback(string.format("Handler error: %s", error_message), 2)
            end
        )

        -- Handle execution errors
        if not ok then
            return false, nil, success -- 'success' contains error message when 'ok' is false
        end

        -- Update metadata if provided
        if result and type(result.metadata) == "table" then
            provider.metadata = deep_merge('keep', provider.metadata, result.metadata)
        end

        return success, result, err
    end

    return provider
end

return M