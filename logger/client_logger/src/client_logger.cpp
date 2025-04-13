#include <string>
#include <sstream>
#include <algorithm>
#include <utility>
#include <ctime>
#include <iomanip>
#include "../include/client_logger.h"
#include <not_implemented.h>

std::unordered_map<std::string, std::pair<size_t, std::ofstream>> client_logger::refcounted_stream::_global_streams;


client_logger::refcounted_stream::refcounted_stream(const std::string &path)
{
    _stream.first = path;
    _stream.second = nullptr;
}

client_logger::refcounted_stream::refcounted_stream(const refcounted_stream &oth)
{
    _stream = oth._stream;
    if (!_stream.first.empty() && _stream.second == nullptr)
    {
        open();
    }
}

client_logger::refcounted_stream &client_logger::refcounted_stream::operator=(const refcounted_stream &oth)
{
    if (this != &oth)
    {
        this->~refcounted_stream();
        new (this) refcounted_stream(oth);
    }
    return *this;
}

client_logger::refcounted_stream::refcounted_stream(refcounted_stream &&oth) noexcept
{
    _stream = std::move(oth._stream);
    oth._stream.second = nullptr;
}

client_logger::refcounted_stream &client_logger::refcounted_stream::operator=(refcounted_stream &&oth) noexcept
{
    if (this != &oth)
    {
        this->~refcounted_stream();
        new (this) refcounted_stream(std::move(oth));
    }
    return *this;
}

void client_logger::refcounted_stream::open()
{
    if (_stream.first.empty() || _stream.second != nullptr)
    {
        return;
    }

    auto it = _global_streams.find(_stream.first);
    if (it == _global_streams.end())
    {
        std::ofstream stream(_stream.first, std::ios::app);
        if (!stream.is_open())
        {
            throw std::runtime_error("Cannot open file: " + _stream.first);
        }
        _global_streams.emplace(_stream.first, std::make_pair(1, std::move(stream)));
        _stream.second = &_global_streams.at(_stream.first).second;
    }
    else
    {
        ++it->second.first;
        _stream.second = &it->second.second;
    }
}

client_logger::refcounted_stream::~refcounted_stream()
{
    if (!_stream.first.empty() && _stream.second != nullptr)
    {
        auto it = _global_streams.find(_stream.first);
        if (it != _global_streams.end() && --it->second.first == 0)
        {
            it->second.second.close();
            _global_streams.erase(it);
        }
    }
}

// endregion refcounted_stream implementation

// region client_logger implementation

client_logger::client_logger(
    const std::unordered_map<logger::severity, std::pair<std::forward_list<refcounted_stream>, bool>> &streams,
    std::string format)
    : _output_streams(streams), _format(std::move(format))
{
    for (auto &[severity, streams_pair] : _output_streams)
    {
        for (auto &stream : streams_pair.first)
        {
            stream.open();
        }
    }
}

client_logger::client_logger(const client_logger &other)
    : _output_streams(other._output_streams), _format(other._format)
{
    for (auto &[severity, streams_pair] : _output_streams)
    {
        for (auto &stream : streams_pair.first)
        {
            stream.open();
        }
    }
}

client_logger &client_logger::operator=(const client_logger &other)
{
    if (this != &other)
    {
        this->~client_logger();
        new (this) client_logger(other);
    }
    return *this;
}

client_logger::client_logger(client_logger &&other) noexcept
    : _output_streams(std::move(other._output_streams)), _format(std::move(other._format))
{
}

client_logger &client_logger::operator=(client_logger &&other) noexcept
{
    if (this != &other)
    {
        this->~client_logger();
        new (this) client_logger(std::move(other));
    }
    return *this;
}

client_logger::~client_logger() noexcept
{
    // Деструкторы refcounted_stream сами уменьшат счетчики и закроют файлы при необходимости
}

logger &client_logger::log(const std::string &message, logger::severity severity) &
{
    auto it = _output_streams.find(severity);
    if (it == _output_streams.end())
    {
        return *this;
    }

    const auto &[streams, use_console] = it->second;
    std::string formatted_message = make_format(message, severity);

    if (use_console)
    {
        std::cout << formatted_message << std::endl;
    }

    for (const auto &stream : streams)
    {
        if (stream._stream.second != nullptr && stream._stream.second->is_open())
        {
            *stream._stream.second << formatted_message << std::endl;
        }
    }

    return *this;
}

std::string client_logger::make_format(const std::string &message, severity sev) const
{
    std::string result;
    bool in_format = false;

    for (char c : _format)
    {
        if (in_format)
        {
            switch (char_to_flag(c))
            {
                case flag::DATE:
                    result += logger::current_date_to_string();
                    break;
                case flag::TIME:
                    result += logger::current_time_to_string();
                    break;
                case flag::SEVERITY:
                    result += logger::severity_to_string(sev);
                    break;
                case flag::MESSAGE:
                    result += message;
                    break;
                case flag::NO_FLAG:
                    result += '%';
                    result += c;
                    break;
            }
            in_format = false;
        }
        else if (c == '%')
        {
            in_format = true;
        }
        else
        {
            result += c;
        }
    }

    if (in_format)
    {
        result += '%';
    }

    return result;
}

client_logger::flag client_logger::char_to_flag(char c) noexcept
{
    switch (c)
    {
        case 'd': return flag::DATE;
        case 't': return flag::TIME;
        case 's': return flag::SEVERITY;
        case 'm': return flag::MESSAGE;
        default: return flag::NO_FLAG;
    }
}
