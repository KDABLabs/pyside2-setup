/*
 * This file is part of the Shiboken Python Binding Generator project.
 *
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Contact: PySide team <contact@pyside.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation. Please
 * review the following information to ensure the GNU Lesser General
 * Public License version 2.1 requirements will be met:
 * http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 *
 * As a special exception to the GNU Lesser General Public License
 * version 2.1, the object code form of a "work that uses the Library"
 * may incorporate material from a header file that is part of the
 * Library.  You may distribute such object code under terms of your
 * choice, provided that the incorporated material (i) does not exceed
 * more than 5% of the total size of the Library; and (ii) is limited to
 * numerical parameters, data structure layouts, accessors, macros,
 * inline functions and templates.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <iostream>
#include "mapuser.h"

using namespace std;

std::map<const char*, std::pair<Complex, int> >
MapUser::callCreateMap()
{
    return createMap();
}


std::map<const char*, std::pair<Complex, int> >
MapUser::createMap()
{
    std::map<const char*, std::pair<Complex, int> > retval;

    std::pair<const char *, std::pair<Complex, int> >
            item0("zero", std::pair<Complex, int>(Complex(1.2, 3.4), 2));
    retval.insert(item0);

    std::pair<const char *, std::pair<Complex, int> >
            item1("one", std::pair<Complex, int>(Complex(5.6, 7.8), 3));
    retval.insert(item1);

    std::pair<const char *, std::pair<Complex, int> >
            item2("two", std::pair<Complex, int>(Complex(9.1, 2.3), 5));
    retval.insert(item2);

    return retval;
}

void
MapUser::showMap(std::map<const char*, int> mapping)
{
    std::map<const char*, int>::iterator it;
    cout << __FUNCTION__ << endl;
    for (it = mapping.begin() ; it != mapping.end(); it++)
        cout << (*it).first << " => " << (*it).second << endl;
}

