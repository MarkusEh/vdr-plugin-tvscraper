#ifndef __LARGE_STRING_H
#define __LARGE_STRING_H

#include <string.h>
#include <string>
#include <stdio.h>
#include <stdlib.h>

class cLargeString {
  private:
    char *m_s = NULL;
    char *m_string_end = NULL; // *m_string_end = 0; m_string_end must be < m_buffer_end, because m_buffer_end is excluded
    char *m_buffer_end = NULL; // buffer is between [m_s;m_buffer_end), i.e. m_buffer_end is excluded
    size_t m_increaseSize = 0;
    size_t m_maxSize = 0;
    const char *m_nameData;
    int m_nameLen;
    bool m_debugBufferSize = false;
    bool m_endBorrowed = false;
    bool enlarge(size_t increaseSize = 0);
    bool appendLen(size_t len);  // enure buffer is large enough to append len characters
    void setMaxSize() { m_maxSize = std::max(m_maxSize, (size_t)(m_string_end - m_s)); }
    void init(size_t initialSize, size_t increaseSize, bool debugBufferSize);
    void loadFile(const char *filename, bool *exists);
  public:
    cLargeString(const cLargeString& o) = delete;
    cLargeString &operator= (const cLargeString &) = delete;
    cLargeString(cLargeString&& o) = default;
    cLargeString &operator= (cLargeString &&) = default;
    template<std::size_t N>
    cLargeString(const char (&name)[N], size_t initialSize, size_t increaseSize = 0, bool debugBufferSize = false) {
      m_nameData = name;
      m_nameLen = static_cast<int>(N) - 1;
      init(initialSize, increaseSize, debugBufferSize);
    }
    template<std::size_t N>
    cLargeString(const char (&name)[N], const char *filename, bool *exists = NULL) {
      m_nameData = name;
      m_nameLen = static_cast<int>(N) - 1;
      loadFile(filename, exists);
    }
    ~cLargeString();
    char *data() { if (m_string_end) *m_string_end = 0; return m_s; }
    const char *c_str() const { if (m_string_end) *m_string_end = 0; return m_s; }
    bool append(char c);
    bool append(const char *s);
    bool append(const char *s, size_t len);
    bool append(std::string s) { return append(s.c_str(), s.length()); }
    bool append(int i);
    char *borrowEnd(size_t len); // len: upper limit of characters that can be written
    bool finishBorrow(size_t len);  // len: number of actually written characters
    bool finishBorrow();  // number of actually written characters: zero terminated
    bool clear();
    size_t length() const { return m_string_end - m_s; }
    bool empty() const { return m_string_end == m_s; }
    cLargeString &erase(size_t index = 0) { setMaxSize(); m_string_end = std::min(m_string_end, m_s + index); return *this;}
    char operator[](size_t i) const { return m_s?*(m_s + i):0; }
    operator std::string_view() const { return m_s?std::string_view(m_s, m_string_end - m_s):std::string_view(); }
    const char *nameData() const { return m_nameData; }
    int nameLen() const { return m_nameLen; }
};

#endif  // __LARGE_STRING_H
