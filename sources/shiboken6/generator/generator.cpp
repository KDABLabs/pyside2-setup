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

#include "generator.h"
#include "apiextractorresult.h"
#include "ctypenames.h"
#include "abstractmetaenum.h"
#include "abstractmetafield.h"
#include "abstractmetafunction.h"
#include "abstractmetalang.h"
#include "parser/codemodel.h"
#include "messages.h"
#include "reporthandler.h"
#include "fileout.h"
#include "apiextractor.h"
#include "typesystem.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QRegularExpression>
#include <QDebug>
#include <typedatabase.h>

static const char ENABLE_PYSIDE_EXTENSIONS[] = "enable-pyside-extensions";

/**
 * DefaultValue is used for storing default values of types for which code is
 * generated in different contexts:
 *
 * Context             | Example: "Class *"            | Example: "Class" with default Constructor
 * --------------------+-------------------------------+------------------------------------------
 * Variable            |  var{nullptr};                | var;
 * initializations     |                               |
 * --------------------+-------------------------------+------------------------------------------
 * Return values       | return nullptr;               | return {}
 * --------------------+-------------------------------+------------------------------------------
 * constructor         | static_cast<Class *>(nullptr) | Class()
 * arguments lists     |                               |
 * (recursive, precise |                               |
 * matching).          |                               |
 */

DefaultValue::DefaultValue(Type t, QString value) :
    m_type(t), m_value(std::move(value))
{
}

DefaultValue::DefaultValue(QString customValue) :
    m_type(Custom), m_value(std::move(customValue))
{
}

QString DefaultValue::returnValue() const
{
    switch (m_type) {
    case DefaultValue::Boolean:
        return QLatin1String("false");
    case DefaultValue::CppScalar:
        return QLatin1String("0");
    case DefaultValue::Custom:
    case DefaultValue::Enum:
        return m_value;
    case DefaultValue::Pointer:
        return QLatin1String("nullptr");
    case DefaultValue::Void:
        return QString();
    case DefaultValue::DefaultConstructorWithDefaultValues:
        return m_value + QLatin1String("()");
    case DefaultValue::DefaultConstructor:
        break;
    }
    return QLatin1String("{}");
}

QString DefaultValue::initialization() const
{
    switch (m_type) {
    case DefaultValue::Boolean:
        return QLatin1String("{false}");
    case DefaultValue::CppScalar:
        return QLatin1String("{0}");
    case DefaultValue::Custom:
        return QLatin1String(" = ") + m_value;
    case DefaultValue::Enum:
        return QLatin1Char('{') + m_value + QLatin1Char('}');
    case DefaultValue::Pointer:
        return QLatin1String("{nullptr}");
    case DefaultValue::Void:
        Q_ASSERT(false);
        break;
    case DefaultValue::DefaultConstructor:
    case DefaultValue::DefaultConstructorWithDefaultValues:
        break;
    }
    return QString();
}

QString DefaultValue::constructorParameter() const
{
    switch (m_type) {
    case DefaultValue::Boolean:
        return QLatin1String("false");
    case DefaultValue::CppScalar: {
        // PYSIDE-846: Use static_cast in case of "unsigned long" and similar
        const QString cast = m_value.contains(QLatin1Char(' '))
            ? QLatin1String("static_cast<") + m_value + QLatin1Char('>')
            : m_value;
        return cast + QLatin1String("(0)");
    }
    case DefaultValue::Custom:
    case DefaultValue::Enum:
        return m_value;
    case DefaultValue::Pointer:
        // Be precise here to be able to differentiate between constructors
        // taking different pointer types, cf
        // QTreeWidgetItemIterator(QTreeWidget *) and
        // QTreeWidgetItemIterator(QTreeWidgetItemIterator *).
        return QLatin1String("static_cast<") + m_value + QLatin1String("*>(nullptr)");
    case DefaultValue::Void:
        Q_ASSERT(false);
        break;
    case DefaultValue::DefaultConstructor:
    case DefaultValue::DefaultConstructorWithDefaultValues:
        break;
    }
    return m_value + QLatin1String("()");
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug debug, const DefaultValue &v)
{
    QDebugStateSaver saver(debug);
    debug.noquote();
    debug.nospace();
    debug << "DefaultValue(" <<  v.type() << ", \"" << v.value() << "\")";
    return debug;
}
#endif // !QT_NO_DEBUG_STREAM

QString GeneratorContext::smartPointerWrapperName() const
{
    Q_ASSERT(m_type == SmartPointer);
    return m_preciseClassType.cppSignature();
}

struct Generator::GeneratorPrivate
{
    ApiExtractorResult api;
    QString outDir;
    // License comment
    QString licenseComment;
    QStringList instantiatedContainersNames;
    AbstractMetaTypeList instantiatedContainers;
    AbstractMetaTypeList instantiatedSmartPointers;
    AbstractMetaClassCList m_invisibleTopNamespaces;
    bool m_hasPrivateClasses = false;
    bool m_usePySideExtensions = false;
};

Generator::Generator() : m_d(new GeneratorPrivate)
{
}

Generator::~Generator()
{
    delete m_d;
}

bool Generator::setup(const ApiExtractorResult &api)
{
    m_d->api = api;
    const auto moduleEntry = TypeDatabase::instance()->defaultTypeSystemType();
    if (!moduleEntry) {
        qCWarning(lcShiboken,"Couldn't find the package name!!");
        return false;
    }
    if (!moduleEntry->generateCode()) {
        qCWarning(lcShiboken, "Code generation of root typesystem is disabled!!");
        return false;
    }

    collectInstantiatedContainersAndSmartPointers();

    for (auto c : api.classes()) {
        if (c->enclosingClass() == nullptr && c->isInvisibleNamespace()) {
            m_d->m_invisibleTopNamespaces.append(c);
            c->invisibleNamespaceRecursion([&](AbstractMetaClass *ic) {
                m_d->m_invisibleTopNamespaces.append(ic);
            });
        }
    }

    return doSetup();
}

QString Generator::getSimplifiedContainerTypeName(const AbstractMetaType &type)
{
    const QString signature = type.cppSignature();
    if (!type.typeEntry()->isContainer() && !type.typeEntry()->isSmartPointer())
        return signature;
    QString typeName = signature;
    if (type.isConstant())
        typeName.remove(0, sizeof("const ") / sizeof(char) - 1);
    switch (type.referenceType()) {
    case NoReference:
        break;
    case LValueReference:
        typeName.chop(1);
        break;
    case RValueReference:
        typeName.chop(2);
        break;
    }
    while (typeName.endsWith(QLatin1Char('*')) || typeName.endsWith(QLatin1Char(' ')))
        typeName.chop(1);
    return typeName;
}

// Strip a "const QSharedPtr<const Foo> &" or similar to "QSharedPtr<Foo>" (PYSIDE-1016/454)
AbstractMetaType canonicalSmartPtrInstantiation(const AbstractMetaType &type)
{
    const AbstractMetaTypeList &instantiations = type.instantiations();
    Q_ASSERT(instantiations.size() == 1);
    const bool needsFix = type.isConstant() || type.referenceType() != NoReference;
    const bool pointeeNeedsFix = instantiations.constFirst().isConstant();
    if (!needsFix && !pointeeNeedsFix)
        return type;
    auto fixedType = type;
    fixedType.setReferenceType(NoReference);
    fixedType.setConstant(false);
    if (pointeeNeedsFix) {
        auto fixedPointeeType = instantiations.constFirst();
        fixedPointeeType.setConstant(false);
        fixedType.setInstantiations(AbstractMetaTypeList(1, fixedPointeeType));
    }
    return fixedType;
}

static inline const TypeEntry *pointeeTypeEntry(const AbstractMetaType &smartPtrType)
{
    return smartPtrType.instantiations().constFirst().typeEntry();
}

void Generator::addInstantiatedContainersAndSmartPointers(const AbstractMetaType &type,
                                                          const QString &context)
{
    for (const auto &t : type.instantiations())
        addInstantiatedContainersAndSmartPointers(t, context);
    const auto typeEntry = type.typeEntry();
    const bool isContainer = typeEntry->isContainer();
    if (!isContainer
        && !(typeEntry->isSmartPointer() && typeEntry->generateCode())) {
        return;
    }
    if (type.hasTemplateChildren()) {
        QString piece = isContainer ? QStringLiteral("container") : QStringLiteral("smart pointer");
        QString warning =
                QString::fromLatin1("Skipping instantiation of %1 '%2' because it has template"
                               " arguments.").arg(piece, type.originalTypeDescription());
        if (!context.isEmpty())
            warning.append(QStringLiteral(" Calling context: ") + context);

        qCWarning(lcShiboken).noquote().nospace() << warning;
        return;

    }
    if (isContainer) {
        const QString typeName = getSimplifiedContainerTypeName(type);
        if (!m_d->instantiatedContainersNames.contains(typeName)) {
            m_d->instantiatedContainersNames.append(typeName);
            auto simplifiedType = type;
            simplifiedType.setIndirections(0);
            simplifiedType.setConstant(false);
            simplifiedType.setReferenceType(NoReference);
            simplifiedType.decideUsagePattern();
            m_d->instantiatedContainers.append(simplifiedType);
        }
        return;
    }

    // Is smart pointer. Check if the (const?) pointee is already known for the given
    // smart pointer type entry.
    auto pt = pointeeTypeEntry(type);
    const bool present =
        std::any_of(m_d->instantiatedSmartPointers.cbegin(), m_d->instantiatedSmartPointers.cend(),
                    [typeEntry, pt] (const AbstractMetaType &t) {
                        return t.typeEntry() == typeEntry && pointeeTypeEntry(t) == pt;
                    });
    if (!present)
        m_d->instantiatedSmartPointers.append(canonicalSmartPtrInstantiation(type));
}

void Generator::collectInstantiatedContainersAndSmartPointers(const AbstractMetaFunctionCPtr &func)
{
    addInstantiatedContainersAndSmartPointers(func->type(), func->signature());
    const AbstractMetaArgumentList &arguments = func->arguments();
    for (const AbstractMetaArgument &arg : arguments)
        addInstantiatedContainersAndSmartPointers(arg.type(), func->signature());
}

void Generator::collectInstantiatedContainersAndSmartPointers(const AbstractMetaClass *metaClass)
{
    if (!metaClass->typeEntry()->generateCode())
        return;
    for (const auto &func : metaClass->functions())
        collectInstantiatedContainersAndSmartPointers(func);
    for (const AbstractMetaField &field : metaClass->fields())
        addInstantiatedContainersAndSmartPointers(field.type(), field.name());
    const AbstractMetaClassList &innerClasses = metaClass->innerClasses();
    for (AbstractMetaClass *innerClass : innerClasses)
        collectInstantiatedContainersAndSmartPointers(innerClass);
}

void Generator::collectInstantiatedContainersAndSmartPointers()
{
    for (const auto &func : m_d->api.globalFunctions())
        collectInstantiatedContainersAndSmartPointers(func);
    for (auto metaClass : m_d->api.classes())
        collectInstantiatedContainersAndSmartPointers(metaClass);
}

AbstractMetaTypeList Generator::instantiatedContainers() const
{
    return m_d->instantiatedContainers;
}

AbstractMetaTypeList Generator::instantiatedSmartPointers() const
{
    return m_d->instantiatedSmartPointers;
}

Generator::OptionDescriptions Generator::options() const
{
    return {
        {QLatin1String(ENABLE_PYSIDE_EXTENSIONS),
         u"Enable PySide extensions, such as support for signal/slots,\n"
          "use this if you are creating a binding for a Qt-based library."_qs}
    };
}

bool Generator::handleOption(const QString & key, const QString & /* value */)
{
    if (key == QLatin1String(ENABLE_PYSIDE_EXTENSIONS))
        return ( m_d->m_usePySideExtensions = true);
    return false;
}

const AbstractMetaClassCList &Generator::invisibleTopNamespaces() const
{
    return m_d->m_invisibleTopNamespaces;
}

PrimitiveTypeEntryList Generator::primitiveTypes()
{
    return TypeDatabase::instance()->primitiveTypes();
}

ContainerTypeEntryList Generator::containerTypes()
{
    return TypeDatabase::instance()->containerTypes();
}

QString Generator::licenseComment() const
{
    return m_d->licenseComment;
}

void Generator::setLicenseComment(const QString &licenseComment)
{
    m_d->licenseComment = licenseComment;
}

QString Generator::packageName()
{
    return TypeDatabase::instance()->defaultPackageName();
}

static QString getModuleName()
{
    QString result = TypeDatabase::instance()->defaultPackageName();
    result.remove(0, result.lastIndexOf(QLatin1Char('.')) + 1);
    return result;
}

QString Generator::moduleName()
{
    static const QString result = getModuleName();
    return result;
}

QString Generator::outputDirectory() const
{
    return m_d->outDir;
}

void Generator::setOutputDirectory(const QString &outDir)
{
    m_d->outDir = outDir;
}

bool Generator::generateFileForContext(const GeneratorContext &context)
{
    const AbstractMetaClass *cls = context.metaClass();

    if (!shouldGenerate(cls))
        return true;

    const QString fileName = fileNameForContext(context);
    if (fileName.isEmpty())
        return true;

    QString filePath = outputDirectory() + QLatin1Char('/') + subDirectoryForClass(cls)
            + QLatin1Char('/') + fileName;
    FileOut fileOut(filePath);

    generateClass(fileOut.stream, context);

    fileOut.done();
    return true;
}

QString Generator::getFileNameBaseForSmartPointer(const AbstractMetaType &smartPointerType,
                                                  const AbstractMetaClass *smartPointerClass)
{
    const AbstractMetaType innerType = smartPointerType.getSmartPointerInnerType();
    QString fileName = smartPointerClass->qualifiedCppName().toLower();
    fileName.replace(QLatin1String("::"), QLatin1String("_"));
    fileName.append(QLatin1String("_"));
    fileName.append(innerType.name().toLower());

    return fileName;
}

GeneratorContext Generator::contextForClass(const AbstractMetaClass *c) const
{
    GeneratorContext result;
    result.m_metaClass = c;
    return result;
}

GeneratorContext Generator::contextForSmartPointer(const AbstractMetaClass *c,
                                                   const AbstractMetaType &t,
                                                   const AbstractMetaClass *pointeeClass)
{
    GeneratorContext result;
    result.m_metaClass = c;
    result.m_preciseClassType = t;
    result.m_type = GeneratorContext::SmartPointer;
    result.m_pointeeClass = pointeeClass;
    return result;
}

bool Generator::generate()
{
    for (auto cls : m_d->api.classes()) {
        if (!generateFileForContext(contextForClass(cls)))
            return false;
        if (shouldGenerate(cls) && cls->typeEntry()->isPrivate())
            m_d->m_hasPrivateClasses = true;
    }

    const auto smartPointers = m_d->api.smartPointers();
    for (const AbstractMetaType &type : qAsConst(m_d->instantiatedSmartPointers)) {
        const AbstractMetaClass *smartPointerClass =
            AbstractMetaClass::findClass(smartPointers, type.typeEntry());
        if (!smartPointerClass) {
            qCWarning(lcShiboken, "%s",
                      qPrintable(msgCannotFindSmartPointer(type.cppSignature(),
                                                           smartPointers)));
            return false;
        }
        const AbstractMetaClass *pointeeClass = nullptr;
        const auto *instantiatedType = type.instantiations().constFirst().typeEntry();
        if (instantiatedType->isComplex()) // not a C++ primitive
            pointeeClass = AbstractMetaClass::findClass(m_d->api.classes(), instantiatedType);
        if (!generateFileForContext(contextForSmartPointer(smartPointerClass, type,
                                                           pointeeClass))) {
            return false;
        }
    }
    return finishGeneration();
}

bool Generator::shouldGenerateTypeEntry(const TypeEntry *type)
{
    return type->generateCode() && NamespaceTypeEntry::isVisibleScope(type);
}

bool Generator::shouldGenerate(const AbstractMetaClass *metaClass) const
{
    return shouldGenerateTypeEntry(metaClass->typeEntry());
}

const ApiExtractorResult &Generator::api() const
{
    return m_d->api;
}

bool Generator::hasPrivateClasses() const
{
    return m_d->m_hasPrivateClasses;
}

bool Generator::usePySideExtensions() const
{
    return m_d->m_usePySideExtensions;
}

QString Generator::getFullTypeName(const TypeEntry *type)
{
    QString result = type->qualifiedCppName();
    if (type->isArray())
        type = static_cast<const ArrayTypeEntry *>(type)->nestedTypeEntry();
    if (!type->isCppPrimitive())
        result.prepend(QLatin1String("::"));
    return result;
}

QString Generator::getFullTypeName(const AbstractMetaType &type)
{
    if (type.isCString())
        return QLatin1String("const char*");
    if (type.isVoidPointer())
        return QLatin1String("void*");
    if (type.typeEntry()->isContainer())
        return QLatin1String("::") + type.cppSignature();
    QString typeName;
    if (type.typeEntry()->isComplex() && type.hasInstantiations())
        typeName = getFullTypeNameWithoutModifiers(type);
    else
        typeName = getFullTypeName(type.typeEntry());
    return typeName + QString::fromLatin1("*").repeated(type.indirections());
}

QString Generator::getFullTypeName(const AbstractMetaClass *metaClass)
{
    return QLatin1String("::") + metaClass->qualifiedCppName();
}

QString Generator::getFullTypeNameWithoutModifiers(const AbstractMetaType &type)
{
    if (type.isCString())
        return QLatin1String("const char*");
    if (type.isVoidPointer())
        return QLatin1String("void*");
    if (!type.hasInstantiations())
        return getFullTypeName(type.typeEntry());
    QString typeName = type.cppSignature();
    if (type.isConstant())
        typeName.remove(0, sizeof("const ") / sizeof(char) - 1);
    switch (type.referenceType()) {
    case NoReference:
        break;
    case LValueReference:
        typeName.chop(1);
        break;
    case RValueReference:
        typeName.chop(2);
        break;
    }
    while (typeName.endsWith(QLatin1Char('*')) || typeName.endsWith(QLatin1Char(' ')))
        typeName.chop(1);
    return QLatin1String("::") + typeName;
}

std::optional<DefaultValue>
    Generator::minimalConstructor(const ApiExtractorResult &api,
                                  const AbstractMetaType &type,
                                  QString *errorString)
{
    if (type.referenceType() == LValueReference && type.isObjectType())
        return {};

    if (type.isContainer()) {
        QString ctor = type.cppSignature();
        if (ctor.endsWith(QLatin1Char('*'))) {
            ctor.chop(1);
            return DefaultValue(DefaultValue::Pointer, ctor.trimmed());
        }
        if (ctor.startsWith(QLatin1String("const ")))
            ctor.remove(0, sizeof("const ") / sizeof(char) - 1);
        if (ctor.endsWith(QLatin1Char('&'))) {
            ctor.chop(1);
            ctor = ctor.trimmed();
        }
        return DefaultValue(DefaultValue::DefaultConstructor, QLatin1String("::") + ctor);
    }

    if (type.isNativePointer())
        return DefaultValue(DefaultValue::Pointer, type.typeEntry()->qualifiedCppName());
    if (type.isPointer())
        return DefaultValue(DefaultValue::Pointer, QLatin1String("::") + type.typeEntry()->qualifiedCppName());

    if (type.typeEntry()->isSmartPointer())
        return minimalConstructor(api, type.typeEntry());

    if (type.typeEntry()->isComplex()) {
        auto cType = static_cast<const ComplexTypeEntry *>(type.typeEntry());
        if (cType->hasDefaultConstructor())
            return DefaultValue(DefaultValue::Custom, cType->defaultConstructor());
        auto klass = AbstractMetaClass::findClass(api.classes(), cType);
        if (!klass) {
            if (errorString != nullptr)
                *errorString = msgClassNotFound(cType);
            return {};
        }
        auto ctorO = minimalConstructor(api, klass);
        if (ctorO.has_value() && type.hasInstantiations()) {
            auto ctor = ctorO.value();
            QString v = ctor.value();
            v.replace(getFullTypeName(cType), getFullTypeNameWithoutModifiers(type));
            ctor.setValue(v);
            return ctor;
        }
        return ctorO;
    }

    return minimalConstructor(api, type.typeEntry(), errorString);
}

std::optional<DefaultValue>
   Generator::minimalConstructor(const ApiExtractorResult &api,
                                 const TypeEntry *type,
                                 QString *errorString)
{
    if (!type)
        return {};

    if (type->isCppPrimitive()) {
        const QString &name = type->qualifiedCppName();
        return name == QLatin1String("bool")
            ? DefaultValue(DefaultValue::Boolean)
            : DefaultValue(DefaultValue::CppScalar, name);
    }

    if (type->isEnum()) {
        const auto enumEntry = static_cast<const EnumTypeEntry *>(type);
        if (const auto *nullValue = enumEntry->nullValue())
            return DefaultValue(DefaultValue::Enum, nullValue->name());
        return DefaultValue(DefaultValue::Custom,
                            QLatin1String("static_cast< ::") + type->qualifiedCppName()
                            + QLatin1String(">(0)"));
    }

    if (type->isFlags()) {
        return DefaultValue(DefaultValue::Custom,
                            type->qualifiedCppName() + QLatin1String("(0)"));
    }

    if (type->isPrimitive()) {
        QString ctor = static_cast<const PrimitiveTypeEntry *>(type)->defaultConstructor();
        // If a non-C++ (i.e. defined by the user) primitive type does not have
        // a default constructor defined by the user, the empty constructor is
        // heuristically returned. If this is wrong the build of the generated
        // bindings will tell.
        return ctor.isEmpty()
            ? DefaultValue(DefaultValue::DefaultConstructorWithDefaultValues, QLatin1String("::")
                           + type->qualifiedCppName())
            : DefaultValue(DefaultValue::Custom, ctor);
    }

    if (type->isSmartPointer())
        return DefaultValue(DefaultValue::DefaultConstructor, type->qualifiedCppName());

    if (type->isComplex()) {
        auto klass = AbstractMetaClass::findClass(api.classes(), type);
        if (!klass) {
            if (errorString != nullptr)
                *errorString = msgClassNotFound(type);
            return {};
        }
        return minimalConstructor(api, klass, errorString);
    }

    if (errorString != nullptr)
        *errorString = QLatin1String("No default value could be determined.");
    return {};
}

static QString constructorCall(const QString &qualifiedCppName, const QStringList &args)
{
    return QLatin1String("::") + qualifiedCppName + QLatin1Char('(')
        + args.join(QLatin1String(", ")) + QLatin1Char(')');
}

std::optional<DefaultValue>
    Generator::minimalConstructor(const ApiExtractorResult &api,
                                  const AbstractMetaClass *metaClass,
                                  QString *errorString)
{
    if (!metaClass)
        return {};

    auto cType = static_cast<const ComplexTypeEntry *>(metaClass->typeEntry());
    if (cType->hasDefaultConstructor())
        return DefaultValue(DefaultValue::Custom, cType->defaultConstructor());

    const QString qualifiedCppName = cType->qualifiedCppName();
    // Obtain a list of constructors sorted by complexity and number of arguments
    QMultiMap<int, const AbstractMetaFunctionCPtr> candidates;
    const auto &constructors = metaClass->queryFunctions(FunctionQueryOption::Constructors);
    for (const auto &ctor : constructors) {
        if (!ctor->isUserAdded() && !ctor->isPrivate()
            && ctor->functionType() == AbstractMetaFunction::ConstructorFunction) {
            // No arguments: Default constructible
            const auto &arguments = ctor->arguments();
            if (arguments.isEmpty()) {
                return DefaultValue(DefaultValue::DefaultConstructor,
                                    QLatin1String("::") + qualifiedCppName);
            }
            // First argument has unmodified default: Default constructible with values
            if (arguments.constFirst().hasUnmodifiedDefaultValueExpression()) {
                return DefaultValue(DefaultValue::DefaultConstructorWithDefaultValues,
                                    QLatin1String("::") + qualifiedCppName);
            }
            // Examine arguments, exclude functions taking a self parameter
            bool simple = true;
            bool suitable = true;
            for (int i = 0, size = arguments.size();
                 suitable && i < size && !arguments.at(i).hasOriginalDefaultValueExpression(); ++i) {
                const AbstractMetaArgument &arg = arguments.at(i);
                const TypeEntry *aType = arg.type().typeEntry();
                suitable &= aType != cType;
                simple &= aType->isCppPrimitive() || aType->isEnum() || arg.type().isPointer();
            }
            if (suitable)
                candidates.insert(arguments.size() + (simple ? 0 : 100), ctor);
        }
    }

    for (auto it = candidates.cbegin(), end = candidates.cend(); it != end; ++it) {
        const AbstractMetaArgumentList &arguments = it.value()->arguments();
        QStringList args;
        for (int i = 0, size = arguments.size(); i < size; ++i) {
            const AbstractMetaArgument &arg = arguments.at(i);
            if (arg.hasModifiedDefaultValueExpression()) {
                args << arg.defaultValueExpression(); // Spell out modified values
                break;
            }
            if (arg.hasOriginalDefaultValueExpression())
                break;
            auto argValue = minimalConstructor(api, arg.type(), errorString);
            if (!argValue.has_value())
                return {};
            args << argValue->constructorParameter();
        }
        return DefaultValue(DefaultValue::Custom, constructorCall(qualifiedCppName, args));
    }

    return {};
}

QString Generator::translateType(AbstractMetaType cType,
                                 const AbstractMetaClass *context,
                                 Options options) const
{
    QString s;

    if (context &&
        context->typeEntry()->isGenericClass() &&
        cType.originalTemplateType()) {
        cType = *cType.originalTemplateType();
    }

    if (cType.isVoid()) {
        s = QLatin1String("void");
    } else if (cType.isArray()) {
        s = translateType(*cType.arrayElementType(), context, options) + QLatin1String("[]");
    } else {
        if (options & Generator::ExcludeConst || options & Generator::ExcludeReference) {
            AbstractMetaType copyType = cType;

            if (options & Generator::ExcludeConst)
                copyType.setConstant(false);

            if (options & Generator::ExcludeReference)
                copyType.setReferenceType(NoReference);

            s = copyType.cppSignature();
            if (!copyType.typeEntry()->isVoid() && !copyType.typeEntry()->isCppPrimitive())
                s.prepend(QLatin1String("::"));
        } else {
            s = cType.cppSignature();
        }
    }

    return s;
}

static const QHash<QString, QString> &pythonOperators()
{
    static const QHash<QString, QString> result = {
        // call operator
        {u"operator()"_qs, u"__call__"_qs},
        // Arithmetic operators
        {u"operator+"_qs, u"__add__"_qs},
        {u"operator-"_qs, u"__sub__"_qs},
        {u"operator*"_qs, u"__mul__"_qs},
        {u"operator/"_qs, u"__div__"_qs},
        {u"operator%"_qs, u"__mod__"_qs},
        // Inplace arithmetic operators
        {u"operator+="_qs, u"__iadd__"_qs},
        {u"operator-="_qs, u"__isub__"_qs},
        {u"operator++"_qs, u"__iadd__"_qs},
        {u"operator--"_qs, u"__isub__"_qs},
        {u"operator*="_qs, u"__imul__"_qs},
        {u"operator/="_qs, u"__idiv__"_qs},
        {u"operator%="_qs, u"__imod__"_qs},
        // Bitwise operators
        {u"operator&"_qs, u"__and__"_qs},
        {u"operator^"_qs, u"__xor__"_qs},
        {u"operator|"_qs, u"__or__"_qs},
        {u"operator<<"_qs, u"__lshift__"_qs},
        {u"operator>>"_qs, u"__rshift__"_qs},
        {u"operator~"_qs, u"__invert__"_qs},
        // Inplace bitwise operators
        {u"operator&="_qs, u"__iand__"_qs},
        {u"operator^="_qs, u"__ixor__"_qs},
        {u"operator|="_qs, u"__ior__"_qs},
        {u"operator<<="_qs, u"__ilshift__"_qs},
        {u"operator>>="_qs, u"__irshift__"_qs},
        // Comparison operators
        {u"operator=="_qs, u"__eq__"_qs},
        {u"operator!="_qs, u"__ne__"_qs},
        {u"operator<"_qs, u"__lt__"_qs},
        {u"operator>"_qs, u"__gt__"_qs},
        {u"operator<="_qs, u"__le__"_qs},
        {u"operator>="_qs, u"__ge__"_qs}
    };
    return result;
}

QString Generator::pythonOperatorFunctionName(const QString &cppOpFuncName)
{
    return pythonOperators().value(cppOpFuncName);
}

QString Generator::subDirectoryForClass(const AbstractMetaClass *clazz) const
{
    return subDirectoryForPackage(clazz->package());
}

QString Generator::subDirectoryForPackage(QString packageNameIn) const
{
    if (packageNameIn.isEmpty())
        packageNameIn = packageName();
    packageNameIn.replace(QLatin1Char('.'), QDir::separator());
    return packageNameIn;
}

template<typename T>
static QString getClassTargetFullName_(const T *t, bool includePackageName)
{
    QString name = t->name();
    const AbstractMetaClass *context = t->enclosingClass();
    while (context) {
        // If the type was marked as 'visible=false' we should not use it in
        // the type name
        if (NamespaceTypeEntry::isVisibleScope(context->typeEntry())) {
            name.prepend(QLatin1Char('.'));
            name.prepend(context->name());
        }
        context = context->enclosingClass();
    }
    if (includePackageName) {
        name.prepend(QLatin1Char('.'));
        name.prepend(t->package());
    }
    return name;
}

QString getClassTargetFullName(const AbstractMetaClass *metaClass, bool includePackageName)
{
    return getClassTargetFullName_(metaClass, includePackageName);
}

QString getClassTargetFullName(const AbstractMetaEnum &metaEnum, bool includePackageName)
{
    return getClassTargetFullName_(&metaEnum, includePackageName);
}

QString getClassTargetFullName(const AbstractMetaType &metaType, bool includePackageName)
{
    QString name = metaType.cppSignature();
    name.replace(QLatin1String("::"), QLatin1String("_"));
    name.replace(QLatin1Char('<'), QLatin1Char('_'));
    name.remove(QLatin1Char('>'));
    name.remove(QLatin1Char(' '));
    if (includePackageName) {
        name.prepend(QLatin1Char('.'));
        name.prepend(metaType.package());
    }
    return name;
}

QString getFilteredCppSignatureString(QString signature)
{
    signature.replace(QLatin1String("::"), QLatin1String("_"));
    signature.replace(QLatin1Char('<'), QLatin1Char('_'));
    signature.replace(QLatin1Char('>'), QLatin1Char('_'));
    signature.replace(QLatin1Char(' '), QLatin1Char('_'));
    return signature;
}
