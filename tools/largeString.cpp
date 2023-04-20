#include <string.h>
#include <string>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "largeString.h"

void cLargeString::init(size_t initialSize, size_t increaseSize, bool debugBufferSize) {
  m_debugBufferSize = debugBufferSize;
  if (initialSize <= 0) {
    esyslog("cLargeString::cLargeString, ERROR name = %.*s, initialSize = %zu", nameLen(), nameData(), initialSize);
    initialSize = 100;
  }
  if (increaseSize > 0) m_increaseSize = increaseSize;
  else m_increaseSize = std::max((size_t)10, initialSize / 2);
  m_s = (char *) malloc(initialSize * sizeof(char));
  if (!m_s) {
    esyslog("cLargeString::init, ERROR out of memory, name = %.*s, initialSize = %zu", nameLen(), nameData(), initialSize);
//  throw std::runtime_error("cLargeString::cLargeString, ERROR out of memory");
    return;
  }
  m_buffer_end = m_s + initialSize;
  m_string_end = m_s;
}
void cLargeString::loadFile(const char *filename, bool *exists) {
  if (exists) *exists = false;
  if (!filename) return;
  struct stat buffer;                                                                                                                       
  if (stat(filename, &buffer) != 0) return;

// file exists, length buffer.st_size
  if (exists) *exists = true;
  if (buffer.st_size == 0) return;  // empty file
  m_s = (char *) malloc((size_t)(buffer.st_size + 1) * sizeof(char));  // add one. So we can add the 0 string terminator
  if (!m_s) {
    esyslog("tvscraper, cLargeString::cLargeString, ERROR out of memory, filename = %s, requested size = %zu", filename, (size_t)(buffer.st_size + 1));
//  throw std::runtime_error("cLargeString::cLargeString, ERROR out of memory");
    return;
  }
  m_buffer_end = m_s + buffer.st_size + 1;
  m_string_end = m_s;
  FILE *f = fopen(filename, "rb");
  if (!f) {
    esyslog("tvscraper, ERROR: stat OK, fopen fails, filename %s", filename);
    return;
  }
  size_t num_read = fread (m_s, 1, (size_t)buffer.st_size, f);
  if (num_read != (size_t)buffer.st_size) {
    esyslog("tvscraper, ERROR: num_read = %zu, buffer.st_size = %zu, ferror %i, filename %s", num_read, (size_t)buffer.st_size, ferror(f), filename);
  }
  fclose(f);
  m_string_end = m_s + num_read;
}

cLargeString::~cLargeString() {
  if (!m_s) return;
  free (m_s);
  setMaxSize();
  size_t buffer_len = m_buffer_end - m_s;
  if (m_debugBufferSize && buffer_len > m_maxSize * 2) esyslog("cLargeString::cLargeString WARNING too large buffer, name = %.*s, buffer %zu, len %zu", nameLen(), nameData(), buffer_len, m_maxSize);
}

bool cLargeString::clear() {
  if (!m_s) return false;
  setMaxSize();
  m_string_end = m_s;
  m_endBorrowed = false;
  return true;
}

char *cLargeString::borrowEnd(size_t len) {
  if (!m_s) return NULL;
  if (!appendLen(len)) return NULL;
  m_endBorrowed = true;
  return m_string_end;
}
bool cLargeString::finishBorrow(size_t len) {
  if (!m_s) return false;
  if (!m_endBorrowed) return true;
  m_endBorrowed = false;
  if (m_string_end + len >= m_buffer_end) {
    esyslog("cLargeString::finishBorrow(size_t len), ERROR name = %.*s, len %zu too large, available %zu", nameLen(), nameData(), len, m_buffer_end - m_string_end - 1);
    m_string_end = m_buffer_end - 1;
    return false;
  }
  m_string_end = m_string_end + len;
  return true;
}

bool cLargeString::finishBorrow() {
  if (!m_s) return false;
  if (!m_endBorrowed) return true;
  m_endBorrowed = false;
  char *end = strchr(m_string_end, 0);
  if (!end) return false;
  if (end >= m_buffer_end) {
    esyslog("cLargeString::finishBorrow(), ERROR name = %.*s, end >= m_buffer_end, available %zu", nameLen(), nameData(), m_buffer_end - m_string_end - 1);
    m_string_end = m_buffer_end - 1;
    return false;
  }
  m_string_end = end;
  return true;
}

bool cLargeString::append(char c) {
  if (!m_s) return false;
  if (!appendLen(1)) return false;
  *(m_string_end++) = c;
  return true;
}

bool cLargeString::append(int i) {
  if (!m_s) return false;
  if (i < 0) {
    if(!appendLen(2)) return false;
    *(m_string_end++) = '-';
    i *= -1;
  }
  if (i < 10) return append((char)('0' + i));
  char *buf;
  for (int j = i; j > 0;) {
    for (j = i, buf = m_buffer_end - 2; j > 0 && buf >= m_string_end; j /= 10, buf--) *buf = j%10 + '0';
    if (j > 0 && !enlarge()) return false;
  }
  if (buf >= m_string_end) for (buf++; buf < m_buffer_end - 1;m_string_end++, buf++) *m_string_end = *buf;
  else m_string_end = m_buffer_end - 1;
  return true;
}

bool cLargeString::appendLen(size_t len) {
  if (!m_s) return false;
  char *newStringEnd = m_string_end + len;
  if (newStringEnd < m_buffer_end) return true;
  return enlarge(newStringEnd + 1 - m_buffer_end);
}

bool cLargeString::append(const char *s, size_t len) {
  if (!m_s) return false;
  if (!s || len == 0) return true;
  if (!appendLen(len)) return false;
  for (char *newStringEnd = m_string_end + len; m_string_end < newStringEnd; s++, m_string_end++) *m_string_end = *s;
  return true;
}

bool cLargeString::append(const char *s) {
  if (!m_s) return false;
  if (!s || !*s) return true;
  for (; *s && m_string_end < m_buffer_end; s++, m_string_end++) *m_string_end = *s;
  while(m_string_end == m_buffer_end) {
    if (!enlarge()) return false;
    for (; *s && m_string_end < m_buffer_end; s++, m_string_end++) *m_string_end = *s;
  }
  return true;
}

bool cLargeString::enlarge(size_t increaseSize) {
  if (!m_s) return false;
  increaseSize = std::max(increaseSize, m_increaseSize);
  increaseSize = std::max(increaseSize, (size_t)((m_buffer_end - m_s)/2) );
  size_t stringLength = m_string_end - m_s;
  size_t newSize = m_buffer_end - m_s + increaseSize;
  if (m_debugBufferSize) esyslog("cLargeString::cLargeString, WARNING realloc required!!!, name = %.*s, new Size = %zu", nameLen(), nameData(), newSize);
  char *tmp = (char *)realloc(m_s, newSize * sizeof(char));
  if (!tmp) {
    esyslog("cLargeString::cLargeString, ERROR out of memory, name = %.*s, new Size = %zu", nameLen(), nameData(), newSize);
    throw std::runtime_error("cLargeString::cLargeString, ERROR out of memory (enlarge)");
    return false;
  }
  m_s = tmp;
  m_buffer_end = m_s + newSize;
  m_string_end = m_s + stringLength;
  return true;
}
