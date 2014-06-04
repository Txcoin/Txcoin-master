#ifndef ADVERTISEDIALOG_H
#define ADVERTISEDIALOG_H

#include <QDialog>

QT_BEGIN_NAMESPACE
class QLabel;
QT_END_NAMESPACE

namespace Ui {
    class AdvertiseDialog;
}

class CWallet;
class WalletModel;
class SendCoinsEntry;
class SendCoinsRecipient;
class CCoinControl;
class CTransaction;

/** Dialog for editing an address and associated information.
 */
class AdvertiseDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AdvertiseDialog(QWidget *parent = 0);
    ~AdvertiseDialog();

    void setModel(WalletModel *model);

public slots:
    void send();
    void cancel();
    void onTextChanged(const QString & text);
    void updatePreview(const QString & text);

private:
    Ui::AdvertiseDialog *ui;
    WalletModel *model;
    CWallet *wallet;
};

#endif // ADVERTISEDIALOG_H
