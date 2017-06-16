/********************************************************************************
** Form generated from reading UI file 'qprintwidget.ui'
**
** Created by: Qt User Interface Compiler version 5.0.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef QPRINTWIDGET_H
#define QPRINTWIDGET_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_QPrintWidget
{
public:
    QHBoxLayout *horizontalLayout_2;
    QGroupBox *printerGroup;
    QGridLayout *gridLayout;
    QLabel *label;
    QComboBox *printers;
    QPushButton *properties;
    QLabel *label_2;
    QLabel *location;
    QCheckBox *preview;
    QLabel *label_3;
    QLabel *type;
    QLabel *lOutput;
    QHBoxLayout *horizontalLayout;
    QLineEdit *filename;
    QToolButton *fileBrowser;

    void setupUi(QWidget *QPrintWidget)
    {
        if (QPrintWidget->objectName().isEmpty())
            QPrintWidget->setObjectName(QStringLiteral("QPrintWidget"));
        QPrintWidget->resize(443, 175);
        horizontalLayout_2 = new QHBoxLayout(QPrintWidget);
        horizontalLayout_2->setContentsMargins(0, 0, 0, 0);
        horizontalLayout_2->setObjectName(QStringLiteral("horizontalLayout_2"));
        printerGroup = new QGroupBox(QPrintWidget);
        printerGroup->setObjectName(QStringLiteral("printerGroup"));
        gridLayout = new QGridLayout(printerGroup);
        gridLayout->setObjectName(QStringLiteral("gridLayout"));
        label = new QLabel(printerGroup);
        label->setObjectName(QStringLiteral("label"));

        gridLayout->addWidget(label, 0, 0, 1, 1);

        printers = new QComboBox(printerGroup);
        printers->setObjectName(QStringLiteral("printers"));
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(3);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(printers->sizePolicy().hasHeightForWidth());
        printers->setSizePolicy(sizePolicy);

        gridLayout->addWidget(printers, 0, 1, 1, 1);

        properties = new QPushButton(printerGroup);
        properties->setObjectName(QStringLiteral("properties"));
        QSizePolicy sizePolicy1(QSizePolicy::Minimum, QSizePolicy::Fixed);
        sizePolicy1.setHorizontalStretch(1);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(properties->sizePolicy().hasHeightForWidth());
        properties->setSizePolicy(sizePolicy1);

        gridLayout->addWidget(properties, 0, 2, 1, 1);

        label_2 = new QLabel(printerGroup);
        label_2->setObjectName(QStringLiteral("label_2"));

        gridLayout->addWidget(label_2, 1, 0, 1, 1);

        location = new QLabel(printerGroup);
        location->setObjectName(QStringLiteral("location"));

        gridLayout->addWidget(location, 1, 1, 1, 1);

        preview = new QCheckBox(printerGroup);
        preview->setObjectName(QStringLiteral("preview"));

        gridLayout->addWidget(preview, 1, 2, 1, 1);

        label_3 = new QLabel(printerGroup);
        label_3->setObjectName(QStringLiteral("label_3"));

        gridLayout->addWidget(label_3, 2, 0, 1, 1);

        type = new QLabel(printerGroup);
        type->setObjectName(QStringLiteral("type"));

        gridLayout->addWidget(type, 2, 1, 1, 1);

        lOutput = new QLabel(printerGroup);
        lOutput->setObjectName(QStringLiteral("lOutput"));

        gridLayout->addWidget(lOutput, 3, 0, 1, 1);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
        filename = new QLineEdit(printerGroup);
        filename->setObjectName(QStringLiteral("filename"));

        horizontalLayout->addWidget(filename);

        fileBrowser = new QToolButton(printerGroup);
        fileBrowser->setObjectName(QStringLiteral("fileBrowser"));

        horizontalLayout->addWidget(fileBrowser);


        gridLayout->addLayout(horizontalLayout, 3, 1, 1, 2);


        horizontalLayout_2->addWidget(printerGroup);

#ifndef QT_NO_SHORTCUT
        label->setBuddy(printers);
        lOutput->setBuddy(filename);
#endif // QT_NO_SHORTCUT

        retranslateUi(QPrintWidget);

        QMetaObject::connectSlotsByName(QPrintWidget);
    } // setupUi

    void retranslateUi(QWidget *QPrintWidget)
    {
        QPrintWidget->setWindowTitle(QApplication::translate("QPrintWidget", "Form", 0));
        printerGroup->setTitle(QApplication::translate("QPrintWidget", "Printer", 0));
        label->setText(QApplication::translate("QPrintWidget", "&Name:", 0));
        properties->setText(QApplication::translate("QPrintWidget", "P&roperties", 0));
        label_2->setText(QApplication::translate("QPrintWidget", "Location:", 0));
        preview->setText(QApplication::translate("QPrintWidget", "Preview", 0));
        label_3->setText(QApplication::translate("QPrintWidget", "Type:", 0));
        lOutput->setText(QApplication::translate("QPrintWidget", "Output &file:", 0));
        fileBrowser->setText(QApplication::translate("QPrintWidget", "...", 0));
    } // retranslateUi

};

namespace Ui {
    class QPrintWidget: public Ui_QPrintWidget {};
} // namespace Ui

QT_END_NAMESPACE

#endif // QPRINTWIDGET_H
