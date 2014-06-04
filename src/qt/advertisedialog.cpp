#include "advertisedialog.h"
#include "ui_advertisedialog.h"

#include "addresstablemodel.h"

#include "bitcoinrpc.h"
#include "wallet.h"
#include "walletmodel.h"
#include "bitcoinunits.h"
#include "addressbookpage.h"
#include "optionsmodel.h"
#include "sendcoinsentry.h"
#include "guiutil.h"
#include "askpassphrasedialog.h"
#include "base58.h"
#include "init.h"
#include "coincontrol.h"
#include "smalldata.h"
using namespace json_spirit;

#include <QSortFilterProxyModel>
#include <QClipboard>
#include <QMessageBox>
#include <QMenu>
#include <QTextDocument>
#include <QScrollBar>
#include <QFile>
#include <QTextStream>
#include <QDataWidgetMapper>
#include <QPalette>

AdvertiseDialog::AdvertiseDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AdvertiseDialog)
{
    ui->setupUi(this);

    connect(ui->btnSend, SIGNAL(clicked()), this, SLOT(send()));
    connect(ui->btnCancel, SIGNAL(clicked()), this, SLOT(cancel()));

    connect(ui->lineEdit, SIGNAL(textChanged( const QString &)), this, SLOT(onTextChanged(const QString &)));

    QPalette Pal(palette());
    Pal.setColor(QPalette::Background, QColor(200,200,200));
    ui->labelPreview->setAutoFillBackground(true);
    ui->labelPreview->setPalette(Pal);
}

AdvertiseDialog::~AdvertiseDialog()
{
    delete ui;
}

void AdvertiseDialog::setModel(WalletModel *model)
{
    this->model = model;
    this->wallet = model->getWallet();
}

void AdvertiseDialog::send()
{
    bool retval = true;
    if(!ui->lineEditFee->validate())
        retval = false;

    if(!ui->lineEdit->hasAcceptableInput() || 0 == ui->lineEdit->text().length())
    {
        ui->lineEdit->setValid(false);
        retval = false;
    }

    if (!retval)
        return;

    QString txmsg = ui->lineEdit->text();
    if ( std::strlen(txmsg.toStdString().c_str()) > 244 )
    {
        QMessageBox::question(this, tr("Send error"),
                              tr("Text length exceeds the limit (244 bytes)!"),
              QMessageBox::Cancel,
              QMessageBox::Cancel);

        return;
    }

    {
        LOCK2(cs_main, wallet->cs_wallet);

        // Sendmany
        std::vector<std::pair<CScript, int64> > vecSend;

        const char* msg = ui->lineEdit->text().toStdString().c_str();
        CScript scriptMsg;
        const unsigned char *msgHeader = GetSmallDataHeader(SMALLDATA_TYPE_BROADCAST);
        std::vector<unsigned char> vMsg;
        int i;
        for ( i = 0; i < 4; ++ i )
            vMsg.push_back(msgHeader[i]);
        for ( i = 0; i < std::strlen(msg); ++ i )
            vMsg.push_back(msg[i]);

        scriptMsg << OP_RETURN << vMsg;
        vecSend.push_back(make_pair(scriptMsg, 0));

        CTransaction txNew;
        int64 nFeeRequired = ui->lineEditFee->value();
        std::string strFailReason;
        CReserveKey reservekey(wallet);
        bool fCreated = wallet->CreateRawTransaction(vecSend, txNew, nFeeRequired, strFailReason, false, reservekey, NULL);

        if(!fCreated)
        {
            QMessageBox::warning(this, tr("Send AD"),
                tr("Error: Transaction creation failed!"),
                QMessageBox::Ok, QMessageBox::Ok);
            return;
        }

        uint256 hashTx = txNew.GetHash();

        bool fHave = false;
        CCoinsViewCache &view = *pcoinsTip;
        CCoins existingCoins;
        {
            fHave = view.GetCoins(hashTx, existingCoins);
            if (!fHave) {
                // push to local node
                CValidationState state;
                if (!txNew.AcceptToMemoryPool(state, true, false))
                    return;
            }
        }
        if (fHave) {
            return;
        } else {
            SyncWithWallets(hashTx, txNew, NULL, true);
        }
        RelayTransaction(txNew, hashTx);
        reservekey.KeepKey();

        accept();
    }    
}

void AdvertiseDialog::cancel()
{
    reject();
}

void AdvertiseDialog::updatePreview(const QString & text)
{
    ui->labelPreview->setText(text);
}

void AdvertiseDialog::onTextChanged(const QString & text)
{
    updatePreview(text);
}


