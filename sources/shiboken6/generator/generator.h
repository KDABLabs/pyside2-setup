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

#ifndef GENERATOR_H
#define GENERATOR_H

#include <abstractmetatype.h>
#include <typedatabase_typedefs.h>
#include <QtCore/QObject>
#include <QtCore/QSharedPointer>
#include <QtCore/QTextStream>
#include <QtCore/QList>

#include <optional>

class ApiExtractorResult;
class AbstractMetaFunction;
class AbstractMetaClass;
class AbstractMetaEnum;
class TypeEntry;
class ComplexTypeEntry;
class AbstractMetaType;
class EnumTypeEntry;
class FlagsTypeEntry;
class TextStream;

QT_BEGIN_NAMESPACE
class QFile;
class QDebug;
QT_END_NAMESPACE

class PrimitiveTypeEntry;
class ContainerTypeEntry;

QString getClassTargetFullName(const AbstractMetaClass *metaClass, bool includePackageName = true);
QString getClassTargetFullName(const AbstractMetaEnum &metaEnum, bool includePackageName = true);
QString getClassTargetFullName(const AbstractMetaType &metaType, bool includePackageName = true);
QString getFilteredCppSignatureString(QString signature);

/**
 * PYSIDE-504: Handling the "protected hack"
 *
 * The problem: Creating wrappers when the class has private destructors.
 * You can see an example on Windows in qclipboard_wrapper.h and others.
 * Simply search for the text "// C++11: need to declare (unimplemented) destructor".
 *
 * The protected hack is the definition "#define protected public".
 * For most compilers, this "hack" is enabled, because the problem of private
 * destructors simply vanishes.
 *
 * If one does not want to use this hack, then a new problem arises:
 * C++11 requires that a destructor is declared in a wrapper class when it is
 * private in the base class. There is no implementation allowed!
 *
 * Unfortunately, MSVC in recent versions supports C++11, and due to restrictive
 * rules, it is impossible to use the hack with this compiler.
 * More unfortunate: Clang, when C++11 is enabled, also enforces a declaration
 * of a private destructor, but it falsely then creates a linker error!
 *
 * Originally, we wanted to remove the protected hack. But due to the Clang
 * problem, we gave up on removal of the protected hack and use it always
 * when we can. This might change again when the Clang problem is solved.
 */

#ifdef Q_CC_MSVC
const int alwaysGenerateDestructor = 1;
#else
const int alwaysGenerateDestructor = 0;
#endif

class DefaultValue
{
public:
    enum Type
    {
        Boolean,
        CppScalar, // A C++ scalar type (int,..) specified by value()
        Custom, // A custom constructor/expression, uses value() as is
        DefaultConstructor, // For classes named value()
        DefaultConstructorWithDefaultValues, // as DefaultConstructor, but can't return {} though.
        Enum, // Enum value as specified by value()
        Pointer, // Pointer of type value()
        Void  // "", for return values only
    };

    explicit DefaultValue(Type t, QString value = QString());
    explicit DefaultValue(QString customValue);

    QString returnValue() const;
    QString initialization() const;
    QString constructorParameter() const;

    QString value() const { return m_value; }
    void setValue(const QString &value) { m_value = value; }

    Type type() const { return m_type; }
    void setType(Type type) { m_type = type; }

private:
    Type m_type;
    QString m_value;
};

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug debug, const DefaultValue &v);
#endif

/**
 * A GeneratorContext object contains a pointer to an AbstractMetaClass and/or a specialized
 * AbstractMetaType, for which code is currently being generated.
 *
 * The main case is when the context contains only an AbstractMetaClass pointer, which is used
 * by different methods to generate appropriate expressions, functions, type names, etc.
 *
 * The second case is for generation of code for smart pointers. In this case the m_metaClass member
 * contains the generic template class of the smart pointer, and the m_preciseClassType member
 * contains the instantiated template type, e.g. a concrete shared_ptr<int>. To
 * distinguish this case, the member m_forSmartPointer is set to true.
 *
 * In the future the second case might be generalized for all template type instantiations.
 */
class GeneratorContext {
    friend class ShibokenGenerator;
    friend class Generator;
public:
    enum Type { Class, WrappedClass, SmartPointer };

    GeneratorContext() = default;

    const AbstractMetaClass *metaClass() const { return m_metaClass; }
    const AbstractMetaType &preciseType() const { return m_preciseClassType; }
    const AbstractMetaClass *pointeeClass() const { return m_pointeeClass; }

    bool forSmartPointer() const { return m_type == SmartPointer; }
    bool useWrapper() const { return m_type ==  WrappedClass; }

    QString wrapperName() const
    {
        Q_ASSERT(m_type == WrappedClass);
        return m_wrappername;
    }

    QString smartPointerWrapperName() const;

private:
    const AbstractMetaClass *m_metaClass = nullptr;
    const AbstractMetaClass *m_pointeeClass = nullptr;
    AbstractMetaType m_preciseClassType;
    QString m_wrappername;
    Type m_type = Class;
};

/**
 *   Base class for all generators. The default implementations does nothing,
 *   you must subclass this to create your own generators.
 */
class Generator
{
public:
    using OptionDescription = QPair<QString, QString>;
    using OptionDescriptions = QList<OptionDescription>;

    /// Optiosn used around the generator code
    enum Option {
        NoOption                 = 0x00000000,
        ExcludeConst             = 0x00000001,
        ExcludeReference         = 0x00000002,

        SkipReturnType           = 0x00000010,
        VirtualCall              = 0x00000040,
        OriginalTypeDescription  = 0x00000080,
        SkipRemovedArguments     = 0x00000100,

        SkipDefaultValues        = 0x00000200,
    };
    Q_DECLARE_FLAGS(Options, Option)

    Generator();
    virtual ~Generator();

    bool setup(const ApiExtractorResult &api);

    virtual OptionDescriptions options() const;
    virtual bool handleOption(const QString &key, const QString &value);

    /// Returns the top namespace made invisible
    const AbstractMetaClassCList &invisibleTopNamespaces() const;

    /// Returns the output directory
    QString outputDirectory() const;

    /// Set the output directory
    void setOutputDirectory(const QString &outDir);

    /**
     *   Start the code generation, be sure to call setClasses before callign this method.
     *   For each class it creates a QTextStream, call the write method with the current
     *   class and the associated text stream, then write the text stream contents if needed.
     *   \see #write
     */
    bool generate();

    /// Returns the license comment to be prepended to each source file generated.
    QString licenseComment() const;

    /// Sets the license comment to be prepended to each source file generated.
    void setLicenseComment(const QString &licenseComment);

    /// Returns the generator's name. Used for cosmetic purposes.
    virtual const char *name() const = 0;

    /// Returns the API as determined by ApiExtractor
    const ApiExtractorResult &api() const;

    bool hasPrivateClasses() const;

    /// Returns true if the user enabled PySide extensions (command line option)
    bool usePySideExtensions() const;

    /**
     *  Retrieves the name of the currently processed module.
     *  While package name is a complete package idetification, e.g. 'PySide.QtCore',
     *  a module name represents the last part of the package, e.g. 'QtCore'.
     *  If the target language separates the modules with characters other than
     *  dots ('.') the generator subclass must overload this method.
     *  \return a string representing the last part of a package name
     */
    static QString moduleName();

protected:
    /// Returns all primitive types found by APIExtractor
    static PrimitiveTypeEntryList primitiveTypes();

    /// Returns all container types found by APIExtractor
    static ContainerTypeEntryList containerTypes();

    virtual GeneratorContext contextForClass(const AbstractMetaClass *c) const;
    static GeneratorContext contextForSmartPointer(const AbstractMetaClass *c,
                                                   const AbstractMetaType &t,
                                                   const AbstractMetaClass *pointeeClass = nullptr);

    /// Generates a file for given AbstractMetaClass or AbstractMetaType (smart pointer case).
    bool generateFileForContext(const GeneratorContext &context);

    /// Returns the file base name for a smart pointer.
    static QString getFileNameBaseForSmartPointer(const AbstractMetaType &smartPointerType,
                                                  const AbstractMetaClass *smartPointer);

    /// Returns true if the generator should generate any code for the TypeEntry.
    static bool shouldGenerateTypeEntry(const TypeEntry *) ;

    /// Returns true if the generator should generate any code for the AbstractMetaClass.
    virtual bool shouldGenerate(const AbstractMetaClass *) const;

    /// Returns the subdirectory used to write the binding code of an AbstractMetaClass.
    virtual QString subDirectoryForClass(const AbstractMetaClass *clazz) const;

    /**
    *   Translate metatypes to binding source format.
    *   \param metatype a pointer to metatype
    *   \param context the current meta class
    *   \param option some extra options
    *   \return the metatype translated to binding source format
    */
    QString translateType(AbstractMetaType metatype,
                          const AbstractMetaClass *context,
                          Options options = NoOption) const;

    static QString pythonOperatorFunctionName(const QString &cppOpFuncName);

    /**
     *   Returns the package name.
     */
    static QString packageName();

    // Returns the full name of the type.
    static QString getFullTypeName(const TypeEntry *type);
    static QString getFullTypeName(const AbstractMetaType &type);
    static QString getFullTypeName(const AbstractMetaClass *metaClass);

    /**
     *  Returns the full qualified C++ name for an AbstractMetaType, but removing modifiers
     *  as 'const', '&', and '*' (except if the class is not derived from a template).
     *  This is useful for instantiated templates.
     */
    static QString getFullTypeNameWithoutModifiers(const AbstractMetaType &type);

    /**
     *   Tries to build a minimal constructor for the type.
     *   It will check first for a user defined default constructor.
     *   Returns a null string if it fails.
     */
    static std::optional<DefaultValue>
        minimalConstructor(const ApiExtractorResult &api, const TypeEntry *type,
                           QString *errorString = nullptr);
    static std::optional<DefaultValue>
        minimalConstructor(const ApiExtractorResult &api, const AbstractMetaType &type,
                           QString *errorString = nullptr);
    static std::optional<DefaultValue>
        minimalConstructor(const ApiExtractorResult &api,
                           const AbstractMetaClass *metaClass,
                           QString *errorString = nullptr);

    /**
     *   Returns the file name used to write the binding code of an AbstractMetaClass/Type.
     *   \param context the GeneratorContext which contains an AbstractMetaClass or AbstractMetaType
     *   for which the file name must be returned
     *   \return the file name used to write the binding code for the class
     */
    virtual QString fileNameSuffix() const = 0;
    virtual QString fileNameForContext(const GeneratorContext &context) const = 0;


    virtual bool doSetup() = 0;

    /**
     *   Write the bindding code for an AbstractMetaClass.
     *   This is called by generate method.
     *   \param  s   text stream to write the generated output
     *   \param  metaClass  the class that should be generated
     */
    virtual void generateClass(TextStream &s, const GeneratorContext &classContext) = 0;
    virtual bool finishGeneration() = 0;

    /**
    *    Returns the subdirectory path for a given package
    *    (aka module, aka library) name.
    *    If the target language separates the package modules with characters other
    *    than dots ('.') the generator subclass must overload this method.
    *    /param packageName complete package name for which to return the subdirectory path
    *    or nothing the use the name of the currently processed package
    *    /return a string representing the subdirectory path for the given package
    */
    virtual QString subDirectoryForPackage(QString packageName = QString()) const;

    AbstractMetaTypeList instantiatedContainers() const;
    AbstractMetaTypeList instantiatedSmartPointers() const;

    static QString getSimplifiedContainerTypeName(const AbstractMetaType &type);
    void addInstantiatedContainersAndSmartPointers(const AbstractMetaType &type,
                                                   const QString &context);

private:
    struct GeneratorPrivate;
    GeneratorPrivate *m_d;
    void collectInstantiatedContainersAndSmartPointers(const AbstractMetaFunctionCPtr &func);
    void collectInstantiatedContainersAndSmartPointers(const AbstractMetaClass *metaClass);
    void collectInstantiatedContainersAndSmartPointers();
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Generator::Options)
using GeneratorPtr = QSharedPointer<Generator>;
using Generators = QList<GeneratorPtr>;

#endif // GENERATOR_H

