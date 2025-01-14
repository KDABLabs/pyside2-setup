/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt for Python.
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

#include "apiextractor.h"
#include "apiextractorresult.h"
#include "abstractmetalang.h"

#include <QDir>
#include <QDebug>
#include <QTemporaryFile>
#include <algorithm>
#include <iostream>
#include <iterator>

#include "reporthandler.h"
#include "typesystem.h"
#include "fileout.h"
#include "abstractmetabuilder.h"
#include "abstractmetaenum.h"
#include "typedatabase.h"
#include "typesystem.h"

#include <algorithm>
#include <iterator>

ApiExtractor::ApiExtractor()
{
    // Environment TYPESYSTEMPATH
    QString envTypesystemPaths = QFile::decodeName(qgetenv("TYPESYSTEMPATH"));
    if (!envTypesystemPaths.isEmpty())
        TypeDatabase::instance()->addTypesystemPath(envTypesystemPaths);
}

ApiExtractor::~ApiExtractor()
{
    delete m_builder;
}

void ApiExtractor::addTypesystemSearchPath (const QString& path)
{
    TypeDatabase::instance()->addTypesystemPath(path);
}

void ApiExtractor::addTypesystemSearchPath(const QStringList& paths)
{
    for (const QString &path : paths)
        addTypesystemSearchPath(path);
}

void ApiExtractor::setTypesystemKeywords(const QStringList &keywords)
{
    TypeDatabase::instance()->setTypesystemKeywords(keywords);
}

void ApiExtractor::addIncludePath(const HeaderPath& path)
{
    m_includePaths << path;
}

void ApiExtractor::addIncludePath(const HeaderPaths& paths)
{
    m_includePaths << paths;
}

void ApiExtractor::setLogDirectory(const QString& logDir)
{
    m_logDirectory = logDir;
}

void ApiExtractor::setCppFileNames(const QFileInfoList &cppFileName)
{
    m_cppFileNames = cppFileName;
}

void ApiExtractor::setTypeSystem(const QString& typeSystemFileName)
{
    m_typeSystemFileName = typeSystemFileName;
}

void ApiExtractor::setSkipDeprecated(bool value)
{
    m_skipDeprecated = value;
    if (m_builder)
        m_builder->setSkipDeprecated(m_skipDeprecated);
}

void ApiExtractor::setSuppressWarnings ( bool value )
{
    TypeDatabase::instance()->setSuppressWarnings(value);
}

void ApiExtractor::setSilent ( bool value )
{
    ReportHandler::setSilent(value);
}

bool ApiExtractor::setApiVersion(const QString& package, const QString &version)
{
    return TypeDatabase::setApiVersion(package, version);
}

void ApiExtractor::setDropTypeEntries(const QStringList &dropEntries)
{
    TypeDatabase::instance()->setDropTypeEntries(dropEntries);
}

const AbstractMetaEnumList &ApiExtractor::globalEnums() const
{
    Q_ASSERT(m_builder);
    return m_builder->globalEnums();
}

const AbstractMetaFunctionCList &ApiExtractor::globalFunctions() const
{
    Q_ASSERT(m_builder);
    return m_builder->globalFunctions();
}

const AbstractMetaClassList &ApiExtractor::classes() const
{
    Q_ASSERT(m_builder);
    return m_builder->classes();
}

const AbstractMetaClassList &ApiExtractor::smartPointers() const
{
    Q_ASSERT(m_builder);
    return m_builder->smartPointers();
}

// Add defines required for parsing Qt code headers
static void addPySideExtensions(QByteArrayList *a)
{
    // Make "signals:", "slots:" visible as access specifiers
    a->append(QByteArrayLiteral("-DQT_ANNOTATE_ACCESS_SPECIFIER(a)=__attribute__((annotate(#a)))"));

    // Q_PROPERTY is defined as class annotation which does not work since a
    // sequence of properties will to expand to a sequence of annotations
    // annotating nothing, causing clang to complain. Instead, define it away in a
    // static assert with the stringified argument in a ','-operator (cf qdoc).
    a->append(QByteArrayLiteral("-DQT_ANNOTATE_CLASS(type,...)=static_assert(sizeof(#__VA_ARGS__),#type);"));

    // With Qt6, qsimd.h became public header and was included in <QtCore>. That
    // introduced a conflict with libclang headers on macOS. To be able to include
    // <QtCore>, we prevent its inclusion by adding its include guard.
    a->append(QByteArrayLiteral("-DQSIMD_H"));
}

bool ApiExtractor::runHelper(bool usePySideExtensions)
{
    if (m_builder)
        return false;

    if (!TypeDatabase::instance()->parseFile(m_typeSystemFileName)) {
        std::cerr << "Cannot parse file: " << qPrintable(m_typeSystemFileName);
        return false;
    }

    const QString pattern = QDir::tempPath() + QLatin1Char('/')
        + m_cppFileNames.constFirst().baseName()
        + QStringLiteral("_XXXXXX.hpp");
    QTemporaryFile ppFile(pattern);
    bool autoRemove = !qEnvironmentVariableIsSet("KEEP_TEMP_FILES");
    // make sure that a tempfile can be written
    if (!ppFile.open()) {
        std::cerr << "could not create tempfile " << qPrintable(pattern)
            << ": " << qPrintable(ppFile.errorString()) << '\n';
        return false;
    }
    for (const auto &cppFileName : qAsConst(m_cppFileNames)) {
        ppFile.write("#include \"");
        ppFile.write(cppFileName.absoluteFilePath().toLocal8Bit());
        ppFile.write("\"\n");
    }
    const QString preprocessedCppFileName = ppFile.fileName();
    ppFile.close();
    m_builder = new AbstractMetaBuilder;
    m_builder->setLogDirectory(m_logDirectory);
    m_builder->setGlobalHeaders(m_cppFileNames);
    m_builder->setSkipDeprecated(m_skipDeprecated);
    m_builder->setHeaderPaths(m_includePaths);

    QByteArrayList arguments;
    const auto clangOptionsSize = m_clangOptions.size();
    arguments.reserve(m_includePaths.size() + clangOptionsSize + 1);

    bool addCompilerSupportArguments = true;
    if (clangOptionsSize > 0) {
        qsizetype i = 0;
        if (m_clangOptions.at(i) == u"-") {
            ++i;
            addCompilerSupportArguments = false; // No built-in options
        }
        for (; i < clangOptionsSize; ++i)
            arguments.append(m_clangOptions.at(i).toUtf8());
    }

    for (const HeaderPath &headerPath : qAsConst(m_includePaths))
        arguments.append(HeaderPath::includeOption(headerPath));
    arguments.append(QFile::encodeName(preprocessedCppFileName));
    if (ReportHandler::isDebug(ReportHandler::SparseDebug)) {
        qCInfo(lcShiboken).noquote().nospace()
            << "clang language level: " << int(m_languageLevel)
            << "\nclang arguments: " << arguments;
    }

    if (usePySideExtensions)
        addPySideExtensions(&arguments);

    const bool result = m_builder->build(arguments, addCompilerSupportArguments, m_languageLevel);
    if (!result)
        autoRemove = false;
    if (!autoRemove) {
        ppFile.setAutoRemove(false);
        std::cerr << "Keeping temporary file: " << qPrintable(QDir::toNativeSeparators(preprocessedCppFileName)) << '\n';
    }
    return result;
}

static inline void classListToCList(const AbstractMetaClassList &list, AbstractMetaClassCList *target)
{
    target->reserve(list.size());
    std::copy(list.cbegin(), list.cend(), std::back_inserter(*target));
}

std::optional<ApiExtractorResult> ApiExtractor::run(bool usePySideExtensions)
{
    if (!runHelper(usePySideExtensions))
        return {};
    ApiExtractorResult result;
    classListToCList(m_builder->classes(), &result.m_metaClasses);
    classListToCList(m_builder->smartPointers(), &result.m_smartPointers);
    result.m_globalFunctions = m_builder->globalFunctions();
    result.m_globalEnums = m_builder->globalEnums();
    result.m_enums = m_builder->typeEntryToEnumsHash();
    result.m_typeSystem = m_typeSystemFileName;
    return result;
}

LanguageLevel ApiExtractor::languageLevel() const
{
    return m_languageLevel;
}

void ApiExtractor::setLanguageLevel(LanguageLevel languageLevel)
{
    m_languageLevel = languageLevel;
}

QStringList ApiExtractor::clangOptions() const
{
    return m_clangOptions;
}

void ApiExtractor::setClangOptions(const QStringList &co)
{
    m_clangOptions = co;
}

void ApiExtractor::setUseGlobalHeader(bool h)
{
    AbstractMetaBuilder::setUseGlobalHeader(h);
}

#ifndef QT_NO_DEBUG_STREAM
template <class Container>
static void debugFormatSequence(QDebug &d, const char *key, const Container& c)
{
    if (c.isEmpty())
        return;
    const auto begin = c.begin();
    d << "\n  " << key << '[' << c.size() << "]=(";
    for (auto it = begin, end = c.end(); it != end; ++it) {
        if (it != begin)
            d << ", ";
        d << *it;
    }
    d << ')';
}

QDebug operator<<(QDebug d, const ApiExtractor &ae)
{
    QDebugStateSaver saver(d);
    d.noquote();
    d.nospace();
    if (ReportHandler::debugLevel() >= ReportHandler::FullDebug)
        d.setVerbosity(3); // Trigger verbose output of AbstractMetaClass
    d << "ApiExtractor(typeSystem=\"" << ae.typeSystem() << "\", cppFileNames=\""
      << ae.cppFileNames() << ", ";
    ae.m_builder->formatDebug(d);
    d << ')';
    return d;
}
#endif // QT_NO_DEBUG_STREAM
