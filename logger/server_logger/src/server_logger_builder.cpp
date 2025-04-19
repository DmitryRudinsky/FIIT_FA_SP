#include <not_implemented.h>
#include "../include/server_logger_builder.h"
#include <fstream>

using namespace nlohmann;

logger_builder& server_logger_builder::add_file_stream(
    std::string const &stream_file_path,
    logger::severity severity) &
{
    if (_output_streams.contains(severity))
    {
        _output_streams[severity].first = stream_file_path;
    }
    else
    {
        _output_streams[severity] = std::make_pair(stream_file_path, false);
    }
    return *this;
}

logger_builder& server_logger_builder::add_console_stream(
    logger::severity severity) &
{
    if (_output_streams.contains(severity))
    {
        _output_streams[severity].second = true;
    }
    else
    {
        _output_streams[severity] = std::make_pair("", true);
    }
    return *this;
}

logger_builder& server_logger_builder::transform_with_configuration(
    std::string const &configuration_file_path,
    std::string const &configuration_path) &
{
    json js;

    std::ifstream stream(configuration_file_path);
    if (stream.is_open())
    {
        json::parser_callback_t callback = [&configuration_path](int depth,
                                                                 json::parse_event_t event, json &parsed) {
            if (event == json::parse_event_t::key && depth == 1 && parsed != json(configuration_path))
            {
                return false;
            }
            return true;
        };

        js = json::parse(stream, callback);
        stream.close();
        if (js.contains(configuration_path) == false)
        {
            return *this;
        }

        js = js[configuration_path];
        if (js.contains("format"))
        {
            set_format(js["format"]);
        }
        if (js.contains("console_stream"))
        {
            for (auto &[key, value]: js["console_stream"].items())
            {
                try
                {
                    add_console_stream(string_to_severity(value.get<std::string>()));
                }
                catch (std::out_of_range const &e) {}
            }
        }
        if (js.find("file_streams") != js.end())
        {
            for (auto &[path, sev]: js["file_streams"].items())
            {
                try
                {
                    add_file_stream(path, string_to_severity(sev.get<std::string>()));
                }
                catch (std::out_of_range const &e) {}
            }
        }
    }
    return *this;
}

logger_builder& server_logger_builder::clear() &
{
    _destination = "http://127.0.0.1:9200";
    _output_streams.clear();
    return *this;
}

logger *server_logger_builder::build() const
{
    return new server_logger(_destination, _output_streams, _format);
}

logger_builder& server_logger_builder::set_destination(const std::string& dest) &
{
    _destination = dest;
    return *this;
}

logger_builder& server_logger_builder::set_format(const std::string &format) &
{
    _format = format;
    return *this;
}