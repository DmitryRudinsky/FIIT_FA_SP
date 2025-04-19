
#include <not_implemented.h>
#include "../include/allocator_global_heap.h"

allocator_global_heap::allocator_global_heap(
    logger *logger) : _logger(logger)
{
    if (_logger != nullptr)
    {
        _logger->log("Создан allocator_global_heap", logger::severity::debug);
    }
}

[[nodiscard]] void *allocator_global_heap::do_allocate_sm(
    size_t size)
{
    if (_logger)
        _logger->log("Начало do_allocate_sm", logger::severity::trace);

    if (_logger)
        _logger->log("Вызов do_allocate_sm(size_t)", logger::severity::debug);

    void *ptr = nullptr;
    try
    {
        ptr = ::operator new(size);
    } catch (const std::bad_alloc &) {
        if (_logger)
            _logger->log("Ошибка выделения памяти: std::bad_alloc", logger::severity::error);
        throw;
    }

    if (_logger)
        _logger->log("Завершение do_allocate_sm", logger::severity::trace);

    return ptr;
}

void allocator_global_heap::do_deallocate_sm(
    void *at)
{
    if (_logger)
        _logger->log("Начало do_deallocate_sm", logger::severity::trace);

    if (_logger)
        _logger->log("Вызов do_deallocate_sm(void *)", logger::severity::debug);

    ::operator delete(at);

    if (_logger)
        _logger->log("Завершение do_deallocate_sm", logger::severity::trace);
}

inline logger *allocator_global_heap::get_logger() const
{
    return _logger;
}

inline std::string allocator_global_heap::get_typename() const
{
    return "allocator_global_heap";
}

allocator_global_heap::~allocator_global_heap()
{
    if (_logger)
        _logger->log("Уничтожен allocator_global_heap", logger::severity::debug);
}

allocator_global_heap::allocator_global_heap(const allocator_global_heap &other) : _logger(other._logger)
{
    if (_logger)
        _logger->log("Копирующий конструктор allocator_global_heap", logger::severity::debug);
}

allocator_global_heap &allocator_global_heap::operator=(const allocator_global_heap &other)
{
    if (this != &other) {
        _logger = other._logger;
        if (_logger)
            _logger->log("Оператор копирующего присваивания allocator_global_heap", logger::severity::debug);
    }
    return *this;
}

bool allocator_global_heap::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    return this == &other;
}

allocator_global_heap::allocator_global_heap(allocator_global_heap &&other) noexcept : _logger(other._logger)
{
    other._logger = nullptr;
    if (_logger)
        _logger->log("Конструктор перемещения allocator_global_heap", logger::severity::debug);
}

allocator_global_heap &allocator_global_heap::operator=(allocator_global_heap &&other) noexcept
{
    if (this != &other) {
        _logger = other._logger;
        other._logger = nullptr;
        if (_logger)
            _logger->log("Оператор перемещающего присваивания allocator_global_heap", logger::severity::debug);
    }
    return *this;
}
