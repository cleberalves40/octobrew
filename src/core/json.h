// SPDX-License-Identifier: GPL-2.0-or-later
//
// Leitor de JSON (RFC 8259) em C++ padrão, para a saída do brew
// ("--json=v2") e o cache da API do Homebrew. Só leitura, sem dependências.

#ifndef OCTOBREW_TUI_JSON_H
#define OCTOBREW_TUI_JSON_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace json {

class Value
{
public:
    enum Type { Null, Bool, Number, String, Array, Object };
    using Member = std::pair<std::string, Value>;

    Value() = default;

    Type type() const { return m_type; }
    bool isNull() const { return m_type == Null; }
    bool isBool() const { return m_type == Bool; }
    bool isNumber() const { return m_type == Number; }
    bool isString() const { return m_type == String; }
    bool isArray() const { return m_type == Array; }
    bool isObject() const { return m_type == Object; }

    /** Valores com fallback quando o tipo não confere (null, ausente...). */
    bool toBool(bool fallback = false) const { return m_type == Bool ? m_bool : fallback; }
    double toNumber(double fallback = 0) const { return m_type == Number ? m_number : fallback; }
    std::int64_t toInt(std::int64_t fallback = 0) const
    {
        return m_type == Number ? static_cast<std::int64_t>(m_number) : fallback;
    }
    std::string toString(const std::string &fallback = std::string()) const
    {
        return m_type == String ? m_string : fallback;
    }
    const std::string &str() const { return m_string; }

    /** Itens de um array (vazio para os outros tipos). */
    const std::vector<Value> &items() const { return m_items; }
    /** Membros de um objeto, na ordem do texto (vazio para os outros tipos). */
    const std::vector<Member> &members() const { return m_members; }
    size_t size() const { return m_type == Array ? m_items.size() : m_members.size(); }

    /** Membro key de um objeto; um Value null se não houver. */
    const Value &operator[](const std::string &key) const;
    /** Item i de um array; um Value null se não houver. */
    const Value &operator[](size_t i) const;
    bool contains(const std::string &key) const;

    /** Strings de um array, ignorando os itens de outro tipo. */
    std::vector<std::string> strings() const;

private:
    friend class Parser;
    Type m_type = Null;
    bool m_bool = false;
    double m_number = 0;
    std::string m_string;
    std::vector<Value> m_items;
    std::vector<Member> m_members;
};

/** Lê text; lança std::runtime_error com a posição se não for JSON válido. */
Value parse(const std::string &text);

} // namespace json

#endif
