#include <libgen.h> /* for basename(). */
#include <stdarg.h> /* for va_list, va_start(), va_arg(). */
#include <stdio.h> /* for fprintf(), vfprintf(). */
#include <stdlib.h> /* for exit(), EXIT_FAILURE. */
#include <string.h> /* for strlen(). */
#include <time.h> /* for time_t, time(), tm and gmtime(). */
#include <unistd.h>
#include <sys/types.h> /* for pid_t. */

#include "debug.h"

/**
 * @brief Writes a string passed as argument to stderr with a timestamp.
 *
 * @param line the line number this function was called.
 */
void util_log(int line, ...) {
  const time_t now = time(NULL);
  if (now == (time_t)-1) return;

  struct tm* time = gmtime(&now);
  if (!time) return;
  fprintf(stderr, "[LOG - %i:%i:%i] - ", time->tm_hour, time->tm_min,
          time->tm_sec);

  va_list args;
  va_start(args, line);
  const char* fmt = va_arg(args, char*);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
}

/**
 * @brief Writes a simple message to stderr.
 *
 * @param line the line number this function was called.
 */
void util_message(int line, ...) {
  va_list args;
  va_start(args, line);
  const char* fmt = va_arg(args, char*);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
}

#ifndef NDEBUG
/**
 * @brief Writes a warning message to stderr in the form :
 * [WARN [PID] - @FILENAME:LINE_NUMBER - MESSAGE]
 *
 * @param filename The filename this function was called.
 *
 * @param line the line number this function was called.
 */
void util_warn(const char* filename, int line, ...) {
  const pid_t pid = getpid();
  fprintf(stderr, "[WARN [%i] - @%s:%i]", pid, filename, line);

  va_list args;
  va_start(args, line);
  const char* fmt = va_arg(args, char*);

  if (!fmt || strlen(fmt) == 0) {
    fprintf(stderr, "\n");
    return;
  }

  fprintf(stderr, " - ");
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
}

/**
 * @brief Writes a error message to stderr in the form :
 * [WARN [PID] - @FILENAME:LINE_NUMBER - MESSAGE] and
 * exits the application.
 *
 * @param filename The filename this function was called.
 *
 * @param line the line number this function was called.
 */
#if defined __STDC_VERSION__ && __STDC_VERSION__ == 201112L
_Noreturn void util_die(const char* filename, int line, ...) {
#else
void util_die(const char* filename, int line, ...) {
#endif /* __STDC_VERSION__ && __STDC_VERSION__ == 201112L */
  const pid_t pid = getpid();
  fprintf(stderr, "[DIE [%i] - @%s:%i]", pid, filename, line);

  va_list args;
  va_start(args, line);
  const char* fmt = va_arg(args, char*);
  if (!fmt || strlen(fmt) == 0) goto DIE;

  fprintf(stderr, " - ");
  vfprintf(stderr, fmt, args);

DIE:
  fprintf(stderr, "\n");
  exit(EXIT_FAILURE);
}
#endif /* NDEBUG */
