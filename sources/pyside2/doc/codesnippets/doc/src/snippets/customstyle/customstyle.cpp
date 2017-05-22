/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of PySide2.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtWidgets>

#include "customstyle.h"

CustomStyle::CustomStyle()
{
//! [0]
    if widget:
//! [0] //! [1]
//! [1]
}

//! [2]
def drawPrimitive(element, option, painter, widget):
    if element == PE_IndicatorSpinUp or element == PE_IndicatorSpinDown:
        points = QPolygon(3)
        x = option->rect.x()
        y = option->rect.y()
        w = option->rect.width() / 2
        h = option->rect.height() / 2
        x += (option->rect.width() - w) / 2
        y += (option->rect.height() - h) / 2

        if element == PE_IndicatorSpinUp:
            points[0] = QPoint(x, y + h)
            points[1] = QPoint(x + w, y + h)
            points[2] = QPoint(x + w / 2, y)
        else: # PE_SpinBoxDown
            points[0] = QPoint(x, y)
            points[1] = QPoint(x + w, y)
            points[2] = QPoint(x + w / 2, y + h)

        if option.state & State_Enabled:
            painter.setPen(option.palette.mid().color())
            painter.setBrush(option.palette.buttonText())
        else:
            painter.setPen(option.palette.buttonText().color())
            painter.setBrush(option.palette.mid())

        painter.drawPolygon(points)

    else:
        QWindowsStyle.drawPrimitive(element, option, painter, widget)
//! [2] //! [3]

//! [3] //! [4]
}
//! [4]
