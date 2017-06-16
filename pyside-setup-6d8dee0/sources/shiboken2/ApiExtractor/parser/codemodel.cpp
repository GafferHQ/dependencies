/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2002-2005 Roberto Raggi <roberto@kdevelop.org>
** Contact: https://www.qt.io/licensing/
**
** This file is part of PySide2.
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


#include "codemodel.h"
#include <algorithm>
#include <functional>
#include <iostream>
#include <QDebug>
#include <QDir>

// Predicate to find an item by name in a list of QSharedPointer<Item>
template <class T> class ModelItemNamePredicate : public std::unary_function<bool, QSharedPointer<T> >
{
public:
    explicit ModelItemNamePredicate(const QString &name) : m_name(name) {}
    bool operator()(const QSharedPointer<T> &item) const { return item->name() == m_name; }

private:
    const QString m_name;
};

template <class T>
static QSharedPointer<T> findModelItem(const QList<QSharedPointer<T> > &list, const QString &name)
{
    typedef typename QList<QSharedPointer<T> >::const_iterator It;
    const It it = std::find_if(list.begin(), list.end(), ModelItemNamePredicate<T>(name));
    return it != list.end() ? *it : QSharedPointer<T>();
}

// ---------------------------------------------------------------------------

CodeModel::CodeModel() : m_globalNamespace(new _NamespaceModelItem(this))
{
}

CodeModel::~CodeModel()
{
}

NamespaceModelItem CodeModel::globalNamespace() const
{
    return m_globalNamespace;
}

void CodeModel::addFile(FileModelItem item)
{
    m_files.append(item);
}

FileModelItem CodeModel::findFile(const QString &name) const
{
    return findModelItem(m_files, name);
}

CodeModelItem CodeModel::findItem(const QStringList &qualifiedName, CodeModelItem scope) const
{
    for (int i = 0; i < qualifiedName.size(); ++i) {
        // ### Extend to look for members etc too.
        const QString &name = qualifiedName.at(i);

        if (NamespaceModelItem ns = qSharedPointerDynamicCast<_NamespaceModelItem>(scope)) {
            if (NamespaceModelItem tmp_ns = ns->findNamespace(name)) {
                scope = tmp_ns;
                continue;
            }
        }

        if (ScopeModelItem ss = qSharedPointerDynamicCast<_ScopeModelItem>(scope)) {
            if (ClassModelItem cs = ss->findClass(name)) {
                scope = cs;
            } else if (EnumModelItem es = ss->findEnum(name)) {
                if (i == qualifiedName.size() - 1)
                    return es;
            } else if (TypeDefModelItem tp = ss->findTypeDef(name)) {
                if (i == qualifiedName.size() - 1)
                    return tp;
            } else {
                // If we don't find the name in the scope chain we
                // need to return an empty item to indicate failure...
                return CodeModelItem();
            }
        }
    }

    return scope;
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug d, const CodeModel *m)
{
    QDebugStateSaver s(d);
    d.noquote();
    d.nospace();
    d << "CodeModel(";
    if (m) {
        const NamespaceModelItem globalNamespaceP = m->globalNamespace();
        if (globalNamespaceP.data())
            globalNamespaceP->formatDebug(d);
    } else {
        d << '0';
    }
    d << ')';
    return d;
}
#endif // !QT_NO_DEBUG_STREAM

// ---------------------------------------------------------------------------
TypeInfo TypeInfo::combine(const TypeInfo &__lhs, const TypeInfo &__rhs)
{
    TypeInfo __result = __lhs;

    __result.setConstant(__result.isConstant() || __rhs.isConstant());
    __result.setVolatile(__result.isVolatile() || __rhs.isVolatile());
    if (__rhs.referenceType() > __result.referenceType())
        __result.setReferenceType(__rhs.referenceType());
    __result.setIndirections(__result.indirections() + __rhs.indirections());
    __result.setArrayElements(__result.arrayElements() + __rhs.arrayElements());

    return __result;
}

TypeInfo TypeInfo::resolveType(TypeInfo const &__type, CodeModelItem __scope)
{
    CodeModel *__model = __scope->model();
    Q_ASSERT(__model != 0);

    return TypeInfo::resolveType(__model->findItem(__type.qualifiedName(), __scope),  __type, __scope);
}

TypeInfo TypeInfo::resolveType(CodeModelItem __item, TypeInfo const &__type, CodeModelItem __scope)
{
    // Copy the type and replace with the proper qualified name. This
    // only makes sence to do if we're actually getting a resolved
    // type with a namespace. We only get this if the returned type
    // has more than 2 entries in the qualified name... This test
    // could be improved by returning if the type was found or not.
    TypeInfo otherType(__type);
    if (__item && __item->qualifiedName().size() > 1) {
        otherType.setQualifiedName(__item->qualifiedName());
    }

    if (TypeDefModelItem __typedef = qSharedPointerDynamicCast<_TypeDefModelItem>(__item)) {
        const TypeInfo combined = TypeInfo::combine(__typedef->type(), otherType);
        const CodeModelItem nextItem = __scope->model()->findItem(combined.qualifiedName(), __scope);
        if (!nextItem)
            return combined;
        // PYSIDE-362, prevent recursion on opaque structs like
        // typedef struct xcb_connection_t xcb_connection_t;
        if (nextItem.data() ==__item.data()) {
            std::cerr << "** WARNING Bailing out recursion of " << __FUNCTION__
                << "() on " << qPrintable(__type.qualifiedName().join(QLatin1String("::")))
                << std::endl;
            return otherType;
        }
        return resolveType(nextItem, combined, __scope);
    }

    return otherType;
}

QString TypeInfo::toString() const
{
    QString tmp;

    tmp += m_qualifiedName.join(QLatin1String("::"));
    if (isConstant())
        tmp += QLatin1String(" const");

    if (isVolatile())
        tmp += QLatin1String(" volatile");

    if (indirections())
        tmp += QString(indirections(), QLatin1Char('*'));

    switch (referenceType()) {
    case NoReference:
        break;
    case LValueReference:
        tmp += QLatin1Char('&');
        break;
    case RValueReference:
        tmp += QLatin1String("&&");
        break;
    }

    if (isFunctionPointer()) {
        tmp += QLatin1String(" (*)(");
        for (int i = 0; i < m_arguments.count(); ++i) {
            if (i != 0)
                tmp += QLatin1String(", ");

            tmp += m_arguments.at(i).toString();
        }
        tmp += QLatin1Char(')');
    }

    foreach(QString elt, arrayElements()) {
        tmp += QLatin1Char('[');
        tmp += elt;
        tmp += QLatin1Char(']');
    }

    return tmp;
}

bool TypeInfo::operator==(const TypeInfo &other)
{
    if (arrayElements().count() != other.arrayElements().count())
        return false;

#if defined (RXX_CHECK_ARRAY_ELEMENTS) // ### it'll break
    for (int i = 0; i < arrayElements().count(); ++i) {
        QString elt1 = arrayElements().at(i).trimmed();
        QString elt2 = other.arrayElements().at(i).trimmed();

        if (elt1 != elt2)
            return false;
    }
#endif

    return flags == other.flags
           && m_qualifiedName == other.m_qualifiedName
           && (!m_functionPointer || m_arguments == other.m_arguments);
}

#ifndef QT_NO_DEBUG_STREAM
template <class It>
void formatSequence(QDebug &d, It i1, It i2, const char *separator=", ")
{
    for (It i = i1; i != i2; ++i) {
        if (i != i1)
            d << separator;
        d << *i;
    }
}

void TypeInfo::formatDebug(QDebug &d) const
{
    d << '"';
    formatSequence(d, m_qualifiedName.begin(), m_qualifiedName.end(), "\", \"");
    d << '"';
    if (m_constant)
        d << ", [const]";
    if (m_volatile)
        d << ", [volatile]";
    if (m_indirections)
        d << ", indirections=" << m_indirections;
    switch (m_referenceType) {
    case NoReference:
        break;
    case LValueReference:
        d << ", [ref]";
        break;
    case RValueReference:
        d << ", [rvalref]";
        break;
    }
    if (m_functionPointer) {
        d << ", function ptr(";
        formatSequence(d, m_arguments.begin(), m_arguments.end());
        d << ')';
    }
    if (!m_arrayElements.isEmpty()) {
        d << ", array[" << m_arrayElements.size() << "][";
        formatSequence(d, m_arrayElements.begin(), m_arrayElements.end());
        d << ']';
    }
}

QDebug operator<<(QDebug d, const TypeInfo &t)
{
    QDebugStateSaver s(d);
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    const int verbosity = d.verbosity();
#else
    const int verbosity = 0;
#endif
    d.noquote();
    d.nospace();
    d << "TypeInfo(";
    if (verbosity > 2)
        t.formatDebug(d);
    else
        d << t.toString();
    d << ')';
    return d;
}
#endif // !QT_NO_DEBUG_STREAM

// ---------------------------------------------------------------------------
_CodeModelItem::_CodeModelItem(CodeModel *model, int kind)
        : m_model(model),
        m_kind(kind),
        m_startLine(0),
        m_startColumn(0),
        m_endLine(0),
        m_endColumn(0)
{
}

_CodeModelItem::_CodeModelItem(CodeModel *model, const QString &name, int kind)
    : m_model(model),
    m_kind(kind),
    m_startLine(0),
    m_startColumn(0),
    m_endLine(0),
    m_endColumn(0),
    m_name(name)
{
}

_CodeModelItem::~_CodeModelItem()
{
}

int _CodeModelItem::kind() const
{
    return m_kind;
}

QStringList _CodeModelItem::qualifiedName() const
{
    QStringList q = scope();

    if (!name().isEmpty())
        q += name();

    return q;
}

QString _CodeModelItem::name() const
{
    return m_name;
}

void _CodeModelItem::setName(const QString &name)
{
    m_name = name;
}

QStringList _CodeModelItem::scope() const
{
    return m_scope;
}

void _CodeModelItem::setScope(const QStringList &scope)
{
    m_scope = scope;
}

QString _CodeModelItem::fileName() const
{
    return m_fileName;
}

void _CodeModelItem::setFileName(const QString &fileName)
{
    m_fileName = fileName;
}

FileModelItem _CodeModelItem::file() const
{
    return model()->findFile(fileName());
}

void _CodeModelItem::getStartPosition(int *line, int *column)
{
    *line = m_startLine;
    *column = m_startColumn;
}

void _CodeModelItem::setStartPosition(int line, int column)
{
    m_startLine = line;
    m_startColumn = column;
}

void _CodeModelItem::getEndPosition(int *line, int *column)
{
    *line = m_endLine;
    *column = m_endColumn;
}

void _CodeModelItem::setEndPosition(int line, int column)
{
    m_endLine = line;
    m_endColumn = column;
}

#ifndef QT_NO_DEBUG_STREAM
template <class It>
static void formatPtrSequence(QDebug &d, It i1, It i2, const char *separator=", ")
{
    for (It i = i1; i != i2; ++i) {
        if (i != i1)
            d << separator;
        d << i->data();
    }
}

void _CodeModelItem::formatKind(QDebug &d, int k)
{
    switch (k) {
    case Kind_Argument:
        d << "ArgumentModelItem";
        break;
    case Kind_Class:
        d << "ClassModelItem";
        break;
    case Kind_Enum:
        d << "EnumModelItem";
        break;
    case Kind_Enumerator:
        d << "EnumeratorModelItem";
        break;
    case Kind_File:
        d << "FileModelItem";
        break;
    case Kind_Function:
        d << "FunctionModelItem";
        break;
    case Kind_Member:
        d << "MemberModelItem";
        break;
    case Kind_Namespace:
        d << "NamespaceModelItem";
        break;
    case Kind_Variable:
        d << "VariableModelItem";
        break;
    case Kind_Scope:
        d << "ScopeModelItem";
        break;
    case Kind_TemplateParameter:
        d << "TemplateParameter";
        break;
    case Kind_TypeDef:
        d << "TypeDefModelItem";
        break;
    default:
        d << "CodeModelItem";
        break;
    }
}

void _CodeModelItem::formatDebug(QDebug &d) const
{
     d << "(\"" << name() << '"';
     if (!m_scope.isEmpty()) {
         d << ", scope=";
         formatSequence(d, m_scope.cbegin(), m_scope.cend(), "::");
     }
     if (!m_fileName.isEmpty()) {
         d << ", file=\"" << QDir::toNativeSeparators(m_fileName);
         if (m_startLine > 0)
              d << ':' << m_startLine;
         d << '"';
     }
}

QDebug operator<<(QDebug d, const _CodeModelItem *t)
{
    QDebugStateSaver s(d);
    d.noquote();
    d.nospace();
    if (!t) {
        d << "CodeModelItem(0)";
        return d;
    }
    _CodeModelItem::formatKind(d, t->kind());
    t->formatDebug(d);
    switch (t->kind()) {
    case  _CodeModelItem::Kind_Class:
        d << " /* class " << t->name() << " */";
        break;
    case  _CodeModelItem::Kind_Namespace:
        d << " /* namespace " << t->name() << " */";
        break;
    }
    d << ')';
    return d;
}
#endif // !QT_NO_DEBUG_STREAM

// ---------------------------------------------------------------------------
_ClassModelItem::~_ClassModelItem()
{
}

QStringList _ClassModelItem::baseClasses() const
{
    return m_baseClasses;
}

void _ClassModelItem::setBaseClasses(const QStringList &baseClasses)
{
    m_baseClasses = baseClasses;
}

TemplateParameterList _ClassModelItem::templateParameters() const
{
    return m_templateParameters;
}

void _ClassModelItem::setTemplateParameters(const TemplateParameterList &templateParameters)
{
    m_templateParameters = templateParameters;
}

void _ClassModelItem::addBaseClass(const QString &baseClass)
{
    m_baseClasses.append(baseClass);
}

bool _ClassModelItem::extendsClass(const QString &name) const
{
    return m_baseClasses.contains(name);
}

void _ClassModelItem::setClassType(CodeModel::ClassType type)
{
    m_classType = type;
}

CodeModel::ClassType _ClassModelItem::classType() const
{
    return m_classType;
}

void _ClassModelItem::addPropertyDeclaration(const QString &propertyDeclaration)
{
    m_propertyDeclarations << propertyDeclaration;
}

#ifndef QT_NO_DEBUG_STREAM
template <class List>
static void formatModelItemList(QDebug &d, const char *prefix, const List &l,
                                const char *separator = ", ")
{
    if (const int size = l.size()) {
        d << prefix << '[' << size << "](";
        for (int i = 0; i < size; ++i) {
            if (i)
                d << separator;
            l.at(i)->formatDebug(d);
        }
        d << ')';
    }
}

void _ClassModelItem::formatDebug(QDebug &d) const
{
    _CodeModelItem::formatDebug(d);
    if (!m_baseClasses.isEmpty())
        d << ", inherits=" << m_baseClasses;
    formatModelItemList(d, ", templateParameters=", m_templateParameters);
    formatScopeItemsDebug(d);
}
#endif // !QT_NO_DEBUG_STREAM

// ---------------------------------------------------------------------------
FunctionModelItem _ScopeModelItem::declaredFunction(FunctionModelItem item)
{
    foreach (const FunctionModelItem &fun, m_functions) {
        if (fun->name() == item->name() && fun->isSimilar(item))
            return fun;

    }
    return FunctionModelItem();
}

_ScopeModelItem::~_ScopeModelItem()
{
}

void _ScopeModelItem::addEnumsDeclaration(const QString &enumsDeclaration)
{
    m_enumsDeclarations << enumsDeclaration;
}

void _ScopeModelItem::addClass(ClassModelItem item)
{
    m_classes.append(item);
}

void _ScopeModelItem::addFunction(FunctionModelItem item)
{
    m_functions.append(item);
}

void _ScopeModelItem::addVariable(VariableModelItem item)
{
    m_variables.append(item);
}

void _ScopeModelItem::addTypeDef(TypeDefModelItem item)
{
    m_typeDefs.append(item);
}

void _ScopeModelItem::addEnum(EnumModelItem item)
{
    m_enums.append(item);
}

#ifndef QT_NO_DEBUG_STREAM
template <class Hash>
static void formatScopeHash(QDebug &d, const char *prefix, const Hash &h,
                            const char *separator = ", ",
                            bool trailingNewLine = false)
{
    typedef typename Hash::ConstIterator HashIterator;
    if (!h.isEmpty()) {
        d << prefix << '[' << h.size() << "](";
        const HashIterator begin = h.begin();
        const HashIterator end = h.end();
        for (HashIterator it = begin; it != end; ++it) { // Omit the names as they are repeated
            if (it != begin)
                d << separator;
            d << it.value().data();
        }
        d << ')';
        if (trailingNewLine)
            d << '\n';
    }
}

template <class List>
static void formatScopeList(QDebug &d, const char *prefix, const List &l,
                            const char *separator = ", ",
                            bool trailingNewLine = false)
{
    if (!l.isEmpty()) {
        d << prefix << '[' << l.size() << "](";
        formatPtrSequence(d, l.begin(), l.end(), separator);
        d << ')';
        if (trailingNewLine)
            d << '\n';
    }
}

void _ScopeModelItem::formatScopeItemsDebug(QDebug &d) const
{
    formatScopeList(d, ", classes=", m_classes, "\n", true);
    formatScopeList(d, ", enums=", m_enums, "\n", true);
    formatScopeList(d, ", aliases=", m_typeDefs, "\n", true);
    formatScopeList(d, ", functions=", m_functions, "\n", true);
    formatScopeList(d, ", variables=", m_variables);
}

void  _ScopeModelItem::formatDebug(QDebug &d) const
{
    _CodeModelItem::formatDebug(d);
    formatScopeItemsDebug(d);
}
#endif // !QT_NO_DEBUG_STREAM

namespace {
// Predicate to match a non-template class name against the class list.
// "Vector" should match "Vector" as well as "Vector<T>" (as seen for methods
// from within the class "Vector").
class ClassNamePredicate : public std::unary_function<bool, ClassModelItem>
{
public:
    explicit ClassNamePredicate(const QString &name) : m_name(name) {}
    bool operator()(const ClassModelItem &item) const
    {
        const QString &itemName = item->name();
        if (!itemName.startsWith(m_name))
            return false;
        return itemName.size() == m_name.size() || itemName.at(m_name.size()) == QLatin1Char('<');
    }

private:
    const QString m_name;
};
} // namespace

ClassModelItem _ScopeModelItem::findClass(const QString &name) const
{
    // A fully qualified template is matched by name only
    const ClassList::const_iterator it = name.contains(QLatin1Char('<'))
        ? std::find_if(m_classes.begin(), m_classes.end(), ModelItemNamePredicate<_ClassModelItem>(name))
        : std::find_if(m_classes.begin(), m_classes.end(), ClassNamePredicate(name));
    return it != m_classes.end() ? *it : ClassModelItem();
}

VariableModelItem _ScopeModelItem::findVariable(const QString &name) const
{
    return findModelItem(m_variables, name);
}

TypeDefModelItem _ScopeModelItem::findTypeDef(const QString &name) const
{
    return findModelItem(m_typeDefs, name);
}

EnumModelItem _ScopeModelItem::findEnum(const QString &name) const
{
    return findModelItem(m_enums, name);
}

FunctionList _ScopeModelItem::findFunctions(const QString &name) const
{
    FunctionList result;
    foreach (const FunctionModelItem &func, m_functions) {
        if (func->name() == name)
            result.append(func);
    }
    return result;
}

// ---------------------------------------------------------------------------
_NamespaceModelItem::~_NamespaceModelItem()
{
}

void _NamespaceModelItem::addNamespace(NamespaceModelItem item)
{
    m_namespaces.append(item);
}

NamespaceModelItem _NamespaceModelItem::findNamespace(const QString &name) const
{
    return findModelItem(m_namespaces, name);
}

_FileModelItem::~_FileModelItem()
{
}

#ifndef QT_NO_DEBUG_STREAM
void _NamespaceModelItem::formatDebug(QDebug &d) const
{
    _ScopeModelItem::formatDebug(d);
    formatScopeList(d, ", namespaces=", m_namespaces);
}
#endif // !QT_NO_DEBUG_STREAM

// ---------------------------------------------------------------------------
_ArgumentModelItem::~_ArgumentModelItem()
{
}

TypeInfo _ArgumentModelItem::type() const
{
    return m_type;
}

void _ArgumentModelItem::setType(const TypeInfo &type)
{
    m_type = type;
}

bool _ArgumentModelItem::defaultValue() const
{
    return m_defaultValue;
}

void _ArgumentModelItem::setDefaultValue(bool defaultValue)
{
    m_defaultValue = defaultValue;
}

#ifndef QT_NO_DEBUG_STREAM
void _ArgumentModelItem::formatDebug(QDebug &d) const
{
    _CodeModelItem::formatDebug(d);
    d << ", type=" << m_type;
    if (m_defaultValue)
        d << ", defaultValue=\"" << m_defaultValueExpression << '"';
}
#endif // !QT_NO_DEBUG_STREAM
// ---------------------------------------------------------------------------
_FunctionModelItem::~_FunctionModelItem()
{
}

bool _FunctionModelItem::isSimilar(FunctionModelItem other) const
{
    if (name() != other->name())
        return false;

    if (isConstant() != other->isConstant())
        return false;

    if (isVariadics() != other->isVariadics())
        return false;

    if (arguments().count() != other->arguments().count())
        return false;

    // ### check the template parameters

    for (int i = 0; i < arguments().count(); ++i) {
        ArgumentModelItem arg1 = arguments().at(i);
        ArgumentModelItem arg2 = other->arguments().at(i);

        if (arg1->type() != arg2->type())
            return false;
    }

    return true;
}

ArgumentList _FunctionModelItem::arguments() const
{
    return m_arguments;
}

void _FunctionModelItem::addArgument(ArgumentModelItem item)
{
    m_arguments.append(item);
}

CodeModel::FunctionType _FunctionModelItem::functionType() const
{
    return m_functionType;
}

void _FunctionModelItem::setFunctionType(CodeModel::FunctionType functionType)
{
    m_functionType = functionType;
}

bool _FunctionModelItem::isVariadics() const
{
    return m_isVariadics;
}

void _FunctionModelItem::setVariadics(bool isVariadics)
{
    m_isVariadics = isVariadics;
}

bool _FunctionModelItem::isVirtual() const
{
    return m_isVirtual;
}

void _FunctionModelItem::setVirtual(bool isVirtual)
{
    m_isVirtual = isVirtual;
}

bool _FunctionModelItem::isInline() const
{
    return m_isInline;
}

void _FunctionModelItem::setInline(bool isInline)
{
    m_isInline = isInline;
}

bool _FunctionModelItem::isExplicit() const
{
    return m_isExplicit;
}

void _FunctionModelItem::setExplicit(bool isExplicit)
{
    m_isExplicit = isExplicit;
}

bool _FunctionModelItem::isAbstract() const
{
    return m_isAbstract;
}

void _FunctionModelItem::setAbstract(bool isAbstract)
{
    m_isAbstract = isAbstract;
}

// Qt
bool _FunctionModelItem::isInvokable() const
{
    return m_isInvokable;
}

void _FunctionModelItem::setInvokable(bool isInvokable)
{
    m_isInvokable = isInvokable;
}

#ifndef QT_NO_DEBUG_STREAM
void _FunctionModelItem::formatDebug(QDebug &d) const
{
    _MemberModelItem::formatDebug(d);
    d << ", type=" << m_functionType;
    if (m_isInline)
        d << " [inline]";
    if (m_isAbstract)
        d << " [abstract]";
    if (m_isExplicit)
        d << " [explicit]";
    if (m_isInvokable)
        d << " [invokable]";
    formatModelItemList(d, ", arguments=", m_arguments);
    if (m_isVariadics)
        d << ",...";
}
#endif // !QT_NO_DEBUG_STREAM

// ---------------------------------------------------------------------------
TypeInfo _TypeDefModelItem::type() const
{
    return m_type;
}

void _TypeDefModelItem::setType(const TypeInfo &type)
{
    m_type = type;
}

#ifndef QT_NO_DEBUG_STREAM
void _TypeDefModelItem::formatDebug(QDebug &d) const
{
    _CodeModelItem::formatDebug(d);
    d << ", type=" << m_type;
}
#endif // !QT_NO_DEBUG_STREAM

// ---------------------------------------------------------------------------
CodeModel::AccessPolicy _EnumModelItem::accessPolicy() const
{
    return m_accessPolicy;
}

_EnumModelItem::~_EnumModelItem()
{
}

void _EnumModelItem::setAccessPolicy(CodeModel::AccessPolicy accessPolicy)
{
    m_accessPolicy = accessPolicy;
}

EnumeratorList _EnumModelItem::enumerators() const
{
    return m_enumerators;
}

void _EnumModelItem::addEnumerator(EnumeratorModelItem item)
{
    m_enumerators.append(item);
}

bool _EnumModelItem::isAnonymous() const
{
    return m_anonymous;
}

void _EnumModelItem::setAnonymous(bool anonymous)
{
    m_anonymous = anonymous;
}

#ifndef QT_NO_DEBUG_STREAM
void _EnumModelItem::formatDebug(QDebug &d) const
{
    _CodeModelItem::formatDebug(d);
    if (m_anonymous)
         d << " (anonymous)";
    formatModelItemList(d, ", enumerators=", m_enumerators);
}
#endif // !QT_NO_DEBUG_STREAM

// ---------------------------------------------------------------------------
_EnumeratorModelItem::~_EnumeratorModelItem()
{
}

QString _EnumeratorModelItem::value() const
{
    return m_value;
}

void _EnumeratorModelItem::setValue(const QString &value)
{
    m_value = value;
}

#ifndef QT_NO_DEBUG_STREAM
void _EnumeratorModelItem::formatDebug(QDebug &d) const
{
    _CodeModelItem::formatDebug(d);
    if (!m_value.isEmpty())
        d << ", value=\"" << m_value << '"';
}
#endif // !QT_NO_DEBUG_STREAM

// ---------------------------------------------------------------------------
_TemplateParameterModelItem::~_TemplateParameterModelItem()
{
}

TypeInfo _TemplateParameterModelItem::type() const
{
    return m_type;
}

void _TemplateParameterModelItem::setType(const TypeInfo &type)
{
    m_type = type;
}

bool _TemplateParameterModelItem::defaultValue() const
{
    return m_defaultValue;
}

void _TemplateParameterModelItem::setDefaultValue(bool defaultValue)
{
    m_defaultValue = defaultValue;
}

#ifndef QT_NO_DEBUG_STREAM
void _TemplateParameterModelItem::formatDebug(QDebug &d) const
{
    _CodeModelItem::formatDebug(d);
    d << ", type=" << m_type;
    if (m_defaultValue)
        d << " [defaultValue]";
}
#endif // !QT_NO_DEBUG_STREAM

// ---------------------------------------------------------------------------
TypeInfo _MemberModelItem::type() const
{
    return m_type;
}

void _MemberModelItem::setType(const TypeInfo &type)
{
    m_type = type;
}

CodeModel::AccessPolicy _MemberModelItem::accessPolicy() const
{
    return m_accessPolicy;
}

_MemberModelItem::~_MemberModelItem()
{
}

void _MemberModelItem::setAccessPolicy(CodeModel::AccessPolicy accessPolicy)
{
    m_accessPolicy = accessPolicy;
}

bool _MemberModelItem::isStatic() const
{
    return m_isStatic;
}

void _MemberModelItem::setStatic(bool isStatic)
{
    m_isStatic = isStatic;
}

bool _MemberModelItem::isConstant() const
{
    return m_isConstant;
}

void _MemberModelItem::setConstant(bool isConstant)
{
    m_isConstant = isConstant;
}

bool _MemberModelItem::isVolatile() const
{
    return m_isVolatile;
}

void _MemberModelItem::setVolatile(bool isVolatile)
{
    m_isVolatile = isVolatile;
}

bool _MemberModelItem::isAuto() const
{
    return m_isAuto;
}

void _MemberModelItem::setAuto(bool isAuto)
{
    m_isAuto = isAuto;
}

bool _MemberModelItem::isFriend() const
{
    return m_isFriend;
}

void _MemberModelItem::setFriend(bool isFriend)
{
    m_isFriend = isFriend;
}

bool _MemberModelItem::isRegister() const
{
    return m_isRegister;
}

void _MemberModelItem::setRegister(bool isRegister)
{
    m_isRegister = isRegister;
}

bool _MemberModelItem::isExtern() const
{
    return m_isExtern;
}

void _MemberModelItem::setExtern(bool isExtern)
{
    m_isExtern = isExtern;
}

bool _MemberModelItem::isMutable() const
{
    return m_isMutable;
}

void _MemberModelItem::setMutable(bool isMutable)
{
    m_isMutable = isMutable;
}

#ifndef QT_NO_DEBUG_STREAM
void _MemberModelItem::formatDebug(QDebug &d) const
{
    _CodeModelItem::formatDebug(d);
    switch (m_accessPolicy) {
    case CodeModel::Public:
        d << ", public";
        break;
    case CodeModel::Protected:
        d << ", protected";
        break;
    case CodeModel::Private:
        d << ", private";
        break;
    }
    d << ", type=";
    if (m_isConstant)
        d << "const ";
    if (m_isVolatile)
        d << "volatile ";
    if (m_isStatic)
        d << "static ";
    if (m_isAuto)
        d << "auto ";
    if (m_isFriend)
        d << "friend ";
    if (m_isRegister)
        d << "register ";
    if (m_isExtern)
        d << "extern ";
    if (m_isMutable)
        d << "mutable ";
    d << m_type;
    formatScopeList(d, ", templateParameters", m_templateParameters);
}
#endif // !QT_NO_DEBUG_STREAM

// kate: space-indent on; indent-width 2; replace-tabs on;

