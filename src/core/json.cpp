// SPDX-License-Identifier: GPL-2.0-or-later

#include "json.h"

#include <cstdlib>
#include <stdexcept>

namespace json {

namespace {

const Value kNull;

// Limite de aninhamento: o cache da API tem 6 ou 7 níveis.
constexpr int kMaxDepth = 256;

void appendUtf8(std::string &out, std::uint32_t cp)
{
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

} // namespace

class Parser
{
public:
    explicit Parser(const std::string &text)
        : m_s(text)
    {
    }

    Value document()
    {
        Value v;
        value(v, 0);
        space();
        if (m_pos != m_s.size())
            fail("unexpected text after the value");
        return v;
    }

private:
    [[noreturn]] void fail(const std::string &what) const
    {
        throw std::runtime_error("JSON: " + what + " at offset " + std::to_string(m_pos));
    }

    void space()
    {
        while (m_pos < m_s.size()) {
            const char c = m_s[m_pos];
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
                break;
            ++m_pos;
        }
    }

    bool literal(const char *word)
    {
        size_t i = 0;
        while (word[i]) {
            if (m_pos + i >= m_s.size() || m_s[m_pos + i] != word[i])
                return false;
            ++i;
        }
        m_pos += i;
        return true;
    }

    void value(Value &v, int depth)
    {
        if (depth > kMaxDepth)
            fail("nesting too deep");
        space();
        if (m_pos >= m_s.size())
            fail("unexpected end");
        const char c = m_s[m_pos];
        if (c == '{') {
            object(v, depth);
        } else if (c == '[') {
            array(v, depth);
        } else if (c == '"') {
            v.m_type = Value::String;
            string(v.m_string);
        } else if (c == 't' && literal("true")) {
            v.m_type = Value::Bool;
            v.m_bool = true;
        } else if (c == 'f' && literal("false")) {
            v.m_type = Value::Bool;
            v.m_bool = false;
        } else if (c == 'n' && literal("null")) {
            v.m_type = Value::Null;
        } else if (c == '-' || (c >= '0' && c <= '9')) {
            number(v);
        } else {
            fail(std::string("unexpected character '") + c + "'");
        }
    }

    void object(Value &v, int depth)
    {
        v.m_type = Value::Object;
        ++m_pos;   // {
        space();
        if (m_pos < m_s.size() && m_s[m_pos] == '}') {
            ++m_pos;
            return;
        }
        for (;;) {
            space();
            if (m_pos >= m_s.size() || m_s[m_pos] != '"')
                fail("expected a member name");
            v.m_members.emplace_back();
            Value::Member &m = v.m_members.back();
            string(m.first);
            space();
            if (m_pos >= m_s.size() || m_s[m_pos] != ':')
                fail("expected ':'");
            ++m_pos;
            value(m.second, depth + 1);
            space();
            if (m_pos < m_s.size() && m_s[m_pos] == ',') {
                ++m_pos;
                continue;
            }
            if (m_pos < m_s.size() && m_s[m_pos] == '}') {
                ++m_pos;
                return;
            }
            fail("expected ',' or '}'");
        }
    }

    void array(Value &v, int depth)
    {
        v.m_type = Value::Array;
        ++m_pos;   // [
        space();
        if (m_pos < m_s.size() && m_s[m_pos] == ']') {
            ++m_pos;
            return;
        }
        for (;;) {
            v.m_items.emplace_back();
            value(v.m_items.back(), depth + 1);
            space();
            if (m_pos < m_s.size() && m_s[m_pos] == ',') {
                ++m_pos;
                continue;
            }
            if (m_pos < m_s.size() && m_s[m_pos] == ']') {
                ++m_pos;
                return;
            }
            fail("expected ',' or ']'");
        }
    }

    std::uint32_t hex4()
    {
        if (m_pos + 4 > m_s.size())
            fail("truncated \\u escape");
        std::uint32_t cp = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = m_s[m_pos++];
            cp <<= 4;
            if (c >= '0' && c <= '9')
                cp |= static_cast<std::uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f')
                cp |= static_cast<std::uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
                cp |= static_cast<std::uint32_t>(c - 'A' + 10);
            else
                fail("invalid \\u escape");
        }
        return cp;
    }

    void string(std::string &out)
    {
        ++m_pos;   // "
        // Sem escapes, o trecho é copiado de uma vez: é o caso comum, e o
        // cache da API tem dezenas de milhares de strings.
        for (;;) {
            const size_t start = m_pos;
            while (m_pos < m_s.size() && m_s[m_pos] != '"' && m_s[m_pos] != '\\'
                   && static_cast<unsigned char>(m_s[m_pos]) >= 0x20)
                ++m_pos;
            out.append(m_s, start, m_pos - start);
            if (m_pos >= m_s.size())
                fail("unterminated string");
            const char c = m_s[m_pos];
            if (c == '"') {
                ++m_pos;
                return;
            }
            if (c != '\\')
                fail("control character in string");
            ++m_pos;
            if (m_pos >= m_s.size())
                fail("unterminated string");
            const char e = m_s[m_pos++];
            switch (e) {
            case '"':  out += '"'; break;
            case '\\': out += '\\'; break;
            case '/':  out += '/'; break;
            case 'b':  out += '\b'; break;
            case 'f':  out += '\f'; break;
            case 'n':  out += '\n'; break;
            case 'r':  out += '\r'; break;
            case 't':  out += '\t'; break;
            case 'u': {
                std::uint32_t cp = hex4();
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    // Par substituto: 😀.
                    if (m_pos + 6 <= m_s.size() && m_s[m_pos] == '\\' && m_s[m_pos + 1] == 'u') {
                        m_pos += 2;
                        const std::uint32_t low = hex4();
                        if (low >= 0xDC00 && low <= 0xDFFF)
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                        else
                            cp = 0xFFFD;
                    } else {
                        cp = 0xFFFD;
                    }
                } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    cp = 0xFFFD;
                }
                appendUtf8(out, cp);
                break;
            }
            default:
                fail("invalid escape");
            }
        }
    }

    void number(Value &v)
    {
        const size_t start = m_pos;
        if (m_s[m_pos] == '-')
            ++m_pos;
        const auto digits = [this] {
            const size_t from = m_pos;
            while (m_pos < m_s.size() && m_s[m_pos] >= '0' && m_s[m_pos] <= '9')
                ++m_pos;
            return m_pos > from;
        };
        if (!digits())
            fail("invalid number");
        if (m_pos < m_s.size() && m_s[m_pos] == '.') {
            ++m_pos;
            if (!digits())
                fail("invalid number");
        }
        if (m_pos < m_s.size() && (m_s[m_pos] == 'e' || m_s[m_pos] == 'E')) {
            ++m_pos;
            if (m_pos < m_s.size() && (m_s[m_pos] == '+' || m_s[m_pos] == '-'))
                ++m_pos;
            if (!digits())
                fail("invalid number");
        }
        v.m_type = Value::Number;
        v.m_number = std::strtod(m_s.substr(start, m_pos - start).c_str(), nullptr);
    }

    const std::string &m_s;
    size_t m_pos = 0;
};

const Value &Value::operator[](const std::string &key) const
{
    if (m_type != Object)
        return kNull;
    for (const auto &m : m_members)
        if (m.first == key)
            return m.second;
    return kNull;
}

const Value &Value::operator[](size_t i) const
{
    return m_type == Array && i < m_items.size() ? m_items[i] : kNull;
}

bool Value::contains(const std::string &key) const
{
    if (m_type != Object)
        return false;
    for (const auto &m : m_members)
        if (m.first == key)
            return true;
    return false;
}

std::vector<std::string> Value::strings() const
{
    std::vector<std::string> out;
    for (const auto &i : m_items)
        if (i.isString())
            out.push_back(i.m_string);
    return out;
}

Value parse(const std::string &text)
{
    return Parser(text).document();
}

} // namespace json
