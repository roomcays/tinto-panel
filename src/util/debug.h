#ifndef UTIL_DEBUG_H
#define UTIL_DEBUG_H

/* Silence the compiler's unused variable/parameter. */
#define UNUSED(var) ((void)var);

void util_message(int line, ...);
void util_log(int line, ...);

#if defined __GNUC__ && !defined __clang__
#define LOG(...) util_log(__LINE__, ## __VA_ARGS__)
#define MESSAGE(...) util_message(__LINE__, ## __VA_ARGS__)
#else
#define LOG(...) util_log(__LINE__, __VA_ARGS__)
#define MESSAGE(...) util_message(__LINE__, __VA_ARGS__)
#endif /* _GNUC_ and __clang__ */

#ifdef NDEBUG
#define DIE(...)
#define WARN(...)
#else
void util_warn(const char* file, int line, ...);

#if defined __STDC_VERSION__ && __STDC_VERSION__ == 201112L
_Noreturn void util_die(const char* file, int line, ...);
#else
void util_die(const char* file, int line, ...);
#endif /* __STDC_VERSION__ && __STDC_VERSION__ == 201112L */

#if defined __GNUC__ && !defined __clang__
#define WARN(...) util_warn(__FILE__, __LINE__, ## __VA_ARGS__)
#define DIE(...) util_die(__FILE__, __LINE__, ## __VA_ARGS__)
#else
#define WARN(...) util_warn(__FILE__, __LINE__, __VA_ARGS__)
#define DIE(...) util_die(__FILE__, __LINE__, __VA_ARGS__)
#endif /* _GNUC_ and __clang__ */

#endif /* NDEBUG */

#endif /* UTIL_DEBUG_H */
