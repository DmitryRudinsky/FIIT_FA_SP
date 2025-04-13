#include <filesystem>
#include <fstream>
#include <sstream>
#include "../include/client_logger_builder.h"
#include <not_implemented.h>

using namespace nlohmann;

logger_builder& client_logger_builder::add_file_stream(
    const std::string& stream_file_path,
    logger::severity severity) &
{
    auto& [streams, use_console] = _output_streams[severity];
    streams.emplace_front(stream_file_path);
    return *this;
}

logger_builder& client_logger_builder::add_console_stream(
    logger::severity severity) &
{
    _output_streams[severity].second = true;
    return *this;
}

logger_builder& client_logger_builder::transform_with_configuration(
    const std::string& configuration_file_path,
    const std::string& configuration_path) &
{
    if (!std::filesystem::exists(configuration_file_path)) {
        throw std::runtime_error("Configuration file not found: " + configuration_file_path);
    }

    std::ifstream config_file(configuration_file_path);
    if (!config_file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + configuration_file_path);
    }

    try {
        json config = json::parse(config_file);
        json* current = &config;

        // Находим нужный раздел конфигурации
        if (!configuration_path.empty()) {
            std::istringstream path_stream(configuration_path);
            std::string path_part;

            while (std::getline(path_stream, path_part, '.')) {
                if (current->contains(path_part)) {
                    current = &(*current)[path_part];
                } else {
                    throw std::runtime_error("Config path not found: " + configuration_path);
                }
            }
        }

        // Устанавливаем формат
        if (current->contains("format")) {
            _format = current->at("format").get<std::string>();
        }

        // Очищаем текущие настройки
        _output_streams.clear();

        // Настраиваем severity из конфига
        if (current->contains("severities")) {
            for (auto& [key, value] : current->at("severities").items()) {
                try {
                    logger::severity sev = string_to_severity(key);
                    parse_severity(sev, value);
                } catch (const std::out_of_range& e) {
                    // Пропускаем неизвестные severity
                    continue;
                }
            }
        }

    } catch (const json::exception& e) {
        throw std::runtime_error("JSON error: " + std::string(e.what()));
    }

    return *this;
}

void client_logger_builder::parse_severity(logger::severity sev, nlohmann::json& j)
{
    if (j.is_object()) {
        // Обработка файлов
        if (j.contains("files") && j["files"].is_array()) {
            for (const auto& file : j["files"]) {
                if (file.is_string()) {
                    add_file_stream(file.get<std::string>(), sev);
                }
            }
        } else if (j.contains("file") && j["file"].is_string()) {
            add_file_stream(j["file"].get<std::string>(), sev);
        }

        // Обработка консоли
        if (j.contains("console") && j["console"].is_boolean() && j["console"].get<bool>()) {
            add_console_stream(sev);
        }
    }
    else if (j.is_array()) {
        for (const auto& item : j) {
            if (item.is_string()) {
                add_file_stream(item.get<std::string>(), sev);
            }
        }
    }
    else if (j.is_string()) {
        add_file_stream(j.get<std::string>(), sev);
    }
}

logger_builder& client_logger_builder::clear() &
{
    _output_streams.clear();
    _format = "%m";
    return *this;
}

logger* client_logger_builder::build() const
{
    return new client_logger(_output_streams, _format);
}

logger_builder& client_logger_builder::set_format(const std::string& format) &
{
    _format = format;
    return *this;
}

logger_builder& client_logger_builder::set_destination(const std::string&) &
{
    throw std::runtime_error("set_destination not implemented");
}