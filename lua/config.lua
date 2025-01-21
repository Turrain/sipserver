-- Central configuration file
cfg = {
    -- Default provider to use if none is specified
    default_provider = "openai",

    -- List of providers and their configurations
    providers = {
        openai = {
            enabled = true,  -- Enable or disable this provider
            script_path = "openai.lua",  -- Path to the provider's Lua script
            config_overrides = {  -- Override default configuration values
                api_key = "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",
                api_url = "api.openai.com",
                model = "gpt-4o"
            }
        },
        --dify = {
        --    enabled = false,
        --    script_path = "dify.lua",
        --    config_overrides = {
        --        api_key = "your-dify-api-key",
        --        api_url = "https://api.dify.ai/v1"
        --    }
        --},
        groq = {
            enabled = true,
            script_path = "groq.lua",
            config_overrides = {}
        },
        ollama = {
            enabled = true,
            script_path = "ollama.lua",
            config_overrides = {
                model = "llama2:13b"
            }
        }
    }
}