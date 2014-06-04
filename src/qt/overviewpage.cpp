#include "overviewpage.h"
#include "ui_overviewpage.h"

#include "clientmodel.h"
#include "walletmodel.h"
#include "bitcoinunits.h"
#include "optionsmodel.h"
#include "transactiontablemodel.h"
#include "transactionfilterproxy.h"
#include "guiutil.h"
#include "guiconstants.h"
#include "bitcoinrpc.h"
#include "init.h"
#include "main.h"
#include "base58.h"
#include "smalldata.h"

#include "advertisedialog.h"
#include <QAbstractItemDelegate>
#include <QPainter>
#include <QStandardItemModel>
#include <QPalette>
#include <QDesktopServices>
#include <QUrl>

using namespace json_spirit;

#define DECORATION_SIZE 64
#define NUM_ITEMS 3

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    TxViewDelegate(): QAbstractItemDelegate(), unit(BitcoinUnits::BTC)
    {

    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();

        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QRect mainRect = option.rect;
        QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));
        int xspace = DECORATION_SIZE + 8;
        int ypad = 6;
        int halfheight = (mainRect.height() - 2*ypad)/2;
        QRect amountRect(mainRect.left() + xspace, mainRect.top()+ypad, mainRect.width() - xspace, halfheight);
        QRect addressRect(mainRect.left() + xspace, mainRect.top()+ypad+halfheight, mainRect.width() - xspace, halfheight);
        icon.paint(painter, decorationRect);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
        QColor foreground = option.palette.color(QPalette::Text);
        if(value.canConvert<QBrush>())
        {
            QBrush brush = qvariant_cast<QBrush>(value);
            foreground = brush.color();
        }

        painter->setPen(foreground);
        painter->drawText(addressRect, Qt::AlignLeft|Qt::AlignVCenter, address);

        if(amount < 0)
        {
            foreground = COLOR_NEGATIVE;
        }
        else if(!confirmed)
        {
            foreground = COLOR_UNCONFIRMED;
        }
        else
        {
            foreground = option.palette.color(QPalette::Text);
        }
        painter->setPen(foreground);
        QString amountText = BitcoinUnits::formatWithUnit(unit, amount, true);
        if(!confirmed)
        {
            amountText = QString("[") + amountText + QString("]");
        }
        painter->drawText(amountRect, Qt::AlignRight|Qt::AlignVCenter, amountText);

        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(amountRect, Qt::AlignLeft|Qt::AlignVCenter, GUIUtil::dateTimeStr(date));

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        return QSize(DECORATION_SIZE, DECORATION_SIZE);
    }

    int unit;

};
#include "overviewpage.moc"

OverviewPage::OverviewPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OverviewPage),
    clientModel(0),
    walletModel(0),
    currentBalance(-1),
    currentUnconfirmedBalance(-1),
    currentImmatureBalance(-1),
    txdelegate(new TxViewDelegate()),
    filter(0)
{
    ui->setupUi(this);

    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));

    QPalette Pal(palette());
    Pal.setColor(QPalette::Background, QColor(200,200,200));
    ui->frameAd->setAutoFillBackground(true);
    ui->frameAd->setPalette(Pal);

    ui->frameAd->setHidden(true);//Official wallet don't need Ad
    ui->btnAddAD->setHidden(true);//Official wallet don't need Ad

    labelRankArray[0] = ui->labelRank0;labelRankArray[1] = ui->labelRank1;labelRankArray[2] = ui->labelRank2;labelRankArray[3] = ui->labelRank3;
    labelRankArray[4] = ui->labelRank4;labelRankArray[5] = ui->labelRank5;labelRankArray[6] = ui->labelRank6;labelRankArray[7] = ui->labelRank7;
    labelRankArray[8] = ui->labelRank8;labelRankArray[9] = ui->labelRank9;labelRankArray[10] = ui->labelRank10;labelRankArray[11] = ui->labelRank11;
    labelRankArray[12] = ui->labelRank12;labelRankArray[13] = ui->labelRank13;labelRankArray[14] = ui->labelRank14;labelRankArray[15] = ui->labelRank15;

    labelMsgArray[0] = ui->labelAd0;labelMsgArray[1] = ui->labelAd1;labelMsgArray[2] = ui->labelAd2;labelMsgArray[3] = ui->labelAd3;
    labelMsgArray[4] = ui->labelAd4;labelMsgArray[5] = ui->labelAd5;labelMsgArray[6] = ui->labelAd6;labelMsgArray[7] = ui->labelAd7;
    labelMsgArray[8] = ui->labelAd8;labelMsgArray[9] = ui->labelAd9;labelMsgArray[10] = ui->labelAd10;labelMsgArray[11] = ui->labelAd11;
    labelMsgArray[12] = ui->labelAd12;labelMsgArray[13] = ui->labelAd13;labelMsgArray[14] = ui->labelAd14;labelMsgArray[15] = ui->labelAd15;

    labelFeeArray[0] = ui->labelFee0;labelFeeArray[1] = ui->labelFee1;labelFeeArray[2] = ui->labelFee2;labelFeeArray[3] = ui->labelFee3;
    labelFeeArray[4] = ui->labelFee4;labelFeeArray[5] = ui->labelFee5;labelFeeArray[6] = ui->labelFee6;labelFeeArray[7] = ui->labelFee7;
    labelFeeArray[8] = ui->labelFee8;labelFeeArray[9] = ui->labelFee9;labelFeeArray[10] = ui->labelFee10;labelFeeArray[11] = ui->labelFee11;
    labelFeeArray[12] = ui->labelFee12;labelFeeArray[13] = ui->labelFee13;labelFeeArray[14] = ui->labelFee14;labelFeeArray[15] = ui->labelFee15;

    Pal.setColor(QPalette::Background, QColor(210,210,210));
    int i;
    for ( i = 0; i < 16; i += 2 )
    {
        labelRankArray[i+1]->setAutoFillBackground(true);
        labelRankArray[i+1]->setPalette(Pal);
        labelMsgArray[i+1]->setAutoFillBackground(true);
        labelMsgArray[i+1]->setPalette(Pal);
        labelFeeArray[i+1]->setAutoFillBackground(true);
        labelFeeArray[i+1]->setPalette(Pal);
    }
    for ( i = 1; i < 16; i ++ )
    {
        connect(labelMsgArray[i], SIGNAL(linkActivated(const QString&)), this, SLOT(handleAdLinkClicked(const QString&)));
        labelMsgArray[i]->setText("");
        labelFeeArray[i]->setText("");
        labelRankArray[i]->setText("");
    }
    
    connect(ui->btnAddAD, SIGNAL(clicked()), this, SLOT(addAdvertisement()));

    // init "out of sync" warning labels
    ui->labelWalletStatus->setText("(" + tr("out of sync") + ")");
    ui->labelSharedWalletStatus->setText("(" + tr("out of sync") + ")");
    ui->labelTransactionsStatus->setText("(" + tr("out of sync") + ")");

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);
}

void OverviewPage::handleTransactionClicked(const QModelIndex &index)
{
    if(filter)
        emit transactionClicked(filter->mapToSource(index));
}

OverviewPage::~OverviewPage()
{
    delete ui;
}

void OverviewPage::setBalance(qint64 balance, qint64 unconfirmedBalance, qint64 immatureBalance)
{
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    currentBalance = balance;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentImmatureBalance = immatureBalance;
    ui->labelBalance->setText(BitcoinUnits::formatWithUnit(unit, balance));
    ui->labelUnconfirmed->setText(BitcoinUnits::formatWithUnit(unit, unconfirmedBalance));
    ui->labelImmature->setText(BitcoinUnits::formatWithUnit(unit, immatureBalance));

    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    bool showImmature = immatureBalance != 0;
    ui->labelImmature->setVisible(showImmature);
    ui->labelImmatureText->setVisible(showImmature);
}

void OverviewPage::setSharedBalance(qint64 sharedBalance, qint64 sharedUnconfirmedBalance, qint64 sharedImmatureBalance)
{
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    currentSharedBalance = sharedBalance;
    currentSharedUnconfirmedBalance = sharedUnconfirmedBalance;
    currentSharedImmatureBalance = sharedImmatureBalance;
    ui->labelBalanceShared->setText(BitcoinUnits::formatWithUnit(unit, sharedBalance));
    ui->labelUnconfirmedShared->setText(BitcoinUnits::formatWithUnit(unit, sharedUnconfirmedBalance));
    ui->labelImmatureShared->setText(BitcoinUnits::formatWithUnit(unit, sharedImmatureBalance));

    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    bool showImmature = sharedImmatureBalance != 0;
    ui->labelImmatureShared->setVisible(showImmature);
    ui->labelImmatureTextShared->setVisible(showImmature);
}

void OverviewPage::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model)
    {
        // Show warning if this is a prerelease version
        connect(model, SIGNAL(alertsChanged(QString)), this, SLOT(updateAlerts(QString)));
        updateAlerts(model->getStatusBarWarnings());
    }
}

void OverviewPage::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if(model && model->getOptionsModel())
    {
        // Set up transaction list
        filter = new TransactionFilterProxy();
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->sort(TransactionTableModel::Status, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter);
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        setBalance(model->getBalance(), model->getUnconfirmedBalance(), model->getImmatureBalance());
        setSharedBalance(model->getSharedBalance(), model->getSharedUnconfirmedBalance(), model->getSharedImmatureBalance());
        connect(model, SIGNAL(balanceChanged(qint64, qint64, qint64)), this, SLOT(setBalance(qint64, qint64, qint64)));
        connect(model, SIGNAL(sharedBalanceChanged(qint64, qint64, qint64)), this, SLOT(setSharedBalance(qint64, qint64, qint64)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
    }

    // update the display unit, to not use the default ("BTC")
    updateDisplayUnit();
}

void OverviewPage::updateDisplayUnit()
{
    if(walletModel && walletModel->getOptionsModel())
    {
        if(currentBalance != -1)
        {
            setBalance(currentBalance, currentUnconfirmedBalance, currentImmatureBalance);
            setSharedBalance(currentSharedBalance, currentSharedUnconfirmedBalance, currentSharedImmatureBalance);
        }

        // Update txdelegate->unit with the current unit
        txdelegate->unit = walletModel->getOptionsModel()->getDisplayUnit();

        ui->listTransactions->update();
    }
}

void OverviewPage::updateAlerts(const QString &warnings)
{
    this->ui->labelAlerts->setVisible(!warnings.isEmpty());
    this->ui->labelAlerts->setText(warnings);
}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
    ui->labelWalletStatus->setVisible(fShow);
    ui->labelSharedWalletStatus->setVisible(fShow);
    ui->labelTransactionsStatus->setVisible(fShow);
    updateAdvertisement();
}

void OverviewPage::addAdvertisement()
{
    AdvertiseDialog dlg(this);
    dlg.setModel(walletModel);
    if(dlg.exec())
    {
    }
}

void OverviewPage::handleAdLinkClicked(const QString &link)
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Open Link", link,
                                QMessageBox::Yes|QMessageBox::No);

    if(reply == QMessageBox::Yes)
        QDesktopServices::openUrl(QUrl(link));
}

void OverviewPage::updateAdvertisement()
{
    //adManager.load();
    std::vector<CAdTx> adTxList;
    adManager.getAdList(adTxList);

    int i;
    for ( i = 0; i < 15; ++ i )
    {
        if ( i < adTxList.size() )
        {
            labelMsgArray[i + 1]->setText(QString::fromStdString(adTxList[i].adText));
            labelFeeArray[i + 1]->setText(QString::number((double)adTxList[i].nFee / COIN, 'f', 4));
            labelRankArray[i + 1]->setText(QString::number(nBestHeight - adTxList[i].nHeight));
        }
        else
            break;
        //{
            //labelMsgArray[i + 1]->setText("");
            //labelFeeArray[i + 1]->setText("");
            //labelRankArray[i + 1]->setText("");
        //}
    }
}

