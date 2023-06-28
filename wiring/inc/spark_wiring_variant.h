/*
 * Copyright (c) 2023 Particle Industries, Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <variant>
#include <type_traits>
#include <algorithm>
#include <charconv>
#include <cstring>
#include <cstdint>

#include "spark_wiring_string.h"
#include "spark_wiring_vector.h"
#include "spark_wiring_map.h"

#include "debug.h"

namespace spark {

class JSONValue;

} // namespace spark

namespace particle {

using spark::JSONValue;

class Variant;

typedef Vector<Variant> VariantArray;
typedef Map<String, Variant> VariantMap;

class Variant {
public:
    enum Type {
        NULL_,
        BOOL,
        INT,
        UINT,
        INT64,
        UINT64,
        DOUBLE,
        STRING,
        ARRAY,
        MAP
    };

    Variant() = default;

    Variant(const std::monostate&) :
            Variant() {
    }

    Variant(bool val) :
            v_(val) {
    }

    Variant(int val) :
            v_(val) {
    }

    Variant(unsigned val) :
            v_(val) {
    }
#ifdef __LP64__
    Variant(long val) :
            v_(static_cast<int64_t>(val)) {
    }

    Variant(unsigned long val) :
            v_(static_cast<uint64_t>(val)) {
    }
#else
    Variant(long val) :
            v_(static_cast<int>(val)) {
    }

    Variant(unsigned long val) :
            v_(static_cast<unsigned>(val)) {
    }
#endif
    Variant(long long val) :
            v_(val) {
    }

    Variant(unsigned long long val) :
            v_(val) {
    }

    Variant(double val) :
            v_(val) {
    }

    Variant(const char* val) :
            v_(String(val)) {
    }

    Variant(String val) :
            v_(std::move(val)) {
    }

    Variant(VariantArray val) :
            v_(std::move(val)) {
    }

    Variant(VariantMap val) :
            v_(std::move(val)) {
    }

    Variant(const Variant& var) :
            v_(var.v_) {
    }

    Variant(Variant&& var) :
            Variant() {
        swap(*this, var);
    }

    Type type() const {
        return static_cast<Type>(v_.index());
    }

    bool isNull() const {
        return is<std::monostate>();
    }

    bool isBool() const {
        return is<bool>();
    }

    bool isInt() const {
        return is<int>();
    }

    bool isUInt() const {
        return is<unsigned>();
    }

    bool isInt64() const {
        return is<int64_t>();
    }

    bool isUInt64() const {
        return is<uint64_t>();
    }

    bool isDouble() const {
        return is<double>();
    }

    bool isNumber() const {
        auto t = type();
        return t >= Type::INT && t <= Type::DOUBLE;
    }

    bool isString() const {
        return is<String>();
    }

    bool isArray() const {
        return is<VariantArray>();
    }

    bool isMap() const {
        return is<VariantMap>();
    }

    template<typename T>
    bool is() const {
        static_assert(IsAlternativeType<T>::value, "The type specified is not one of the alternative types of Variant");
        return std::holds_alternative<T>(v_);
    }

    // Conversion methods
    bool toBool() const {
        return to<bool>();
    }

    bool toBool(bool& ok) const {
        return to<bool>(ok);
    }

    int toInt() const {
        return to<int>();
    }

    int toInt(bool& ok) const {
        return to<int>();
    }

    unsigned toUInt() const {
        return to<unsigned>();
    }

    unsigned toUInt(bool& ok) const {
        return to<unsigned>();
    }

    int64_t toInt64() const {
        return to<int64_t>();
    }

    int64_t toInt64(bool& ok) const {
        return to<int64_t>();
    }

    uint64_t toUInt64() const {
        return to<uint64_t>();
    }

    uint64_t toUInt64(bool& ok) const {
        return to<uint64_t>();
    }

    double toDouble() const {
        return to<double>();
    }

    double toDouble(bool& ok) const {
        return to<double>();
    }

    String toString() const {
        return to<String>();
    }

    String toString(bool& ok) const {
        return to<String>();
    }

    VariantArray toArray() const {
        return to<VariantArray>();
    }

    VariantArray toArray(bool& ok) const {
        return to<VariantArray>();
    }

    VariantMap toMap() const {
        return to<VariantMap>();
    }

    VariantMap toMap(bool& ok) const {
        return to<VariantMap>();
    }

    template<typename T>
    T to() const {
        return std::visit(ConvertToVisitor<T>(), v_);
    }

    template<typename T>
    T to(bool& ok) const {
        ConvertToVisitor<T> vis;
        T val = std::visit(vis, v_);
        ok = vis.ok;
        return val;
    }

    // Access to the underlying value
    bool& asBool() {
        return as<bool>();
    }

    int& asInt() {
        return as<int>();
    }

    unsigned& asUInt() {
        return as<unsigned>();
    }

    int64_t& asInt64() {
        return as<int64_t>();
    }

    uint64_t& asUInt64() {
        return as<uint64_t>();
    }

    double& asDouble() {
        return as<double>();
    }

    String& asString() {
        return as<String>();
    }

    VariantArray& asArray() {
        return as<VariantArray>();
    }

    VariantMap& asMap() {
        return as<VariantMap>();
    }

    template<typename T>
    T& as() {
        static_assert(IsAlternativeType<T>::value, "The type specified is not one of the alternative types of Variant");
        if (!is<T>()) {
            v_ = to<T>();
        }
        return value<T>();
    }

    template<typename T>
    T& value() {
        static_assert(IsAlternativeType<T>::value, "The type specified is not one of the alternative types of Variant");
        return std::get<T>(v_);
    }

    template<typename T>
    const T& value() const {
        static_assert(IsAlternativeType<T>::value, "The type specified is not one of the alternative types of Variant");
        return std::get<T>(v_);
    }

    // Array operations
    bool append(Variant val);
    bool prepend(Variant val);
    Variant at(int index) const;

    // Map operations
    bool set(const char* key, Variant val);
    bool set(const String& key, Variant val);
    bool set(String&& key, Variant val);
    Variant get(const char* key) const;
    Variant get(const String& key) const;
    bool has(const char* key) const;
    bool has(const String& key) const;

    int size() const;
    bool isEmpty() const;

    String toJSON() const;

    static Variant fromJSON(const char* json);
    static Variant fromJSON(const JSONValue& val);

    Variant& operator[](int index) {
        SPARK_ASSERT(isArray());
        return value<VariantArray>().at(index);
    }

    const Variant& operator[](int index) const {
        SPARK_ASSERT(isArray());
        return value<VariantArray>().at(index);
    }

    Variant& operator[](const char* key) {
        return asMap().operator[](key);
    }

    Variant& operator[](const String& key) {
        return asMap().operator[](key);
    }

    Variant& operator[](String&& key) {
        return asMap().operator[](std::move(key));
    }

    Variant& operator=(Variant var) {
        swap(*this, var);
        return *this;
    }

    bool operator==(const Variant& var) const {
        return std::visit(AreEqualVisitor(), v_, var.v_);
    }

    bool operator!=(const Variant& var) const {
        return !operator==(var);
    }

    template<typename T>
    bool operator==(const T& val) const {
        return std::visit(IsEqualToVisitor(val), v_);
    }

    template<typename T>
    bool operator!=(const T& val) const {
        return !operator==(val);
    }

    friend void swap(Variant& var1, Variant& var2) {
        using std::swap; // For ADL
        swap(var1.v_, var2.v_);
    }

private:
    typedef std::variant<std::monostate, bool, int, unsigned, int64_t, uint64_t, double, String, VariantArray, VariantMap> VariantType;

    template<typename TargetT, typename EnableT = void>
    struct ConvertToVisitor {
        bool ok = false;

        template<typename SourceT>
        TargetT operator()(const SourceT& val) {
            return TargetT();
        }
    };

    // Compares a Variant with a value
    template<typename FirstT>
    struct IsEqualToVisitor {
        const FirstT& first;

        explicit IsEqualToVisitor(const FirstT& first) :
                first(first) {
        }

        template<typename SecondT>
        bool operator()(const SecondT& second) const {
            return areEqual(first, second);
        }
    };

    // Compares two Variants
    struct AreEqualVisitor {
        template<typename FirstT, typename SecondT>
        bool operator()(const FirstT& first, const SecondT& second) const {
            return areEqual(first, second);
        }
    };

    template<typename T>
    struct IsAlternativeType {
        static constexpr bool value = false;
    };

    VariantType v_;

    template<typename FirstT, typename SecondT>
    static bool areEqual(const FirstT& first, const SecondT& second) {
        if constexpr (std::is_same_v<FirstT, SecondT> ||
                (std::is_arithmetic_v<FirstT> && std::is_arithmetic_v<SecondT>) ||
                (std::is_same_v<FirstT, String> && std::is_same_v<SecondT, const char*>) ||
                (std::is_same_v<FirstT, const char*> && std::is_same_v<SecondT, String>)) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
            return first == second;
#pragma GCC diagnostic pop
        } else {
            return false;
        }
    }

    template<typename ContainerT>
    static bool ensureCapacity(ContainerT& cont, int count) {
        int newSize = cont.size() + count;
        if (cont.capacity() >= newSize) {
            return true;
        }
        return cont.reserve(std::max(newSize, cont.capacity() * 3 / 2));
    }
};

namespace detail {

// As of GCC 10, std::to_chars and std::from_chars don't support floating point types. Note that the
// substitution functions below behave differently from the standard ones. They're tailored for the
// needs of the Variant class and are only defined so that we can quickly drop them when <charconv>
// API is fully supported by the current version of GCC
#if !defined(__cpp_lib_to_chars) || defined(UNIT_TEST)

std::to_chars_result to_chars(char* first, char* last, double value);
std::from_chars_result from_chars(const char* first, const char* last, double& value);

#endif // !defined(__cpp_lib_to_chars) || defined(UNIT_TEST)

template<typename T>
inline std::to_chars_result to_chars(char* first, char* last, const T& value) {
    return std::to_chars(first, last, value);
}

template<typename T>
inline std::from_chars_result from_chars(const char* first, const char* last, T& value) {
    return std::from_chars(first, last, value);
}

} // namespace detail

template<>
struct Variant::ConvertToVisitor<std::monostate> {
    bool ok = false;

    std::monostate operator()(const std::monostate&) {
        ok = true;
        return std::monostate();
    }

    template<typename SourceT>
    std::monostate operator()(const SourceT& val) {
        return std::monostate();
    }
};

template<>
class Variant::ConvertToVisitor<bool> {
public:
    bool ok = false;

    bool operator()(const String& val) {
        if (val == "true") {
            ok = true;
            return true;
        }
        if (val == "false") {
            ok = true;
            return false;
        }
        return false;
    }

    template<typename SourceT>
    bool operator()(const SourceT& val) {
        if constexpr (std::is_arithmetic_v<SourceT>) {
            ok = true;
            return static_cast<bool>(val);
        } else { // std::monostate, VariantArray, VariantMap
            return false;
        }
    }
};

template<typename TargetT>
struct Variant::ConvertToVisitor<TargetT, std::enable_if_t<std::is_arithmetic_v<TargetT>>> {
    bool ok = false;

    TargetT operator()(const String& val) {
        TargetT v;
        auto end = val.c_str() + val.length();
        auto r = detail::from_chars(val.c_str(), end, v);
        if (r.ec != std::errc() || r.ptr != end) {
            return TargetT();
        }
        ok = true;
        return v;
    }

    template<typename SourceT>
    TargetT operator()(const SourceT& val) {
        if constexpr (std::is_arithmetic_v<SourceT>) {
            ok = true;
            return static_cast<TargetT>(val);
        } else { // std::monostate, VariantArray, VariantMap
            return TargetT();
        }
    }
};

template<>
struct Variant::ConvertToVisitor<String> {
    bool ok = false;

    String operator()(bool val) {
        ok = true;
        return val ? "true" : "false";
    }

    String operator()(const String& val) {
        ok = true;
        return val;
    }

    template<typename SourceT>
    String operator()(const SourceT& val) {
        if constexpr (std::is_arithmetic_v<SourceT>) {
            char buf[32]; // Large enough for all relevant types
            auto r = detail::to_chars(buf, buf + sizeof(buf), val);
            SPARK_ASSERT(r.ec == std::errc());
            ok = true;
            return String(buf, r.ptr - buf);
        } else { // std::monostate, VariantArray, VariantMap
            return String();
        }
    }
};

template<>
struct Variant::ConvertToVisitor<VariantArray> {
    bool ok = false;

    VariantArray operator()(const VariantArray& val) {
        ok = true;
        return val;
    }

    template<typename SourceT>
    VariantArray operator()(const SourceT& val) {
        return VariantArray();
    }
};

template<>
struct Variant::ConvertToVisitor<VariantMap> {
    bool ok = false;

    VariantMap operator()(const VariantMap& val) {
        ok = true;
        return val;
    }

    template<typename SourceT>
    VariantMap operator()(const SourceT& val) const {
        return VariantMap();
    }
};

template<>
struct Variant::IsAlternativeType<std::monostate> {
    static const bool value = true;
};

template<>
struct Variant::IsAlternativeType<bool> {
    static const bool value = true;
};

template<>
struct Variant::IsAlternativeType<int> {
    static const bool value = true;
};

template<>
struct Variant::IsAlternativeType<unsigned> {
    static const bool value = true;
};

template<>
struct Variant::IsAlternativeType<int64_t> {
    static const bool value = true;
};

template<>
struct Variant::IsAlternativeType<uint64_t> {
    static const bool value = true;
};

template<>
struct Variant::IsAlternativeType<double> {
    static const bool value = true;
};

template<>
struct Variant::IsAlternativeType<String> {
    static const bool value = true;
};

template<>
struct Variant::IsAlternativeType<VariantArray> {
    static const bool value = true;
};

template<>
struct Variant::IsAlternativeType<VariantMap> {
    static const bool value = true;
};

template<typename T>
inline bool operator==(const T& val, const Variant& var) {
    return var == val;
}

template<typename T>
inline bool operator!=(const T& val, const Variant& var) {
    return var != val;
}

} // namespace particle
