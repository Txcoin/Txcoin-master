#ifndef OVERVIEWPAGE_H
#define OVERVIEWPAGE_H

#include <QWidget>
#include <QMessageBox>

namespace Ui {
    class OverviewPage;
}
class ClientModel;
class WalletModel;
class TxViewDelegate;
class TransactionFilterProxy;

QT_BEGIN_NAMESPACE
class QModelIndex;
class QLabel;
QT_END_NAMESPACE

/** Overview ("home") page widget */
class OverviewPage : public QWidget
{
    Q_OBJECT

public:
    explicit OverviewPage(QWidget *parent = 0);
    ~OverviewPage();

    void setClientModel(ClientModel *clientModel);
    void setWalletModel(WalletModel *walletModel);
    void showOutOfSyncWarning(bool fShow);

public slots:
    void setBalance(qint64 balance, qint64 unconfirmedBalance, qint64 immatureBalance);
    void setSharedBalance(qint64 sharedBalance, qint64 sharedUnconfirmedBalance, qint64 sharedImmatureBalance);

signals:
    void transactionClicked(const QModelIndex &index);

private:
    Ui::OverviewPage *ui;
    ClientModel *clientModel;
    WalletModel *walletModel;
    qint64 currentBalance;
    qint64 currentUnconfirmedBalance;
    qint64 currentImmatureBalance;
    
    qint64 currentSharedBalance;
    qint64 currentSharedUnconfirmedBalance;
    qint64 currentSharedImmatureBalance;

    QLabel *labelRankArray[16];
    QLabel *labelMsgArray[16];
    QLabel *labelFeeArray[16];

    TxViewDelegate *txdelegate;
    TransactionFilterProxy *filter;
    void updateAdvertisement();

private slots:
    void updateDisplayUnit();
    void handleTransactionClicked(const QModelIndex &index);
    void updateAlerts(const QString &warnings);
    void addAdvertisement();
    void handleAdLinkClicked(const QString &link);    
};

#endif // OVERVIEWPAGE_H
