// syobon-action-ps5 - what the title runtime lacks for PacBrew's libraries.
// Copyright (C) 2026 Andrei Kvapil
// SPDX-License-Identifier: GPL-3.0-or-later

// Symbols PacBrew's libraries and libc++ import that the title's runtime does
// not provide.
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <langinfo.h>

extern "C"
{

    // libiconv asks for the locale's codeset: the title only runs in the C locale
    char *nl_langinfo(nl_item item)
    {
        static char codeset[] = "UTF-8";
        static char empty[] = "";
        return item == CODESET ? codeset : empty;
    }

    // SDL calls it; the console's copy is in libScePosixForWebKit, which a title
    // does not load
    char *strcasestr(const char *haystack, const char *needle)
    {
        size_t length = strlen(needle);
        for (; *haystack; haystack++)
            if (strncasecmp(haystack, needle, length) == 0)
                return (char *)haystack;
        return length ? nullptr : (char *)haystack;
    }

    // SDL's PS5 video driver opens the USB keyboard and polls the on-screen
    // keyboard dialog; in a title sceKeyboardInit crashes. Report no keyboard and
    // no dialog, which SDL already copes with.
    int __wrap_sceKeyboardInit(void)
    {
        return -1;
    }

    int __wrap_sceKeyboardOpen(int, int, int, void *)
    {
        return -1;
    }

    int __wrap_sceKeyboardReadState(int, void *)
    {
        return -1;
    }

    int __wrap_sceKeyboardClose(int)
    {
        return 0;
    }

    int __wrap_sceImeDialogGetStatus(void)
    {
        return 0; // SCE_IME_DIALOG_STATUS_NONE
    }

    // FreeBSD's MB_CUR_MAX
    int ___mb_cur_max(void)
    {
        return 1;
    }
}

namespace std
{
inline namespace __1
{
[[noreturn]] void __libcpp_verbose_abort(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    abort();
}
} // namespace __1
} // namespace std

// The title's libc heap stops at about 16 MiB, and SDL wants an 8 MiB
// 1920x1080 window surface plus the game's own. Large blocks are mapped
// directly instead; the registry tells them apart from the heap's.
#include <atomic>
#include <sys/mman.h>

extern "C"
{
    void *__real_malloc(size_t);
    void __real_free(void *);
    void *__real_calloc(size_t, size_t);
    void *__real_realloc(void *, size_t);
    int __real_posix_memalign(void **, size_t, size_t);
    size_t malloc_usable_size(void *);
}

namespace
{
constexpr size_t kMappedThreshold = 256 << 10;
constexpr size_t kMappedMax = 1024;

struct Mapped
{
    void *base;
    size_t size;
};

Mapped mapped[kMappedMax];
size_t mappedCount;
std::atomic_flag mappedLock = ATOMIC_FLAG_INIT;

struct Guard
{
    Guard()
    {
        while (mappedLock.test_and_set(std::memory_order_acquire))
        {
        }
    }
    ~Guard()
    {
        mappedLock.clear(std::memory_order_release);
    }
};

void *mapBlock(size_t size)
{
    size = (size + 0x3fff) & ~size_t(0x3fff);
    void *base = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (base == MAP_FAILED)
        return nullptr;
    Guard guard;
    if (mappedCount == kMappedMax)
    {
        munmap(base, size);
        return nullptr;
    }
    mapped[mappedCount++] = {base, size};
    return base;
}

// The mapped size of block, or 0 when the heap owns it; release drops it
size_t findBlock(void *block, bool release)
{
    Guard guard;
    for (size_t i = 0; i < mappedCount; i++)
    {
        if (mapped[i].base != block)
            continue;
        size_t size = mapped[i].size;
        if (release)
            mapped[i] = mapped[--mappedCount];
        return size;
    }
    return 0;
}
} // namespace

extern "C"
{
    void *__wrap_malloc(size_t size)
    {
        if (size >= kMappedThreshold)
            return mapBlock(size);
        return __real_malloc(size);
    }

    void __wrap_free(void *block)
    {
        if (!block)
            return;
        if (size_t size = findBlock(block, true))
            munmap(block, size);
        else
            __real_free(block);
    }

    void *__wrap_calloc(size_t count, size_t size)
    {
        if (size && count > SIZE_MAX / size)
            return nullptr;
        if (count * size >= kMappedThreshold)
            return mapBlock(count * size); // anonymous mappings are zeroed
        return __real_calloc(count, size);
    }

    void *__wrap_realloc(void *block, size_t size)
    {
        if (!block)
            return __wrap_malloc(size);
        size_t old = findBlock(block, false);
        if (!old && size < kMappedThreshold)
            return __real_realloc(block, size);
        if (old && size <= old)
            return block;
        if (!old)
            old = malloc_usable_size(block);
        void *moved = __wrap_malloc(size);
        if (!moved)
            return nullptr;
        memcpy(moved, block, old < size ? old : size);
        __wrap_free(block);
        return moved;
    }

    int __wrap_posix_memalign(void **result, size_t alignment, size_t size)
    {
        if (size >= kMappedThreshold && alignment <= 0x4000)
        {
            *result = mapBlock(size);
            return *result ? 0 : ENOMEM;
        }
        return __real_posix_memalign(result, alignment, size);
    }
}
