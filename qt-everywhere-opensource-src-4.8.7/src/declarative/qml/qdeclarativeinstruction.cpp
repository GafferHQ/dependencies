/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "private/qdeclarativeinstruction_p.h"

#include "private/qdeclarativecompiler_p.h"

#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

void QDeclarativeCompiledData::dump(QDeclarativeInstruction *instr, int idx)
{
#ifdef QT_NO_DEBUG_STREAM
    Q_UNUSED(instr)
    Q_UNUSED(idx)
#else
    QByteArray lineNumber = QByteArray::number(instr->line);
    if (instr->line == (unsigned short)-1)
        lineNumber = "NA";
    const char *line = lineNumber.constData();

    switch(instr->type) {
    case QDeclarativeInstruction::Init:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "INIT\t\t\t" << instr->init.bindingsSize << "\t" << instr->init.parserStatusSize << "\t" << instr->init.contextCache << "\t" << instr->init.compiledBinding;
        break;
    case QDeclarativeInstruction::CreateObject:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "CREATE\t\t\t" << instr->create.type << "\t" << instr->create.bindingBits << "\t\t" << types.at(instr->create.type).className;
        break;
    case QDeclarativeInstruction::CreateSimpleObject:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "CREATE_SIMPLE\t\t" << instr->createSimple.typeSize;
        break;
    case QDeclarativeInstruction::SetId:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "SETID\t\t\t" << instr->setId.value << "\t\t\t" << primitives.at(instr->setId.value);
        break;
    case QDeclarativeInstruction::SetDefault:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "SET_DEFAULT";
        break;
    case QDeclarativeInstruction::CreateComponent:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "CREATE_COMPONENT\t" << instr->createComponent.count;
        break;
    case QDeclarativeInstruction::StoreMetaObject:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_META\t\t" << instr->storeMeta.data;
        break;

    case QDeclarativeInstruction::StoreFloat:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_FLOAT\t\t" << instr->storeFloat.propertyIndex << "\t" << instr->storeFloat.value;
        break;
    case QDeclarativeInstruction::StoreDouble:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_DOUBLE\t\t" << instr->storeDouble.propertyIndex << "\t" << instr->storeDouble.value;
        break;
    case QDeclarativeInstruction::StoreInteger:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_INTEGER\t\t" << instr->storeInteger.propertyIndex << "\t" << instr->storeInteger.value;
        break;
    case QDeclarativeInstruction::StoreBool:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_BOOL\t\t" << instr->storeBool.propertyIndex << "\t" << instr->storeBool.value;
        break;
    case QDeclarativeInstruction::StoreString:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_STRING\t\t" << instr->storeString.propertyIndex << "\t" << instr->storeString.value << "\t\t" << primitives.at(instr->storeString.value);
        break;
    case QDeclarativeInstruction::StoreUrl:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_URL\t\t" << instr->storeUrl.propertyIndex << "\t" << instr->storeUrl.value << "\t\t" << urls.at(instr->storeUrl.value);
        break;
    case QDeclarativeInstruction::StoreColor:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_COLOR\t\t" << instr->storeColor.propertyIndex << "\t\t\t" << QString::number(instr->storeColor.value, 16);
        break;
    case QDeclarativeInstruction::StoreDate:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_DATE\t\t" << instr->storeDate.propertyIndex << "\t" << instr->storeDate.value;
        break;
    case QDeclarativeInstruction::StoreTime:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_TIME\t\t" << instr->storeTime.propertyIndex << "\t" << instr->storeTime.valueIndex;
        break;
    case QDeclarativeInstruction::StoreDateTime:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_DATETIME\t\t" << instr->storeDateTime.propertyIndex << "\t" << instr->storeDateTime.valueIndex;
        break;
    case QDeclarativeInstruction::StorePoint:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_POINT\t\t" << instr->storeRealPair.propertyIndex << "\t" << instr->storeRealPair.valueIndex;
        break;
    case QDeclarativeInstruction::StorePointF:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_POINTF\t\t" << instr->storeRealPair.propertyIndex << "\t" << instr->storeRealPair.valueIndex;
        break;
    case QDeclarativeInstruction::StoreSize:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_SIZE\t\t" << instr->storeRealPair.propertyIndex << "\t" << instr->storeRealPair.valueIndex;
        break;
    case QDeclarativeInstruction::StoreSizeF:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_SIZEF\t\t" << instr->storeRealPair.propertyIndex << "\t" << instr->storeRealPair.valueIndex;
        break;
    case QDeclarativeInstruction::StoreRect:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_RECT\t\t" << instr->storeRect.propertyIndex << "\t" << instr->storeRect.valueIndex;
        break;
    case QDeclarativeInstruction::StoreRectF:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_RECTF\t\t" << instr->storeRect.propertyIndex << "\t" << instr->storeRect.valueIndex;
        break;
    case QDeclarativeInstruction::StoreVector3D:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_VECTOR3D\t\t" << instr->storeVector3D.propertyIndex << "\t" << instr->storeVector3D.valueIndex;
        break;
    case QDeclarativeInstruction::StoreVariant:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_VARIANT\t\t" << instr->storeString.propertyIndex << "\t" << instr->storeString.value << "\t\t" << primitives.at(instr->storeString.value);
        break;
    case QDeclarativeInstruction::StoreVariantInteger:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_VARIANT_INTEGER\t\t" << instr->storeInteger.propertyIndex << "\t" << instr->storeInteger.value;
        break;
    case QDeclarativeInstruction::StoreVariantDouble:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_VARIANT_DOUBLE\t\t" << instr->storeDouble.propertyIndex << "\t" << instr->storeDouble.value;
        break;
    case QDeclarativeInstruction::StoreVariantBool:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_VARIANT_BOOL\t\t" << instr->storeBool.propertyIndex << "\t" << instr->storeBool.value;
        break;
    case QDeclarativeInstruction::StoreObject:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_OBJECT\t\t" << instr->storeObject.propertyIndex;
        break;
    case QDeclarativeInstruction::StoreVariantObject:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_VARIANT_OBJECT\t" << instr->storeObject.propertyIndex;
        break;
    case QDeclarativeInstruction::StoreInterface:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_INTERFACE\t\t" << instr->storeObject.propertyIndex;
        break;

    case QDeclarativeInstruction::StoreSignal:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_SIGNAL\t\t" << instr->storeSignal.signalIndex << "\t" << instr->storeSignal.value << "\t\t" << primitives.at(instr->storeSignal.value);
        break;
    case QDeclarativeInstruction::StoreImportedScript:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_IMPORTED_SCRIPT\t" << instr->storeScript.value;
        break;
    case QDeclarativeInstruction::StoreScriptString:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_SCRIPT_STRING\t" << instr->storeScriptString.propertyIndex << "\t" << instr->storeScriptString.value << "\t" << instr->storeScriptString.scope;
        break;

    case QDeclarativeInstruction::AssignSignalObject:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "ASSIGN_SIGNAL_OBJECT\t" << instr->assignSignalObject.signal << "\t\t\t" << datas.at(instr->assignSignalObject.signal);
        break;
    case QDeclarativeInstruction::AssignCustomType:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "ASSIGN_CUSTOMTYPE\t" << instr->assignCustomType.propertyIndex << "\t" << instr->assignCustomType.valueIndex;
        break;

    case QDeclarativeInstruction::StoreBinding:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_BINDING\t" << instr->assignBinding.property << "\t" << instr->assignBinding.value << "\t" << instr->assignBinding.context;
        break;
    case QDeclarativeInstruction::StoreBindingOnAlias:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_BINDING_ALIAS\t" << instr->assignBinding.property << "\t" << instr->assignBinding.value << "\t" << instr->assignBinding.context;
        break;
    case QDeclarativeInstruction::StoreCompiledBinding:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_COMPILED_BINDING\t" << instr->assignBinding.property << "\t" << instr->assignBinding.value << "\t" << instr->assignBinding.context;
        break;
    case QDeclarativeInstruction::StoreValueSource:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_VALUE_SOURCE\t" << instr->assignValueSource.property << "\t" << instr->assignValueSource.castValue;
        break;
    case QDeclarativeInstruction::StoreValueInterceptor:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_VALUE_INTERCEPTOR\t" << instr->assignValueInterceptor.property << "\t" << instr->assignValueInterceptor.castValue;
        break;

    case QDeclarativeInstruction::BeginObject:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "BEGIN\t\t\t" << instr->begin.castValue;
        break;
    case QDeclarativeInstruction::StoreObjectQList:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "STORE_OBJECT_QLIST";
        break;
    case QDeclarativeInstruction::AssignObjectList:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "ASSIGN_OBJECT_LIST";
        break;
    case QDeclarativeInstruction::FetchAttached:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "FETCH_ATTACHED\t\t" << instr->fetchAttached.id;
        break;
    case QDeclarativeInstruction::FetchQList:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "FETCH_QLIST\t\t" << instr->fetch.property;
        break;
    case QDeclarativeInstruction::FetchObject:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "FETCH\t\t\t" << instr->fetch.property;
        break;
    case QDeclarativeInstruction::FetchValueType:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "FETCH_VALUE\t\t" << instr->fetchValue.property << "\t" << instr->fetchValue.type << "\t" << instr->fetchValue.bindingSkipList;
        break;
    case QDeclarativeInstruction::PopFetchedObject:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "POP";
        break;
    case QDeclarativeInstruction::PopQList:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "POP_QLIST";
        break;
    case QDeclarativeInstruction::PopValueType:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "POP_VALUE\t\t" << instr->fetchValue.property << "\t" << instr->fetchValue.type;
        break;
    case QDeclarativeInstruction::Defer:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "DEFER" << "\t\t\t" << instr->defer.deferCount;
        break;
    default:
        qWarning().nospace() << idx << "\t\t" << line << "\t" << "XXX UNKNOWN INSTRUCTION" << "\t" << instr->type;
        break;
    }
#endif // QT_NO_DEBUG_STREAM
}

QT_END_NAMESPACE
