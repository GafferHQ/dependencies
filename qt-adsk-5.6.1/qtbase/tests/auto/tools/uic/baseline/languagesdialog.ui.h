/********************************************************************************
** Form generated from reading UI file 'languagesdialog.ui'
**
** Created by: Qt User Interface Compiler version 5.0.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef LANGUAGESDIALOG_H
#define LANGUAGESDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_LanguagesDialog
{
public:
    QVBoxLayout *verticalLayout;
    QTreeWidget *languagesList;
    QHBoxLayout *hboxLayout;
    QToolButton *upButton;
    QToolButton *downButton;
    QToolButton *removeButton;
    QToolButton *openFileButton;
    QSpacerItem *spacerItem;
    QPushButton *okButton;

    void setupUi(QDialog *LanguagesDialog)
    {
        if (LanguagesDialog->objectName().isEmpty())
            LanguagesDialog->setObjectName(QStringLiteral("LanguagesDialog"));
        LanguagesDialog->resize(400, 300);
        verticalLayout = new QVBoxLayout(LanguagesDialog);
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        languagesList = new QTreeWidget(LanguagesDialog);
        languagesList->setObjectName(QStringLiteral("languagesList"));
        languagesList->setIndentation(0);

        verticalLayout->addWidget(languagesList);

        hboxLayout = new QHBoxLayout();
        hboxLayout->setObjectName(QStringLiteral("hboxLayout"));
        upButton = new QToolButton(LanguagesDialog);
        upButton->setObjectName(QStringLiteral("upButton"));
        upButton->setEnabled(false);
        QIcon icon;
        icon.addFile(QStringLiteral(":/images/up.png"), QSize(), QIcon::Normal, QIcon::Off);
        upButton->setIcon(icon);

        hboxLayout->addWidget(upButton);

        downButton = new QToolButton(LanguagesDialog);
        downButton->setObjectName(QStringLiteral("downButton"));
        downButton->setEnabled(false);
        QIcon icon1;
        icon1.addFile(QStringLiteral(":/images/down.png"), QSize(), QIcon::Normal, QIcon::Off);
        downButton->setIcon(icon1);

        hboxLayout->addWidget(downButton);

        removeButton = new QToolButton(LanguagesDialog);
        removeButton->setObjectName(QStringLiteral("removeButton"));
        removeButton->setEnabled(false);
        QIcon icon2;
        icon2.addFile(QStringLiteral(":/images/editdelete.png"), QSize(), QIcon::Normal, QIcon::Off);
        removeButton->setIcon(icon2);

        hboxLayout->addWidget(removeButton);

        openFileButton = new QToolButton(LanguagesDialog);
        openFileButton->setObjectName(QStringLiteral("openFileButton"));
        openFileButton->setEnabled(true);
        QIcon icon3;
        icon3.addFile(QStringLiteral(":/images/mac/fileopen.png"), QSize(), QIcon::Normal, QIcon::Off);
        openFileButton->setIcon(icon3);

        hboxLayout->addWidget(openFileButton);

        spacerItem = new QSpacerItem(121, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        hboxLayout->addItem(spacerItem);

        okButton = new QPushButton(LanguagesDialog);
        okButton->setObjectName(QStringLiteral("okButton"));

        hboxLayout->addWidget(okButton);


        verticalLayout->addLayout(hboxLayout);


        retranslateUi(LanguagesDialog);
        QObject::connect(okButton, SIGNAL(clicked()), LanguagesDialog, SLOT(accept()));

        QMetaObject::connectSlotsByName(LanguagesDialog);
    } // setupUi

    void retranslateUi(QDialog *LanguagesDialog)
    {
        LanguagesDialog->setWindowTitle(QApplication::translate("LanguagesDialog", "Auxiliary Languages", 0));
        QTreeWidgetItem *___qtreewidgetitem = languagesList->headerItem();
        ___qtreewidgetitem->setText(1, QApplication::translate("LanguagesDialog", "File", 0));
        ___qtreewidgetitem->setText(0, QApplication::translate("LanguagesDialog", "Locale", 0));
#ifndef QT_NO_TOOLTIP
        upButton->setToolTip(QApplication::translate("LanguagesDialog", "<html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">\n"
"p, li { white-space: pre-wrap; }\n"
"</style></head><body style=\" font-family:'MS Shell Dlg 2'; font-size:8.25pt; font-weight:400; font-style:normal;\">\n"
"<p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Move selected language up</p></body></html>", 0));
#endif // QT_NO_TOOLTIP
        upButton->setText(QApplication::translate("LanguagesDialog", "up", 0));
#ifndef QT_NO_TOOLTIP
        downButton->setToolTip(QApplication::translate("LanguagesDialog", "<html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">\n"
"p, li { white-space: pre-wrap; }\n"
"</style></head><body style=\" font-family:'MS Shell Dlg 2'; font-size:8.25pt; font-weight:400; font-style:normal;\">\n"
"<p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px; font-size:8pt;\">Move selected language down</p></body></html>", 0));
#endif // QT_NO_TOOLTIP
        downButton->setText(QApplication::translate("LanguagesDialog", "down", 0));
#ifndef QT_NO_TOOLTIP
        removeButton->setToolTip(QApplication::translate("LanguagesDialog", "<html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">\n"
"p, li { white-space: pre-wrap; }\n"
"</style></head><body style=\" font-family:'MS Shell Dlg 2'; font-size:8.25pt; font-weight:400; font-style:normal;\">\n"
"<p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Remove selected language</p></body></html>", 0));
#endif // QT_NO_TOOLTIP
        removeButton->setText(QApplication::translate("LanguagesDialog", "remove", 0));
#ifndef QT_NO_TOOLTIP
        openFileButton->setToolTip(QApplication::translate("LanguagesDialog", "<html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">\n"
"p, li { white-space: pre-wrap; }\n"
"</style></head><body style=\" font-family:'MS Shell Dlg 2'; font-size:8.25pt; font-weight:400; font-style:normal;\">\n"
"<p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Open auxiliary language files</p></body></html>", 0));
#endif // QT_NO_TOOLTIP
        openFileButton->setText(QApplication::translate("LanguagesDialog", "...", 0));
        okButton->setText(QApplication::translate("LanguagesDialog", "OK", 0));
    } // retranslateUi

};

namespace Ui {
    class LanguagesDialog: public Ui_LanguagesDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // LANGUAGESDIALOG_H
