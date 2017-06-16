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


#ifndef CODEMODEL_FWD_H
#define CODEMODEL_FWD_H

#include <QtCore/QList>
#include <QtCore/QSharedPointer>

// forward declarations
class CodeModel;
class _ArgumentModelItem;
class _ClassModelItem;
class _CodeModelItem;
class _EnumModelItem;
class _EnumeratorModelItem;
class _FileModelItem;
class _FunctionModelItem;
class _NamespaceModelItem;
class _ScopeModelItem;
class _TemplateParameterModelItem;
class _TypeDefModelItem;
class _VariableModelItem;
class _MemberModelItem;
class TypeInfo;

typedef QSharedPointer<_ArgumentModelItem> ArgumentModelItem;
typedef QSharedPointer<_ClassModelItem> ClassModelItem;
typedef QSharedPointer<_CodeModelItem> CodeModelItem;
typedef QSharedPointer<_EnumModelItem> EnumModelItem;
typedef QSharedPointer<_EnumeratorModelItem> EnumeratorModelItem;
typedef QSharedPointer<_FileModelItem> FileModelItem;
typedef QSharedPointer<_FunctionModelItem> FunctionModelItem;
typedef QSharedPointer<_NamespaceModelItem> NamespaceModelItem;
typedef QSharedPointer<_ScopeModelItem> ScopeModelItem;
typedef QSharedPointer<_TemplateParameterModelItem> TemplateParameterModelItem;
typedef QSharedPointer<_TypeDefModelItem> TypeDefModelItem;
typedef QSharedPointer<_VariableModelItem> VariableModelItem;
typedef QSharedPointer<_MemberModelItem> MemberModelItem;

typedef QList<ArgumentModelItem> ArgumentList;
typedef QList<ClassModelItem> ClassList;
typedef QList<CodeModelItem> ItemList;
typedef QList<EnumModelItem> EnumList;
typedef QList<EnumeratorModelItem> EnumeratorList;
typedef QList<FileModelItem> FileList;
typedef QList<FunctionModelItem> FunctionList;
typedef QList<NamespaceModelItem> NamespaceList;
typedef QList<ScopeModelItem> ScopeList;
typedef QList<TemplateParameterModelItem> TemplateParameterList;
typedef QList<TypeDefModelItem> TypeDefList;
typedef QList<VariableModelItem> VariableList;
typedef QList<MemberModelItem> MemberList;

#endif // CODEMODEL_FWD_H
