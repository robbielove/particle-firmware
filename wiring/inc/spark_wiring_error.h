/*
 * Copyright (c) 2016 Particle Industries, Inc.  All rights reserved.
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

#ifndef SPARK_WIRING_ERROR_H
#define SPARK_WIRING_ERROR_H

#include "system_error.h"

#include <utility>
#include <cstring>
#include <cstdlib>

namespace spark {

class Error {
public:
    // Error type
    enum Type {
        // NONE = 0,
        // UNKNOWN = 100,
        // ...
        SYSTEM_ERROR_ENUM_VALUES()
    };

    Error(Type type = UNKNOWN);
    Error(Type type, const char* msg);
    Error(const Error& error);
    Error(Error&& error);
    ~Error();

    Type type() const;
    const char* message() const;

    bool operator==(const Error& error) const;
    bool operator==(Type type) const;

    Error& operator=(Error error);

private:
    const char* msg_;
    Type type_;

    friend void swap(Error& error1, Error& error2);
};

void swap(Error& error1, Error& error2);

} // namespace spark

inline spark::Error::Error(Type type) :
        msg_(nullptr),
        type_(type) {
}

inline spark::Error::Error(Type type, const char* msg) :
        msg_(msg ? (const char*)strdup(msg) : nullptr),
        type_(type) {
}

inline spark::Error::Error(const Error& error) :
        Error(error.type_, error.msg_) {
}

inline spark::Error::Error(Error&& error) :
        Error() {
    swap(*this, error);
}

inline spark::Error::~Error() {
    free((void*)msg_);
}

inline spark::Error::Type spark::Error::type() const {
    return type_;
}

inline const char* spark::Error::message() const {
    return (msg_ ? msg_ : system_error_message((system_error)type_, nullptr));
}

inline bool spark::Error::operator==(const Error& error) const {
    return (type_ == error.type_);
}

inline bool spark::Error::operator==(Type type) const {
    return (type_ == type);
}

inline spark::Error& spark::Error::operator=(Error error) {
    swap(*this, error);
    return *this;
}

inline void spark::swap(Error& error1, Error& error2) {
    using std::swap;
    swap(error1.type_, error2.type_);
    swap(error1.msg_, error2.msg_);
}

#endif // SPARK_WIRING_ERROR_H
