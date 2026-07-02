#pragma once

#include <iostream>
#include <vector>

namespace hwlib {
    enum class ExceptionType {
        NETWORK_EXCEPTION,
        CONFIG_EXCEPTION,
        INVALID_ARGUMENT,
        BUFFER_OVERFLOW,
        UNKNOWN_EXCEPTION,
        CALLBACK_EXCEPTION
    };

    enum class ExceptionProducer {
        ASSEMBLY,
        DEVICE
    };


    class DriverException : public std::exception {
    public:

        DriverException() = default;

        explicit DriverException(ExceptionProducer producer, ExceptionType type, std::string message)
                : exceptionProducer(producer), exceptionType(type) {
            addMessage(message);
        }

        void addMessage(std::string message) {
            messages.push_back(message);
        }

        [[nodiscard]] ExceptionType getExceptionType() const {
            return exceptionType;
        }

        [[nodiscard]] std::vector<std::string> getMessages() const {
            return messages;
        }

        [[nodiscard]] ExceptionProducer getExceptionProducer() const {
            return exceptionProducer;
        }

        [[nodiscard]] const char *what() const noexcept override {
            return messages[0].c_str();
        }

    private:
        ExceptionProducer exceptionProducer;
        ExceptionType exceptionType;
        std::vector<std::string> messages;
    };
}