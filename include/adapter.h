#pragma once

#include <memory>
#include <functional>
#include <type_traits>
#include <system_error>
#include <unordered_map>
#include <string>
#include <utility>
#include <exception>
#include <any>

// Helper for optional-like behavior in C++14
template <typename T>
class Optional {
public:
    Optional() : m_hasValue(false) {}
    Optional(const T& value) : m_value(value), m_hasValue(true) {}
    Optional(T&& value) : m_value(std::move(value)), m_hasValue(true) {}

    bool has_value() const { return m_hasValue; }
    T& value() { if (!m_hasValue) throw std::runtime_error("No value"); return m_value; }
    const T& value() const { if (!m_hasValue) throw std::runtime_error("No value"); return m_value; }

private:
    T m_value;
    bool m_hasValue;
};

// Base Adapter Interface with Strong Typing and Error Handling
template <typename InputType, typename OutputType>
class AbstractAdapter {
public:
    using Input = InputType;
    using Output = OutputType;
    using ErrorType = std::error_code;

    virtual std::pair<Output, ErrorType> adapt(const Input& input) = 0;
    virtual ~AbstractAdapter() = default;
};

// Adapter Composition and Transformation Utilities
template <typename AdapterType>
class AdapterComposer {
public:
    using Input = typename AdapterType::Input;
    using Output = typename AdapterType::Output;
    using ErrorType = typename AdapterType::ErrorType;

    explicit AdapterComposer(std::unique_ptr<AdapterType> adapter)
        : m_adapter(std::move(adapter)) {}

    template <typename TransformFunc>
    auto then(TransformFunc transformer) {
        return AdapterComposer([adapter = std::move(m_adapter), transformer](const Input& input) {
            auto result = adapter->adapt(input);
            if (!result.second) {
                return std::make_pair(transformer(result.first), ErrorType());
            } else {
                return std::make_pair(Output(), result.second);
            }
        });
    }

private:
    std::unique_ptr<AdapterType> m_adapter;
};

// Strategy-Based Adapter with Dynamic Behavior
template <typename InputType, typename OutputType>
class StrategyAdapter : public AbstractAdapter<InputType, OutputType> {
public:
    using TransformStrategy = std::function<std::pair<OutputType, std::error_code>(const InputType&)>;

    explicit StrategyAdapter(TransformStrategy strategy)
        : m_strategy(std::move(strategy)) {}

    std::pair<OutputType, std::error_code> adapt(const InputType& input) override {
        return m_strategy(input);
    }

private:
    TransformStrategy m_strategy;
};

// Type-Erased Adapter for Runtime Polymorphism
class DynamicAdapter {
public:
    template <typename AdapterType>
    DynamicAdapter(AdapterType adapter)
        : m_adapter(std::make_unique<AdapterModel<AdapterType>>(std::move(adapter))) {}

    template <typename InputType>
    auto adapt(const InputType& input) {
        return m_adapter->adapt(input);
    }

private:
    struct AdapterConcept {
        virtual ~AdapterConcept() = default;
        virtual std::pair<std::any, std::error_code> adapt(const std::any& input) = 0;
    };

    template <typename AdapterType>
    struct AdapterModel : AdapterConcept {
        explicit AdapterModel(AdapterType adapter)
            : m_adapter(std::move(adapter)) {}

        std::pair<std::any, std::error_code> adapt(const std::any& input) override {
            auto typedInput = std::any_cast<typename AdapterType::Input>(&input);
            if (typedInput) {
                auto result = m_adapter.adapt(*typedInput);
                return {std::any(result.first), result.second};
            }
            return {std::any(), std::make_error_code(std::errc::invalid_argument)};
        }

        AdapterType m_adapter;
    };

    std::unique_ptr<AdapterConcept> m_adapter;
};

// Contextual Adapter with Configuration and State
template <typename InputType, typename OutputType, typename ContextType>
class ContextualAdapter : public AbstractAdapter<InputType, OutputType> {
public:
    explicit ContextualAdapter(ContextType context)
        : m_context(std::move(context)) {}

    void updateContext(const ContextType& newContext) {
        m_context = newContext;
    }

protected:
    ContextType m_context;
};



// Adapter Registration and Factory
class AdapterRegistry {
public:
    template <typename AdapterType>
    void registerAdapter(const std::string& key,
                         std::function<std::unique_ptr<AdapterType>()> creator) {
        m_adapters[key] = [creator]() -> std::unique_ptr<AbstractAdapter<std::string, std::string>> {
            return creator();
        };
    }

    std::unique_ptr<AbstractAdapter<std::string, std::string>> createAdapter(const std::string& type) {
        auto it = m_adapters.find(type);
        if (it != m_adapters.end()) {
            return it->second();
        }
        return nullptr;
    }

private:
    std::unordered_map<std::string, std::function<std::unique_ptr<AbstractAdapter<std::string, std::string>>()> > m_adapters;
};
