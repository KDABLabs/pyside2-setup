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
#include "derived.h"

using namespace std;

Derived::Derived(int id) : Abstract(id)
{
    //cout << __PRETTY_FUNCTION__;
    //show();
    //cout << endl;
}

Derived::~Derived()
{
    //cout << __PRETTY_FUNCTION__;
    //show();
    //cout << endl;
}

Abstract*
Derived::createObject()
{
    static int id = 100;
    return new Derived(id++);
}

void
Derived::pureVirtual()
{
    //cout << __PRETTY_FUNCTION__ << endl;
}

void
Derived::unpureVirtual()
{
    //cout << __PRETTY_FUNCTION__ << endl;
}

bool
Derived::singleArgument(bool b)
{
    //cout << __PRETTY_FUNCTION__ << endl;
    return !b;
}

double
Derived::defaultValue(int n)
{
    //cout << __PRETTY_FUNCTION__ << "[n = 0]" << endl;
    return ((double) n) + 0.1;
}

PolymorphicFuncEnum
Derived::polymorphic(int i, int d)
{
    //cout << __PRETTY_FUNCTION__ << "[i = 0, d = 0]" << endl;
    return PolymorphicFunc_ii;
}

PolymorphicFuncEnum
Derived::polymorphic(double n)
{
    //cout << __PRETTY_FUNCTION__ << endl;
    return PolymorphicFunc_d;
}

Derived::OtherPolymorphicFuncEnum
Derived::otherPolymorphic(int a, int b, bool c, double d)
{
    //cout << __PRETTY_FUNCTION__ << endl;
    return OtherPolymorphicFunc_iibd;
}

Derived::OtherPolymorphicFuncEnum
Derived::otherPolymorphic(int a, double b)
{
    //cout << __PRETTY_FUNCTION__ << endl;
    return OtherPolymorphicFunc_id;
}

