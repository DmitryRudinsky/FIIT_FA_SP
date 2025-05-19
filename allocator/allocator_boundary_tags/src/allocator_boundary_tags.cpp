#include "../include/allocator_boundary_tags.h"


allocator_boundary_tags::~allocator_boundary_tags() {
    logger *logger_instance = get_logger();
    logger_instance->debug("Deleting of allocator started.");
    if (!_trusted_memory) {
        logger_instance->error("Nothing to deallocate.");
        return;
    }

    get_mutex().~mutex();

    try {
        auto *parent_allocator = get_parent_resource();
        size_t total_size = allocator_metadata_size + reinterpret_cast<size_t>(static_cast<char *>(_trusted_memory) +
                                                                               sizeof(logger *) +
                                                                               sizeof(memory_resource *) +
                                                                               sizeof(allocator_with_fit_mode::fit_mode));

        if (parent_allocator) {
            parent_allocator->deallocate(_trusted_memory, total_size);
            logger_instance->trace("Memory deallocated via parent allocator.");
        } else {
            ::operator delete(_trusted_memory);
            logger_instance->trace("Memory deallocated via global operator delete.");
        }

        _trusted_memory = nullptr;
    } catch (const std::exception &ex) {
        logger_instance->error("Error while deleting allocator: " + std::string(ex.what()));
        _trusted_memory = nullptr;
    }

    logger_instance->trace("Deleting of allocator finished.");
}

allocator_boundary_tags::allocator_boundary_tags(allocator_boundary_tags &&other) noexcept : _trusted_memory(other._trusted_memory) {
    trace_with_guard("Constructor started.");

    std::lock_guard<std::mutex> lock(other.get_mutex());
    other._trusted_memory = nullptr;

    get_logger()->debug("Resources moved");
    trace_with_guard("Constructor finished");
}

allocator_boundary_tags &allocator_boundary_tags::operator=(allocator_boundary_tags &&other) noexcept {
    trace_with_guard("Constructor with operator= started.");
    logger *log = get_logger();
    if (this != &other) {
        std::unique_lock<std::mutex> lock_this(get_mutex(), std::defer_lock);
        std::unique_lock<std::mutex> lock_other(other.get_mutex(), std::defer_lock);
        std::cout << "Lock mutex in operator=" << std::endl;
        std::lock(lock_this, lock_other);
        if (_trusted_memory) {
            try {
                size_t total_size = *reinterpret_cast<size_t *>(reinterpret_cast<unsigned char *>(_trusted_memory) +
                                                                sizeof(logger *) +
                                                                sizeof(memory_resource *) +
                                                                sizeof(allocator_with_fit_mode::fit_mode));
                if (auto *parent_alloc = get_parent_resource()) {
                    parent_alloc->deallocate(_trusted_memory, total_size);
                } else {
                    ::operator delete(_trusted_memory);
                }
            } catch (const std::exception &e) {
                log->error("Error in operator= :" + std::string(e.what()));
            }
        }

        _trusted_memory = other._trusted_memory;
        other._trusted_memory = nullptr;
        log->debug("Resources moved");
    }

    trace_with_guard("Constructor with operator= finished.");
    return *this;
}

allocator_boundary_tags::allocator_boundary_tags(size_t space_size, std::pmr::memory_resource *parent_allocator, logger *logger, allocator_with_fit_mode::fit_mode allocate_fit_mode) {
    logger->debug("Constructor of allocator started.");
    if (space_size == 0) {
        logger->error("Size must be more than zero.");
        throw std::invalid_argument("Size must be more than zero.");
    }

    try {
        parent_allocator = parent_allocator ? parent_allocator : std::pmr::get_default_resource();
        size_t total_size = allocator_metadata_size + space_size;
        logger->debug("Allocator requires " + std::to_string(total_size) + " bytes.");
        logger->information("Available " + std::to_string(space_size) + " bytes.");

        _trusted_memory = parent_allocator->allocate(total_size);
        auto *memory = reinterpret_cast<unsigned char *>(_trusted_memory);
        *reinterpret_cast<class logger **>(memory) = logger;
        memory += sizeof(class logger *);

        *reinterpret_cast<std::pmr::memory_resource **>(memory) = parent_allocator;
        memory += sizeof(memory_resource *);

        *reinterpret_cast<allocator_with_fit_mode::fit_mode *>(memory) = allocate_fit_mode;
        memory += sizeof(allocator_with_fit_mode::fit_mode);

        *reinterpret_cast<size_t *>(memory) = space_size;
        memory += sizeof(size_t);

        new (reinterpret_cast<std::mutex *>(memory)) std::mutex();
        memory += sizeof(std::mutex);

        *reinterpret_cast<void **>(memory) = nullptr;
    } catch (const std::exception &e) {
        logger->error("Initiation of allocator failed.");
        throw std::iostream ::failure("Initiation of allocator failed.");
    }

    logger->debug("Initiation of allocator finished");
}

[[nodiscard]] void *allocator_boundary_tags::do_allocate_sm(size_t size) {
    logger *logger = get_logger();
    logger->debug("Allocation started.");
    const size_t total_size = size + occupied_block_metadata_size;
    size_t allocator_size = *reinterpret_cast<size_t *>(reinterpret_cast<char *>(_trusted_memory) +
                                                        sizeof(class logger *) +
                                                        sizeof(memory_resource *) +
                                                        sizeof(allocator_with_fit_mode::fit_mode));
    if (total_size > allocator_size) {
        logger->error("Too much size for allocation.");
        throw std::bad_alloc();
    }

    void *allocated_memory = nullptr;
    switch (get_fit_mode()) {
        case fit_mode::first_fit:
            allocated_memory = allocate_first_fit(size);
            break;
        case fit_mode::the_best_fit:
            allocated_memory = allocate_best_fit(size);
            break;
        case fit_mode::the_worst_fit:
            allocated_memory = allocate_worst_fit(size);
            break;
        default:
            logger->error("Unknown fit mode.");
            throw std::invalid_argument("Unknown fit mode");
    }

    if (!allocated_memory) {
        logger->error("Allocation failed for size " + std::to_string(size));
        throw std::bad_alloc();
    }

    logger->debug("Allocation finished");
    return allocated_memory;
}

void allocator_boundary_tags::do_deallocate_sm(void *at) {
    logger* logger = get_logger();
    logger->debug("Deallocation started.");
    std::lock_guard<std::mutex> guard(get_mutex());

    if (!at) return;

    char* heap_start = reinterpret_cast<char*>(_trusted_memory) + allocator_metadata_size;
    void* next_block = *reinterpret_cast<void**>(reinterpret_cast<char*>(at) + sizeof(size_t));
    void* prev_block = *reinterpret_cast<void**>(reinterpret_cast<char*>(at) + sizeof(size_t) + sizeof(void*));

    if (prev_block) {
        *reinterpret_cast<void**>(reinterpret_cast<char*>(prev_block) + sizeof(size_t)) = next_block;
    } else {
        void** first_block_ptr = reinterpret_cast<void**>(heap_start - sizeof(void*));
        *first_block_ptr = next_block;
    }

    if (next_block) {
        *reinterpret_cast<void**>(reinterpret_cast<char*>(next_block) + sizeof(size_t) + sizeof(void*)) = prev_block;
    }

    merge_adjacent_blocks(at);

    logger->debug("Deallocation finished.");
}

allocator_with_fit_mode::fit_mode allocator_boundary_tags::get_fit_mode() const {
    auto *memory = reinterpret_cast<unsigned char *>(_trusted_memory);
    memory += sizeof(logger *) + sizeof(memory_resource *);
    return *reinterpret_cast<allocator_with_fit_mode::fit_mode *>(memory);
}

inline void allocator_boundary_tags::set_fit_mode(allocator_with_fit_mode::fit_mode mode) {
    auto *memory = reinterpret_cast<unsigned char *>(_trusted_memory);
    auto *mode_ptr = reinterpret_cast<allocator_with_fit_mode::fit_mode *>(memory + sizeof(logger *) + sizeof(memory_resource *));
    *mode_ptr = mode;
}

std::vector<allocator_test_utils::block_info> allocator_boundary_tags::get_blocks_info_inner() const {
    logger *logger = get_logger();
    logger->trace("Get info started.");
    std::vector<allocator_test_utils::block_info> blocks_info;

    if (!_trusted_memory) {
        logger->error("No trusted memory.");
        return blocks_info;
    }

    try {
        char *heap_start = reinterpret_cast<char *>(_trusted_memory) + allocator_metadata_size;
        size_t heap_size = *reinterpret_cast<size_t *>(
                reinterpret_cast<char *>(_trusted_memory) +
                sizeof(class logger *) + sizeof(memory_resource *) +
                sizeof(allocator_with_fit_mode::fit_mode));
        char *heap_end = heap_start + heap_size;

        void **first_block_ptr = reinterpret_cast<void **>(heap_start - sizeof(void *));
        char *current_block = reinterpret_cast<char *>(*first_block_ptr);
        while (current_block && current_block < heap_end) {
            size_t block_size = *reinterpret_cast<size_t *>(current_block);
            void *next_block = *reinterpret_cast<void **>(current_block + sizeof(size_t));
            bool is_occupied = true;
            blocks_info.push_back({.block_size = block_size + occupied_block_metadata_size,
                                          .is_block_occupied = is_occupied});
            current_block = reinterpret_cast<char *>(next_block);
        }

        current_block = reinterpret_cast<char *>(*first_block_ptr);
        char *prev_block_end = heap_start;
        while (current_block && current_block < heap_end) {
            if (current_block > prev_block_end) {
                size_t hole_size = current_block - prev_block_end;
                blocks_info.push_back({.block_size = hole_size, .is_block_occupied = false});
            }

            size_t block_size = *reinterpret_cast<size_t *>(current_block);
            prev_block_end = current_block + occupied_block_metadata_size + block_size;
            void *next_block = *reinterpret_cast<void **>(current_block + sizeof(size_t));
            current_block = reinterpret_cast<char *>(next_block);
        }

        if (prev_block_end < heap_end) {
            size_t hole_size = heap_end - prev_block_end;
            blocks_info.push_back({.block_size = hole_size, .is_block_occupied = false});
        }
    } catch (...) {
        logger->error("Iteration failed.");
        throw;
    }

    return blocks_info;
}

std::vector<allocator_test_utils::block_info> allocator_boundary_tags::get_blocks_info() const {
    logger *logger = get_logger();
    logger->trace("Get_blocks_info started.");
    std::lock_guard<std::mutex> guard(get_mutex());
    auto result = get_blocks_info_inner();

    logger->information("Retrieved " + std::to_string(result.size()) + " blocks.");
    logger->trace("Get_blocks_info finished.");

    return result;
}

inline logger *allocator_boundary_tags::get_logger() const {
    if (!_trusted_memory) return nullptr;

    return *reinterpret_cast<logger **>(_trusted_memory);
}

inline std::string allocator_boundary_tags::get_typename() const noexcept {
    return "allocator_boundary_tags";
}


allocator_boundary_tags::boundary_iterator allocator_boundary_tags::begin() const noexcept {
    return {reinterpret_cast<char *>(_trusted_memory) + allocator_metadata_size};
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::end() const noexcept {
    size_t total_size = *reinterpret_cast<size_t *>(reinterpret_cast<char *>(_trusted_memory) +
                                                    sizeof(logger *) + sizeof(memory_resource *) +
                                                    sizeof(allocator_with_fit_mode::fit_mode));

    return {reinterpret_cast<char *>(_trusted_memory) + total_size};
}

bool allocator_boundary_tags::do_is_equal(const std::pmr::memory_resource &other) const noexcept {
    if (this == &other) return true;

    const auto *derived = dynamic_cast<const allocator_boundary_tags *>(&other);

    if (!derived) return false;

    return this->_trusted_memory == derived->_trusted_memory;
}

bool allocator_boundary_tags::boundary_iterator::operator==(const allocator_boundary_tags::boundary_iterator &other) const noexcept {
    return _occupied_ptr == other._occupied_ptr;
}

bool allocator_boundary_tags::boundary_iterator::operator!=(const allocator_boundary_tags::boundary_iterator &other) const noexcept {
    return !(*this == other);
}

allocator_boundary_tags::boundary_iterator &allocator_boundary_tags::boundary_iterator::operator++() & noexcept {
    size_t block_size = *reinterpret_cast<size_t *>(_occupied_ptr) & ~size_t(1);
    _occupied_ptr = reinterpret_cast<void *>(reinterpret_cast<unsigned char *>(_occupied_ptr) + block_size);
    size_t tag = *reinterpret_cast<size_t *>(_occupied_ptr);
    _occupied = tag & 1;

    return *this;
}

allocator_boundary_tags::boundary_iterator &allocator_boundary_tags::boundary_iterator::operator--() & noexcept {
    void *tmp_ptr = reinterpret_cast<unsigned char *>(_occupied_ptr) - sizeof(size_t);
    size_t block_size = *reinterpret_cast<size_t *>(tmp_ptr) & ~size_t(1);
    _occupied_ptr = reinterpret_cast<unsigned char *>(_occupied_ptr) - block_size;
    size_t tag = *reinterpret_cast<size_t *>(_occupied_ptr);
    _occupied = tag & 1;

    return *this;
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::boundary_iterator::operator++(int) {
    boundary_iterator tmp = *this;
    ++(*this);
    return tmp;
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::boundary_iterator::operator--(int) {
    boundary_iterator tmp = *this;
    --(*this);
    return tmp;
}

size_t allocator_boundary_tags::boundary_iterator::size() const noexcept {
    return *reinterpret_cast<size_t *>(_occupied_ptr) & ~size_t(1);
}

bool allocator_boundary_tags::boundary_iterator::occupied() const noexcept {
    return _occupied;
}

void *allocator_boundary_tags::boundary_iterator::operator*() const noexcept {
    return reinterpret_cast<void *>(reinterpret_cast<unsigned char *>(_occupied_ptr) + sizeof(size_t));
}

allocator_boundary_tags::boundary_iterator::boundary_iterator() : _occupied_ptr(nullptr), _occupied(false), _trusted_memory(nullptr) {}

allocator_boundary_tags::boundary_iterator::boundary_iterator(void *trusted) : _trusted_memory(trusted) {
    _occupied_ptr = trusted;
    size_t tag = *reinterpret_cast<size_t *>(_occupied_ptr);
    _occupied = tag & 1;
}

void *allocator_boundary_tags::boundary_iterator::get_ptr() const noexcept {
    return _occupied_ptr;
}

std::pmr::memory_resource *allocator_boundary_tags::get_parent_resource() const noexcept {
    if (!_trusted_memory) return nullptr;
    return *reinterpret_cast<std::pmr::memory_resource **>(reinterpret_cast<unsigned char *>(_trusted_memory) + sizeof(logger *));
}

inline std::mutex &allocator_boundary_tags::get_mutex() const {
    if (!_trusted_memory) throw std::logic_error("Mutex doesn't exist.");

    auto *ptr = reinterpret_cast<unsigned char *>(_trusted_memory) +
                sizeof(logger *) +
                sizeof(memory_resource *) +
                sizeof(allocator_with_fit_mode::fit_mode) +
                sizeof(size_t);
    return *reinterpret_cast<std::mutex *>(ptr);
}

void *allocator_boundary_tags::allocate_first_fit(size_t size) {
    std::lock_guard<std::mutex> guard(get_mutex());
    const size_t total_size = size + occupied_block_metadata_size;

    size_t allocator_size = *reinterpret_cast<size_t *>(
            static_cast<char *>(_trusted_memory) + sizeof(logger *) + sizeof(memory_resource *) +
            sizeof(allocator_with_fit_mode::fit_mode));

    char *heap_start = static_cast<char *>(_trusted_memory) + allocator_metadata_size;
    char *heap_end = heap_start + allocator_size;
    void **first_block_ptr = reinterpret_cast<void **>(heap_start - sizeof(void *));
    if (!*first_block_ptr) {
        if (heap_end - heap_start >= total_size) return allocate_new_block(heap_start, size, first_block_ptr, heap_end - heap_start);
        return nullptr;
    }

    size_t front_hole = static_cast<char *>(*first_block_ptr) - heap_start;
    if (front_hole >= total_size) return allocate_in_hole(heap_start, size, first_block_ptr, nullptr, *first_block_ptr, front_hole);

    void *current = *first_block_ptr;
    while (current) {
        size_t current_size = *reinterpret_cast<size_t *>(current);
        void *next = *reinterpret_cast<void **>(static_cast<char *>(current) + sizeof(size_t));
        char *current_end = static_cast<char *>(current) + occupied_block_metadata_size + current_size;

        if (!next) {
            size_t end_hole = heap_end - current_end;
            if (end_hole >= total_size) return allocate_in_hole(current_end, size, first_block_ptr, current, nullptr, end_hole);
            break;
        }

        size_t middle_hole = static_cast<char *>(next) - current_end;
        if (middle_hole >= total_size) return allocate_in_hole(current_end, size, first_block_ptr, current, next, middle_hole);

        current = next;
    }

    return nullptr;
}

void *allocator_boundary_tags::allocate_best_fit(size_t size) {
    std::lock_guard<std::mutex> guard(get_mutex());
    const size_t total_size = size + occupied_block_metadata_size;
    size_t allocator_size = *reinterpret_cast<size_t *>(static_cast<char *>(_trusted_memory) +
                                                        sizeof(logger *) +
                                                        sizeof(memory_resource *) +
                                                        sizeof(allocator_with_fit_mode::fit_mode));
    char *heap_start = static_cast<char *>(_trusted_memory) + allocator_metadata_size;
    char *heap_end = heap_start + allocator_size;
    void **first_block_ptr = reinterpret_cast<void **>(heap_start - sizeof(void *));

    void *best_pos = nullptr;
    void *best_prev = nullptr;
    void *best_next = nullptr;
    size_t best_diff = SIZE_MAX;

    if (*first_block_ptr) {
        size_t front_hole = static_cast<char *>(*first_block_ptr) - heap_start;
        if (front_hole >= total_size) {
            best_pos = heap_start;
            best_prev = nullptr;
            best_next = *first_block_ptr;
            best_diff = front_hole - total_size;
        }
    } else {
        if (heap_end - heap_start >= total_size) return allocate_new_block(heap_start, size, first_block_ptr, heap_end - heap_start);
        return nullptr;
    }

    void *current = *first_block_ptr;
    while (current) {
        size_t current_size = *reinterpret_cast<size_t *>(current);
        void *next = *reinterpret_cast<void **>(static_cast<char *>(current) + sizeof(size_t));
        char *current_end = static_cast<char *>(current) + occupied_block_metadata_size + current_size;
        size_t hole_size = (next) ? static_cast<char *>(next) - current_end : heap_end - current_end;
        if (hole_size >= total_size) {
            size_t diff = hole_size - total_size;
            if (diff < best_diff) {
                best_pos = current_end;
                best_prev = current;
                best_next = next;
                best_diff = diff;

                if (diff == 0) break;
            }
        }

        current = next;
    }

    if (best_pos) return allocate_in_hole(reinterpret_cast<char *>(best_pos), size, first_block_ptr, best_prev, best_next, best_diff);
    return nullptr;
}

void *allocator_boundary_tags::allocate_worst_fit(size_t size) {
    std::lock_guard<std::mutex> guard(get_mutex());
    const size_t total_size = size + occupied_block_metadata_size;
    size_t allocator_size = *reinterpret_cast<size_t *>(static_cast<char *>(_trusted_memory) +
                                                        sizeof(logger *) +
                                                        sizeof(memory_resource *) +
                                                        sizeof(allocator_with_fit_mode::fit_mode));
    char *heap_start = static_cast<char *>(_trusted_memory) + allocator_metadata_size;
    char *heap_end = heap_start + allocator_size;
    void **first_block_ptr = reinterpret_cast<void **>(heap_start - sizeof(void *));

    void *worst_pos = nullptr;
    void *worst_prev = nullptr;
    void *worst_next = nullptr;
    size_t worst_size = 0;

    if (!*first_block_ptr) {
        if (heap_end - heap_start >= total_size) return allocate_new_block(heap_start, size, first_block_ptr, heap_end - heap_start);
        return nullptr;
    }

    size_t front_hole = static_cast<char *>(*first_block_ptr) - heap_start;
    if (front_hole >= total_size) {
        worst_size = front_hole;
        worst_pos = static_cast<void *>(heap_start);
        worst_next = *first_block_ptr;
        worst_prev = nullptr;
    }

    void *current = *first_block_ptr;
    while (current) {
        size_t current_size = *reinterpret_cast<size_t *>(current);
        void *next = *reinterpret_cast<void **>(static_cast<char *>(current) + sizeof(size_t));
        char *current_end = static_cast<char *>(current) + occupied_block_metadata_size + current_size;
        size_t hole_size = (next) ? static_cast<char *>(next) - current_end : heap_end - current_end;

        if (hole_size >= total_size && hole_size > worst_size) {
            worst_pos = current_end;
            worst_prev = current;
            worst_next = next;
            worst_size = hole_size;
        }

        current = next;
    }

    if (worst_pos) return allocate_in_hole(reinterpret_cast<char *>(worst_pos), size, first_block_ptr, worst_prev, worst_next, worst_size);
    return nullptr;
}

void *allocator_boundary_tags::allocate_new_block(char *address, size_t size, void **first_block_ptr, size_t size_free) {
    if (size_free - size - occupied_block_metadata_size < occupied_block_metadata_size) {
        size = size_free - size - occupied_block_metadata_size;
    }

    *reinterpret_cast<size_t *>(address) = size;
    *reinterpret_cast<void **>(address + sizeof(size_t)) = nullptr;
    *reinterpret_cast<void **>(address + sizeof(size_t) + sizeof(void *)) = nullptr;
    *reinterpret_cast<void **>(address + sizeof(size_t) + 2 * sizeof(void *)) = _trusted_memory;
    *first_block_ptr = address;

    return address;
}

void *allocator_boundary_tags::allocate_in_hole(char *address, size_t size, void **first_block_ptr,
                                                void *prev_block, void *next_block, size_t size_free) {
    if (size_free - size - occupied_block_metadata_size < occupied_block_metadata_size) {
        size += size_free - size - occupied_block_metadata_size;
    }

    *reinterpret_cast<size_t *>(address) = size;
    *reinterpret_cast<void **>(address + sizeof(size_t)) = next_block;
    *reinterpret_cast<void **>(address + sizeof(size_t) + sizeof(void *)) = prev_block;
    *reinterpret_cast<void **>(address + sizeof(size_t) + 2 * sizeof(void *)) = _trusted_memory;
    if (prev_block) {
        *reinterpret_cast<void **>(static_cast<char *>(prev_block) + sizeof(size_t)) = address;
    } else {
        *first_block_ptr = address;
    }

    if (next_block) {
        *reinterpret_cast<void **>(static_cast<char *>(next_block) + sizeof(size_t) + sizeof(void *)) = address;
    }

    return address;
}

void allocator_boundary_tags::merge_adjacent_blocks(void* block_to_free) {
    logger* logger = get_logger();
    logger->debug("Merging adjacent blocks started.");

    char* heap_start = reinterpret_cast<char*>(_trusted_memory) + allocator_metadata_size;
    void** first_block_ptr = reinterpret_cast<void**>(heap_start - sizeof(void*));

    char* current_block = reinterpret_cast<char*>(block_to_free) - sizeof(size_t);
    size_t current_size = *reinterpret_cast<size_t*>(current_block);
    char* current_end = current_block + occupied_block_metadata_size + current_size;

    void* next_block = *reinterpret_cast<void**>(current_block + sizeof(size_t));
    if (next_block) {
        char* next_block_start = reinterpret_cast<char*>(next_block);
        if (current_end == next_block_start) {
            size_t next_size = *reinterpret_cast<size_t*>(next_block_start);
            void* next_next = *reinterpret_cast<void**>(next_block_start + sizeof(size_t));

            *reinterpret_cast<size_t*>(current_block) = current_size + occupied_block_metadata_size + next_size;
            *reinterpret_cast<void**>(current_block + sizeof(size_t)) = next_next;

            if (next_next) {
                *reinterpret_cast<void**>(reinterpret_cast<char*>(next_next) + sizeof(size_t) + sizeof(void*)) = current_block;
            }

            logger->debug("Merged with next block.");
        }
    }

    void* prev_block = *reinterpret_cast<void**>(current_block + sizeof(size_t) + sizeof(void*));
    if (prev_block) {
        char* prev_block_start = reinterpret_cast<char*>(prev_block);
        size_t prev_size = *reinterpret_cast<size_t*>(prev_block_start);
        char* prev_end = prev_block_start + occupied_block_metadata_size + prev_size;

        if (prev_end == current_block) {
            size_t new_size = prev_size + occupied_block_metadata_size + current_size;
            void* next_block1 = *reinterpret_cast<void**>(current_block + sizeof(size_t));

            *reinterpret_cast<size_t*>(prev_block_start) = new_size;
            *reinterpret_cast<void**>(prev_block_start + sizeof(size_t)) = next_block1;

            if (next_block1) {
                *reinterpret_cast<void**>(reinterpret_cast<char*>(next_block1) + sizeof(size_t) + sizeof(void*)) = prev_block_start;
            }

            if (current_block == *first_block_ptr) {
                *first_block_ptr = prev_block_start;
            }

            logger->debug("Merged with previous block.");
        }
    }

    logger->debug("Merging adjacent blocks finished.");
}