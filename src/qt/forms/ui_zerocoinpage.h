/********************************************************************************
** Form generated from reading UI file 'zerocoinpage.ui'
**
** Created by: Qt User Interface Compiler version 5.9.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ZEROCOINPAGE_H
#define UI_ZEROCOINPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTableView>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ZerocoinPage
{
public:
    QVBoxLayout *verticalLayout;
    QLabel *labelExplanation;
    QTableView *tableView;
    QHBoxLayout *horizontalLayout;
    QLabel *denomination;
    QComboBox *zerocoinAmount;
    QPushButton *zerocoinMintButton;
    QPushButton *zerocoinSpendButton;
    QSpacerItem *horizontalSpacer;
    QPushButton *exportButton;

    void setupUi(QWidget *ZerocoinPage)
    {
        if (ZerocoinPage->objectName().isEmpty())
            ZerocoinPage->setObjectName(QStringLiteral("ZerocoinPage"));
        ZerocoinPage->resize(760, 380);
        verticalLayout = new QVBoxLayout(ZerocoinPage);
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        labelExplanation = new QLabel(ZerocoinPage);
        labelExplanation->setObjectName(QStringLiteral("labelExplanation"));
        labelExplanation->setTextFormat(Qt::PlainText);
        labelExplanation->setWordWrap(true);

        verticalLayout->addWidget(labelExplanation);

        tableView = new QTableView(ZerocoinPage);
        tableView->setObjectName(QStringLiteral("tableView"));
        tableView->setContextMenuPolicy(Qt::CustomContextMenu);
        tableView->setTabKeyNavigation(false);
        tableView->setAlternatingRowColors(true);
        tableView->setSelectionMode(QAbstractItemView::SingleSelection);
        tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
        tableView->setSortingEnabled(true);
        tableView->verticalHeader()->setVisible(false);

        verticalLayout->addWidget(tableView);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
        denomination = new QLabel(ZerocoinPage);
        denomination->setObjectName(QStringLiteral("denomination"));

        horizontalLayout->addWidget(denomination);

        zerocoinAmount = new QComboBox(ZerocoinPage);
        zerocoinAmount->setObjectName(QStringLiteral("zerocoinAmount"));

        horizontalLayout->addWidget(zerocoinAmount);

        zerocoinMintButton = new QPushButton(ZerocoinPage);
        zerocoinMintButton->setObjectName(QStringLiteral("zerocoinMintButton"));
        QIcon icon;
        icon.addFile(QStringLiteral("../res/icons/add.png"), QSize(), QIcon::Normal, QIcon::Off);
        zerocoinMintButton->setIcon(icon);
        zerocoinMintButton->setAutoDefault(false);

        horizontalLayout->addWidget(zerocoinMintButton);

        zerocoinSpendButton = new QPushButton(ZerocoinPage);
        zerocoinSpendButton->setObjectName(QStringLiteral("zerocoinSpendButton"));
        QIcon icon1;
        icon1.addFile(QStringLiteral("../res/icons/edit.png"), QSize(), QIcon::Normal, QIcon::Off);
        zerocoinSpendButton->setIcon(icon1);
        zerocoinSpendButton->setAutoDefault(false);

        horizontalLayout->addWidget(zerocoinSpendButton);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);

        exportButton = new QPushButton(ZerocoinPage);
        exportButton->setObjectName(QStringLiteral("exportButton"));
        QIcon icon2;
        icon2.addFile(QStringLiteral("../res/icons/export.png"), QSize(), QIcon::Normal, QIcon::Off);
        exportButton->setIcon(icon2);
        exportButton->setAutoDefault(false);

        horizontalLayout->addWidget(exportButton);


        verticalLayout->addLayout(horizontalLayout);


        retranslateUi(ZerocoinPage);

        QMetaObject::connectSlotsByName(ZerocoinPage);
    } // setupUi

    void retranslateUi(QWidget *ZerocoinPage)
    {
#ifndef QT_NO_TOOLTIP
        tableView->setToolTip(QApplication::translate("ZerocoinPage", "Right-click to edit address or label", Q_NULLPTR));
#endif // QT_NO_TOOLTIP
        denomination->setText(QApplication::translate("ZerocoinPage", "Select denomination", Q_NULLPTR));
#ifndef QT_NO_TOOLTIP
        zerocoinMintButton->setToolTip(QApplication::translate("ZerocoinPage", "Mint Zerocoin", Q_NULLPTR));
#endif // QT_NO_TOOLTIP
        zerocoinMintButton->setText(QApplication::translate("ZerocoinPage", "&Mint Zerocoin", Q_NULLPTR));
#ifndef QT_NO_TOOLTIP
        zerocoinSpendButton->setToolTip(QApplication::translate("ZerocoinPage", "Spend Zerocoin", Q_NULLPTR));
#endif // QT_NO_TOOLTIP
        zerocoinSpendButton->setText(QApplication::translate("ZerocoinPage", "&Spend Zerocoin", Q_NULLPTR));
#ifndef QT_NO_TOOLTIP
        exportButton->setToolTip(QApplication::translate("ZerocoinPage", "Export the data in the current tab to a file", Q_NULLPTR));
#endif // QT_NO_TOOLTIP
        exportButton->setText(QApplication::translate("ZerocoinPage", "&Export", Q_NULLPTR));
        Q_UNUSED(ZerocoinPage);
    } // retranslateUi

};

namespace Ui {
    class ZerocoinPage: public Ui_ZerocoinPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ZEROCOINPAGE_H
