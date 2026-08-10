#pragma once

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

namespace max31865_test {

struct TestCase {
    const char *name;
    bool (*function)();
};

inline bool floatsNear(float expected, float actual, float tolerance)
{
    return isfinite(expected) && isfinite(actual) && tolerance >= 0.0F &&
        fabsf(expected - actual) <= tolerance;
}

inline int runTests(const TestCase *tests, size_t count)
{
    size_t passed = 0U;
    for (size_t index = 0U; index < count; ++index) {
        const bool ok = tests[index].function();
        fprintf(stdout, "%s: %s\n", ok ? "PASS" : "FAIL", tests[index].name);
        if (ok) {
            ++passed;
        }
    }
    fprintf(stdout, "%zu/%zu tests passed\n", passed, count);
    return passed == count ? 0 : 1;
}

} // namespace max31865_test

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(                                                           \
                stderr,                                                        \
                "%s:%d: CHECK failed: %s\n",                                 \
                __FILE__,                                                      \
                __LINE__,                                                      \
                #condition);                                                   \
            return false;                                                      \
        }                                                                      \
    } while (false)

#define CHECK_EQ(expected, actual)                                             \
    CHECK((expected) == (actual))

#define CHECK_NEAR(expected, actual, tolerance)                                \
    CHECK(max31865_test::floatsNear((expected), (actual), (tolerance)))

#define CHECK_CSTR_EQ(expected, actual)                                        \
    CHECK((actual) != nullptr && strcmp((expected), (actual)) == 0)
