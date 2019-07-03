#pragma once

#include <assert.h>
#include <string.h>
#include <string>

class bufstring
{
public:
    bufstring(wchar_t* buf, size_t size)
        : m_buf(buf), m_size(size), m_len(0)
    {
        m_buf[m_len] = L'\0';
        assert(m_buf[m_len] == L'\0');
    }

    bufstring(wchar_t* buf, size_t size, size_t len)
        : m_buf(buf), m_size(size), m_len(len)
    {
        m_buf[m_len] = L'\0';
        assert(m_buf[m_len] == L'\0');
    }

    wchar_t* c_str() { return m_buf; }
    const wchar_t* c_str() const { return m_buf; }
    wchar_t* begin() { return m_buf; }
    const wchar_t* begin() const { return m_buf; }
    wchar_t* end() { return m_buf + m_len; }
    const wchar_t* end() const { return m_buf + m_len; }

    void erase(size_t b)
    {
        assert(b >= 0);
        assert(b < m_len);
        wmemmove(begin() + b, begin() + b + 1, m_len - b);
        --m_len;
        assert(m_buf[m_len] == L'\0');
    }

    void insert(size_t b, wchar_t c)
    {
        assert(b >= 0);
        assert(b <= m_len);
        wmemmove(begin() + b + 1, begin() + b, m_len - b + 1);
        m_buf[b] = c;
        ++m_len;
        assert(m_buf[m_len] == L'\0');
    }

    void replace(size_t b, size_t l, const std::wstring& s)
    {
        assert(b >= 0);
        assert(b <= m_len);
        assert(l >= 0);
        assert((b + l) <= m_len);
        int d = (int) (s.length() - l);
        wmemmove(begin() + b + l + d, begin() + b + l, m_len - b - l + 1);
        wmemmove(begin() + b, s.c_str(), s.length());
        m_len += d;
        assert(m_buf[m_len] == L'\0');
    }

    void append(const wchar_t* w, size_t l)
    {
        wmemmove(end(), w, l);
        m_len += l;
        m_buf[m_len] = L'\0';
    }

    std::wstring substr(size_t b, size_t l) const
    {
        assert(b >= 0);
        assert(b <= m_len);
        assert(l >= 0);
        assert((b + l) <= m_len);
        return std::wstring(begin() + b, l);
    }

    size_t length() const
    {
        return m_len;
    }

    void clear()
    {
        m_len = 0;
        m_buf[m_len] = L'\0';
        assert(m_buf[m_len] == L'\0');
    }

    std::wstring str() const
    {
        return std::wstring(begin(), end());
    }

    int compare(size_t b, size_t l, const wchar_t* s) const
    {
        return wcsncmp(begin() + b, s, l);
    }

    bufstring& operator+=(const wchar_t* w)
    {
        append(w, wcslen(w));
        return *this;
    }

    bufstring& operator+=(wchar_t w)
    {
        append(&w, 1);
        return *this;
    }

    bufstring& operator+=(const std::wstring& s)
    {
        append(s.data(), s.length());
        return *this;
    }

    wchar_t operator[](size_t i) const
    {
        assert(i >= 0);
        assert(i < m_len);
        return begin()[i];
    }

private:
    wchar_t* m_buf;
    const size_t m_size;
    size_t m_len;
};