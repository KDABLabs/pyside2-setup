/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of Qt for Python.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef SMART_INTEGER_H
#define SMART_INTEGER_H

#include "libsmartmacros.h"

class LIB_SMART_API Integer {
public:
    Integer();
    Integer(const Integer &other);
    Integer &operator=(const Integer &other);
    ~Integer();
    void printInteger() const;

    int value() const;
    void setValue(int v);

    int compare(const Integer &rhs) const;

    int m_int; // public for testing member field access.
};

inline bool operator==(const Integer &lhs, const Integer &rhs)
{
    return lhs.compare(rhs) == 0;
}

inline bool operator!=(const Integer &lhs, const Integer &rhs)
{
    return lhs.compare(rhs) != 0;
}

inline bool operator<(const Integer &lhs, const Integer &rhs)
{
    return lhs.compare(rhs) < 0;
}

inline bool operator<=(const Integer &lhs, const Integer &rhs)
{
    return lhs.compare(rhs) <= 0;
}

inline bool operator>(const Integer &lhs, const Integer &rhs)
{
    return lhs.compare(rhs) > 0;
}

inline bool operator>=(const Integer &lhs, const Integer &rhs)
{
    return lhs.compare(rhs) >= 0;
}

namespace Smart {
class LIB_SMART_API Integer2 : public Integer {
public:
    Integer2();
    Integer2(const Integer2 &other);
};
} // namespace Smart

#endif // SMART_INTEGER_H
