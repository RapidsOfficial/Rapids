// Copyright (c) 2019-2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DEFAULTDIALOG_H
#define DEFAULTDIALOG_H

#include <QDialog>

namespace Ui {
class DefaultDialog;
}

class DefaultDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DefaultDialog(QWidget *parent = nullptr);
    ~DefaultDialog();

    void showEvent(QShowEvent *event) override;
    void setText(const QString& title = "", const QString& message = "", const QString& okBtnText = "", const QString& cancelBtnText = "");

    bool isOk = false;

public Q_SLOTS:
    void accept() override;

private:
    Ui::DefaultDialog *ui;
protected:
    void keyPressEvent(QKeyEvent *e) override;
};

#endif // DEFAULTDIALOG_H
