/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwinrtfiledialoghelper.h"
#include "qwinrtfileengine.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/QEventLoop>
#include <QtCore/QMap>
#include <QtCore/QVector>
#include <QtCore/qfunctions_winrt.h>
#include <private/qeventdispatcher_winrt_p.h>

#include <functional>
#include <wrl.h>
#include <windows.foundation.h>
#include <windows.storage.pickers.h>
#include <Windows.ApplicationModel.activation.h>

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::ApplicationModel::Activation;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::Storage;
using namespace ABI::Windows::Storage::Pickers;

#ifndef Q_OS_WINPHONE
typedef IAsyncOperationCompletedHandler<StorageFile *> SingleFileHandler;
typedef IAsyncOperationCompletedHandler<IVectorView<StorageFile *> *> MultipleFileHandler;
typedef IAsyncOperationCompletedHandler<StorageFolder *> SingleFolderHandler;
#endif

QT_BEGIN_NAMESPACE

// Required for save file picker
class WindowsStringVector : public RuntimeClass<IVector<HSTRING>>
{
public:
    HRESULT __stdcall GetAt(quint32 index, HSTRING *item)
    {
        *item = impl.at(index);
        return S_OK;
    }
    HRESULT __stdcall get_Size(quint32 *size)
    {
        *size = impl.size();
        return S_OK;
    }
    HRESULT __stdcall GetView(IVectorView<HSTRING> **view)
    {
        *view = Q_NULLPTR;
        return E_NOTIMPL;
    }
    HRESULT __stdcall IndexOf(HSTRING value, quint32 *index, boolean *found)
    {
        *found = false;
        for (int i = 0; i < impl.size(); ++i) {
            qint32 result;
            HRESULT hr = WindowsCompareStringOrdinal(impl.at(i), value, &result);
            if (FAILED(hr))
                return hr;
            if (result == 0) {
                *index = quint32(i);
                *found = true;
                break;
            }
        }
        return S_OK;
    }
    HRESULT __stdcall SetAt(quint32 index, HSTRING item)
    {
        HSTRING newItem;
        HRESULT hr = WindowsDuplicateString(item, &newItem);
        if (FAILED(hr))
            return hr;
        impl[index] = newItem;
        return S_OK;
    }
    HRESULT __stdcall InsertAt(quint32 index, HSTRING item)
    {
        HSTRING newItem;
        HRESULT hr = WindowsDuplicateString(item, &newItem);
        if (FAILED(hr))
            return hr;
        impl.insert(index, newItem);
        return S_OK;
    }
    HRESULT __stdcall RemoveAt(quint32 index)
    {
        WindowsDeleteString(impl.takeAt(index));
        return S_OK;
    }
    HRESULT __stdcall Append(HSTRING item)
    {
        HSTRING newItem;
        HRESULT hr = WindowsDuplicateString(item, &newItem);
        if (FAILED(hr))
            return hr;
        impl.append(newItem);
        return S_OK;
    }
    HRESULT __stdcall RemoveAtEnd()
    {
        WindowsDeleteString(impl.takeLast());
        return S_OK;
    }
    HRESULT __stdcall Clear()
    {
        foreach (const HSTRING &item, impl)
            WindowsDeleteString(item);
        impl.clear();
        return S_OK;
    }
private:
    QVector<HSTRING> impl;
};

#ifdef Q_OS_WINPHONE
class QActivationEvent : public QEvent
{
public:
    IInspectable *args() const {
        return reinterpret_cast<IInspectable *>(d);
    }
};
#endif

template<typename T>
static bool initializePicker(HSTRING runtimeId, T **picker, const QSharedPointer<QFileDialogOptions> &options)
{
    HRESULT hr;

    ComPtr<IInspectable> basePicker;
    hr = RoActivateInstance(runtimeId, &basePicker);
    RETURN_FALSE_IF_FAILED("Failed to instantiate file picker");
    hr = basePicker.Get()->QueryInterface(IID_PPV_ARGS(picker));
    RETURN_FALSE_IF_FAILED("Failed to cast file picker");

    if (options->isLabelExplicitlySet(QFileDialogOptions::Accept)) {
        const QString labelText = options->labelText(QFileDialogOptions::Accept);
        HStringReference labelTextRef(reinterpret_cast<const wchar_t *>(labelText.utf16()),
                                      labelText.length());
        hr = (*picker)->put_CommitButtonText(labelTextRef.Get());
        RETURN_FALSE_IF_FAILED("Failed to set commit button text");
    }

    return true;
}

template<typename T>
static bool initializeOpenPickerOptions(T *picker, const QSharedPointer<QFileDialogOptions> &options)
{
    HRESULT hr;
    hr = picker->put_ViewMode(options->viewMode() == QFileDialogOptions::Detail
                              ? PickerViewMode_Thumbnail : PickerViewMode_List);
    RETURN_FALSE_IF_FAILED("Failed to set picker view mode");

    ComPtr<IVector<HSTRING>> filters;
    hr = picker->get_FileTypeFilter(&filters);
    RETURN_FALSE_IF_FAILED("Failed to get file type filters list");
    foreach (const QString &namedFilter, options->nameFilters()) {
        foreach (const QString &filter, QPlatformFileDialogHelper::cleanFilterList(namedFilter)) {
            // Remove leading star
            const int offset = (filter.length() > 1 && filter.startsWith(QLatin1Char('*'))) ? 1 : 0;
            HStringReference filterRef(reinterpret_cast<const wchar_t *>(filter.utf16() + offset),
                                       filter.length() - offset);
            hr = filters->Append(filterRef.Get());
            if (FAILED(hr)) {
                qWarning("Failed to add named file filter \"%s\": %s",
                         qPrintable(filter), qPrintable(qt_error_string(hr)));
            }
        }
    }
    // The file dialog won't open with an empty list - add a default wildcard
    quint32 size;
    hr = filters->get_Size(&size);
    RETURN_FALSE_IF_FAILED("Failed to get file type filters list size");
    if (!size) {
        hr = filters->Append(HString::MakeReference(L"*").Get());
        RETURN_FALSE_IF_FAILED("Failed to add default wildcard to file type filters list");
    }

    return true;
}

static bool pickFiles(IFileOpenPicker *picker, QWinRTFileDialogHelper *helper, bool singleFile)
{
    Q_ASSERT(picker);
    Q_ASSERT(helper);
    HRESULT hr;
#ifdef Q_OS_WINPHONE
        hr = QEventDispatcherWinRT::runOnXamlThread([picker, singleFile]() {
            HRESULT hr;
            ComPtr<IFileOpenPicker2> picker2;
            hr = picker->QueryInterface(IID_PPV_ARGS(picker2.GetAddressOf()));
            RETURN_HR_IF_FAILED("Failed to cast file picker");
            if (singleFile)
                return picker2->PickSingleFileAndContinue();
            else
                return picker2->PickMultipleFilesAndContinue();
        });
        RETURN_FALSE_IF_FAILED("Failed to open file picker");
        QAbstractEventDispatcher *eventDispatcher = QCoreApplication::eventDispatcher();
        Q_ASSERT(eventDispatcher);
        eventDispatcher->installEventFilter(helper);
    return true;
#else
    hr = QEventDispatcherWinRT::runOnXamlThread([picker, helper, singleFile]() {
        HRESULT hr;
        if (singleFile) {
            ComPtr<IAsyncOperation<StorageFile *>> op;
            hr = picker->PickSingleFileAsync(&op);
            RETURN_HR_IF_FAILED("Failed to open single file picker");
            hr = op->put_Completed(Callback<SingleFileHandler>(helper, &QWinRTFileDialogHelper::onSingleFilePicked).Get());
            RETURN_HR_IF_FAILED("Failed to attach file picker callback");
        } else {
            ComPtr<IAsyncOperation<IVectorView<StorageFile *> *>> op;
            hr = picker->PickMultipleFilesAsync(&op);
            RETURN_HR_IF_FAILED("Failed to open multi file picker");
            hr = op->put_Completed(Callback<MultipleFileHandler>(helper, &QWinRTFileDialogHelper::onMultipleFilesPicked).Get());
            RETURN_HR_IF_FAILED("Failed to attach multi file callback");
        }
        return S_OK;
    });
    return SUCCEEDED(hr);
#endif
}

static bool pickFolder(IFolderPicker *picker, QWinRTFileDialogHelper *helper)
{
    Q_ASSERT(picker);
    Q_ASSERT(helper);
    HRESULT hr;
#ifdef Q_OS_WINPHONE
    hr = QEventDispatcherWinRT::runOnXamlThread([picker]() {
        HRESULT hr;
        ComPtr<IFolderPicker2> picker2;
        hr = picker->QueryInterface(IID_PPV_ARGS(picker2.GetAddressOf()));
        RETURN_HR_IF_FAILED("Failed to cast folder picker");
        return picker2->PickFolderAndContinue();
    });
    RETURN_FALSE_IF_FAILED("Failed to open folder picker");
    QAbstractEventDispatcher *eventDispatcher = QCoreApplication::eventDispatcher();
    Q_ASSERT(eventDispatcher);
    eventDispatcher->installEventFilter(helper);
#else
    hr = QEventDispatcherWinRT::runOnXamlThread([picker, helper]() {
        HRESULT hr;
        ComPtr<IAsyncOperation<StorageFolder *>> op;
        hr = picker->PickSingleFolderAsync(&op);
        RETURN_HR_IF_FAILED("Failed to open folder picker");
        hr = op->put_Completed(Callback<SingleFolderHandler>(helper, &QWinRTFileDialogHelper::onSingleFolderPicked).Get());
        RETURN_HR_IF_FAILED("Failed to attach folder picker callback");
        return S_OK;
    });
#endif
    return SUCCEEDED(hr);
}

static bool pickSaveFile(IFileSavePicker *picker, QWinRTFileDialogHelper *helper)
{
    Q_ASSERT(picker);
    Q_ASSERT(helper);
    HRESULT hr;
#ifdef Q_OS_WINPHONE
    hr = QEventDispatcherWinRT::runOnXamlThread([picker]() {
        HRESULT hr;
        ComPtr<IFileSavePicker2> picker2;
        hr = picker->QueryInterface(IID_PPV_ARGS(picker2.GetAddressOf()));
        RETURN_HR_IF_FAILED("Failed to cast save file picker");
        return picker2->PickSaveFileAndContinue();
    });
    RETURN_FALSE_IF_FAILED("Failed to open single file picker");
    QAbstractEventDispatcher *eventDispatcher = QCoreApplication::eventDispatcher();
    Q_ASSERT(eventDispatcher);
    eventDispatcher->installEventFilter(helper);
#else
    hr = QEventDispatcherWinRT::runOnXamlThread([picker, helper]() {
        HRESULT hr;
        ComPtr<IAsyncOperation<StorageFile *>> op;
        hr = picker->PickSaveFileAsync(&op);
        RETURN_HR_IF_FAILED("Failed to open save file picker");
        hr = op->put_Completed(Callback<SingleFileHandler>(helper, &QWinRTFileDialogHelper::onSingleFilePicked).Get());
        RETURN_HR_IF_FAILED("Failed to attach save file picker callback");
        return S_OK;
    });
#endif
    return SUCCEEDED(hr);
}

class QWinRTFileDialogHelperPrivate
{
public:
    bool shown;
    QEventLoop loop;

    // Input
    QUrl directory;
    QUrl saveFileName;
    QString selectedNameFilter;

    // Output
    QList<QUrl> selectedFiles;
};

QWinRTFileDialogHelper::QWinRTFileDialogHelper()
    : QPlatformFileDialogHelper(), d_ptr(new QWinRTFileDialogHelperPrivate)
{
    Q_D(QWinRTFileDialogHelper);

    d->shown = false;
}

QWinRTFileDialogHelper::~QWinRTFileDialogHelper()
{
}

void QWinRTFileDialogHelper::exec()
{
    Q_D(QWinRTFileDialogHelper);

    if (!d->shown)
        show(Qt::Dialog, Qt::ApplicationModal, 0);
    d->loop.exec();
}

bool QWinRTFileDialogHelper::show(Qt::WindowFlags windowFlags, Qt::WindowModality windowModality, QWindow *parent)
{
    Q_UNUSED(windowFlags)
    Q_UNUSED(windowModality)
    Q_UNUSED(parent)
    Q_D(QWinRTFileDialogHelper);

    HRESULT hr;
    const QSharedPointer<QFileDialogOptions> dialogOptions = options();
    switch (dialogOptions->acceptMode()) {
    default:
    case QFileDialogOptions::AcceptOpen: {
        switch (dialogOptions->fileMode()) {
        case QFileDialogOptions::AnyFile:
        case QFileDialogOptions::ExistingFile:
        case QFileDialogOptions::ExistingFiles: {
            ComPtr<IFileOpenPicker> picker;
            if (!initializePicker(HString::MakeReference(RuntimeClass_Windows_Storage_Pickers_FileOpenPicker).Get(),
                                  picker.GetAddressOf(), dialogOptions)) {
                return false;
            }
            if (!initializeOpenPickerOptions(picker.Get(), dialogOptions))
                return false;

            if (!pickFiles(picker.Get(), this, dialogOptions->fileMode() == QFileDialogOptions::ExistingFile))
                return false;

            break;
        }
        case QFileDialogOptions::Directory:
        case QFileDialogOptions::DirectoryOnly: {
            ComPtr<IFolderPicker> picker;
            if (!initializePicker(HString::MakeReference(RuntimeClass_Windows_Storage_Pickers_FolderPicker).Get(),
                                  picker.GetAddressOf(), dialogOptions)) {
                return false;
            }
            if (!initializeOpenPickerOptions(picker.Get(), dialogOptions))
                return false;

            if (!pickFolder(picker.Get(), this))
                return false;

            break;
        }
        }
        break;
    }
    case QFileDialogOptions::AcceptSave: {
        ComPtr<IFileSavePicker> picker;
        if (!initializePicker(HString::MakeReference(RuntimeClass_Windows_Storage_Pickers_FileSavePicker).Get(),
                              picker.GetAddressOf(), dialogOptions)) {
            return false;
        }

        if (!dialogOptions->nameFilters().isEmpty()) {
            ComPtr<IMap<HSTRING, IVector<HSTRING> *>> choices;
            hr = picker->get_FileTypeChoices(&choices);
            RETURN_FALSE_IF_FAILED("Failed to get file extension choices");
            foreach (const QString &namedFilter, dialogOptions->nameFilters()) {
                ComPtr<IVector<HSTRING>> entry = Make<WindowsStringVector>();
                foreach (const QString &filter, QPlatformFileDialogHelper::cleanFilterList(namedFilter)) {
                    // Remove leading star
                    const int offset = (filter.length() > 1 && filter.startsWith(QLatin1Char('*'))) ? 1 : 0;
                    HStringReference filterRef(reinterpret_cast<const wchar_t *>(filter.utf16() + offset),
                                               filter.length() - offset);
                    hr = entry->Append(filterRef.Get());
                    if (FAILED(hr)) {
                        qWarning("Failed to add named file filter \"%s\": %s",
                                 qPrintable(filter), qPrintable(qt_error_string(hr)));
                    }
                }
                const int offset = namedFilter.indexOf(QLatin1String(" ("));
                const QString filterTitle = namedFilter.mid(0, offset);
                HStringReference namedFilterRef(reinterpret_cast<const wchar_t *>(filterTitle.utf16()),
                                                filterTitle.length());
                boolean replaced;
                hr = choices->Insert(namedFilterRef.Get(), entry.Get(), &replaced);
                // Only print a warning as * or *.* is not a valid choice on Windows 10
                // but used on a regular basis on all other platforms
                if (FAILED(hr)) {
                    qWarning("Failed to insert file extension choice entry: %s: %s",
                             qPrintable(filterTitle), qPrintable(qt_error_string(hr)));
                }
            }
        }

        QString suffix = dialogOptions->defaultSuffix();
        if (!suffix.isEmpty()) {
            if (!suffix.startsWith(QLatin1Char('.')))
                suffix.prepend(QLatin1Char('.'));
            HStringReference nativeSuffix(reinterpret_cast<const wchar_t *>(suffix.utf16()),
                                          suffix.length());
            hr = picker->put_DefaultFileExtension(nativeSuffix.Get());
            RETURN_FALSE_IF_FAILED_WITH_ARGS("Failed to set default file extension \"%s\"", qPrintable(suffix));
        }

        const QString suggestedName = QFileInfo(d->saveFileName.toLocalFile()).fileName();
        if (!suggestedName.isEmpty()) {
            HStringReference nativeSuggestedName(reinterpret_cast<const wchar_t *>(suggestedName.utf16()),
                                                 suggestedName.length());
            hr = picker->put_SuggestedFileName(nativeSuggestedName.Get());
            RETURN_FALSE_IF_FAILED("Failed to set suggested file name");
        }

        if (!pickSaveFile(picker.Get(), this))
            return false;

        break;
    }
    }

    d->shown = true;
    return true;
}

void QWinRTFileDialogHelper::hide()
{
    Q_D(QWinRTFileDialogHelper);

    if (!d->shown)
        return;

    d->shown = false;
}

#ifdef Q_OS_WINPHONE
bool QWinRTFileDialogHelper::eventFilter(QObject *, QEvent *e)
{
    if (e->type() != QEvent::WinEventAct)
        return false;

    HRESULT hr;
    QActivationEvent *event = static_cast<QActivationEvent *>(e);
    ComPtr<IInspectable> inspectable = event->args();
    ComPtr<IActivatedEventArgs> arguments;
    hr = inspectable.As(&arguments);
    Q_ASSERT_SUCCEEDED(hr);

    ActivationKind activationKind;
    hr = arguments->get_Kind(&activationKind);
    Q_ASSERT_SUCCEEDED(hr);

    // Handle only File, Folder and Save file pick continuation here.
    if (activationKind != ActivationKind_PickFileContinuation
        && activationKind != ActivationKind_PickFolderContinuation
        && activationKind != ActivationKind_PickSaveFileContinuation) {
        return false;
    }

    QAbstractEventDispatcher *eventDispatcher = QCoreApplication::eventDispatcher();
    Q_ASSERT(eventDispatcher);
    eventDispatcher->removeEventFilter(this);
    e->accept();

    if (activationKind == ActivationKind_PickFileContinuation) {
        ComPtr<IFileOpenPickerContinuationEventArgs> fileContinuationArgs;
        hr = arguments.As(&fileContinuationArgs);
        Q_ASSERT_SUCCEEDED(hr);
        ComPtr<IVectorView<StorageFile *>> files;
        hr = fileContinuationArgs->get_Files(&files);
        Q_ASSERT_SUCCEEDED(hr);
        hr = onFilesPicked(files.Get());
        Q_ASSERT_SUCCEEDED(hr);
    } else if (activationKind == ActivationKind_PickFolderContinuation) {
        ComPtr<IFolderPickerContinuationEventArgs> folderContinuationArgs;
        hr = arguments.As(&folderContinuationArgs);
        Q_ASSERT_SUCCEEDED(hr);
        ComPtr<IStorageFolder> folder;
        hr = folderContinuationArgs->get_Folder(&folder);
        Q_ASSERT_SUCCEEDED(hr);
        hr = onFolderPicked(folder.Get());
        Q_ASSERT_SUCCEEDED(hr);
    } else {
        ComPtr<IFileSavePickerContinuationEventArgs> saveFileContinuationArgs;
        hr = arguments.As(&saveFileContinuationArgs);
        Q_ASSERT_SUCCEEDED(hr);
        ComPtr<IStorageFile> file;
        hr = saveFileContinuationArgs->get_File(&file);
        Q_ASSERT_SUCCEEDED(hr);
        hr = onFilePicked(file.Get());
        Q_ASSERT_SUCCEEDED(hr);
    }

    return true;
}
#endif

void QWinRTFileDialogHelper::setDirectory(const QUrl &directory)
{
    Q_D(QWinRTFileDialogHelper);
    d->directory = directory;
}

QUrl QWinRTFileDialogHelper::directory() const
{
    Q_D(const QWinRTFileDialogHelper);
    return d->directory;
}

void QWinRTFileDialogHelper::selectFile(const QUrl &saveFileName)
{
    Q_D(QWinRTFileDialogHelper);
    d->saveFileName = saveFileName;
}

QList<QUrl> QWinRTFileDialogHelper::selectedFiles() const
{
    Q_D(const QWinRTFileDialogHelper);
    return d->selectedFiles;
}

void QWinRTFileDialogHelper::selectNameFilter(const QString &selectedNameFilter)
{
    Q_D(QWinRTFileDialogHelper);
    d->selectedNameFilter = selectedNameFilter;
}

QString QWinRTFileDialogHelper::selectedNameFilter() const
{
    Q_D(const QWinRTFileDialogHelper);
    return d->selectedNameFilter;
}

#ifndef Q_OS_WINPHONE
HRESULT QWinRTFileDialogHelper::onSingleFilePicked(IAsyncOperation<StorageFile *> *args, AsyncStatus status)
{
    Q_D(QWinRTFileDialogHelper);

    QEventLoopLocker locker(&d->loop);
    d->shown = false;
    d->selectedFiles.clear();
    if (status == Canceled || status == Error) {
        emit reject();
        return S_OK;
    }

    HRESULT hr;
    ComPtr<IStorageFile> file;
    hr = args->GetResults(&file);
    Q_ASSERT_SUCCEEDED(hr);
    return onFilePicked(file.Get());
}

HRESULT QWinRTFileDialogHelper::onMultipleFilesPicked(IAsyncOperation<IVectorView<StorageFile *> *> *args, AsyncStatus status)
{
    Q_D(QWinRTFileDialogHelper);

    QEventLoopLocker locker(&d->loop);
    d->shown = false;
    d->selectedFiles.clear();
    if (status == Canceled || status == Error) {
        emit reject();
        return S_OK;
    }

    HRESULT hr;
    ComPtr<IVectorView<StorageFile *>> fileList;
    hr = args->GetResults(&fileList);
    RETURN_HR_IF_FAILED("Failed to get file list");
    return onFilesPicked(fileList.Get());
}

HRESULT QWinRTFileDialogHelper::onSingleFolderPicked(IAsyncOperation<StorageFolder *> *args, AsyncStatus status)
{
    Q_D(QWinRTFileDialogHelper);

    QEventLoopLocker locker(&d->loop);
    d->shown = false;
    d->selectedFiles.clear();
    if (status == Canceled || status == Error) {
        emit reject();
        return S_OK;
    }

    HRESULT hr;
    ComPtr<IStorageFolder> folder;
    hr = args->GetResults(&folder);
    Q_ASSERT_SUCCEEDED(hr);
    return onFolderPicked(folder.Get());
}
#endif //Q_OS_WINPHONE

HRESULT QWinRTFileDialogHelper::onFilesPicked(IVectorView<StorageFile *> *files)
{
#ifdef Q_OS_WINPHONE
    Q_D(QWinRTFileDialogHelper);
    QEventLoopLocker locker(&d->loop);
    d->shown = false;
    d->selectedFiles.clear();
#endif

    HRESULT hr;
    quint32 size;
    hr = files->get_Size(&size);
    Q_ASSERT_SUCCEEDED(hr);
    if (!size) {
        emit reject();
        return S_OK;
    }

    for (quint32 i = 0; i < size; ++i) {
        ComPtr<IStorageFile> file;
        hr = files->GetAt(i, &file);
        Q_ASSERT_SUCCEEDED(hr);
        appendFile(file.Get());
    }

    emit accept();
    return S_OK;
}

HRESULT QWinRTFileDialogHelper::onFolderPicked(IStorageFolder *folder)
{
#ifdef Q_OS_WINPHONE
    Q_D(QWinRTFileDialogHelper);
    QEventLoopLocker locker(&d->loop);
    d->shown = false;
    d->selectedFiles.clear();
#endif

    if (!folder) {
        emit reject();
        return S_OK;
    }

    appendFile(folder);
    emit accept();
    return S_OK;
}

HRESULT QWinRTFileDialogHelper::onFilePicked(IStorageFile *file)
{
#ifdef Q_OS_WINPHONE
    Q_D(QWinRTFileDialogHelper);
    QEventLoopLocker locker(&d->loop);
    d->shown = false;
    d->selectedFiles.clear();
#endif

    if (!file) {
        emit reject();
        return S_OK;
    }

    appendFile(file);
    emit accept();
    return S_OK;
}

void QWinRTFileDialogHelper::appendFile(IInspectable *file)
{
    Q_D(QWinRTFileDialogHelper);

    HRESULT hr;
    ComPtr<IStorageItem> item;
    hr = file->QueryInterface(IID_PPV_ARGS(&item));
    Q_ASSERT_SUCCEEDED(hr);

    HString path;
    hr = item->get_Path(path.GetAddressOf());
    Q_ASSERT_SUCCEEDED(hr);

    quint32 pathLen;
    const wchar_t *pathStr = path.GetRawBuffer(&pathLen);
    const QString filePath = QString::fromWCharArray(pathStr, pathLen);
    QWinRTFileEngineHandler::registerFile(filePath, item.Get());
    d->selectedFiles.append(QUrl::fromLocalFile(filePath));
}

QT_END_NAMESPACE
