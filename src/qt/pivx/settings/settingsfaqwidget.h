// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SETTINGSFAQWIDGET_H
#define SETTINGSFAQWIDGET_H

#include <QDialog>

class PIVXGUI;

namespace Ui {
class SettingsFaqWidget;
}

class SettingsFaqWidget : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsFaqWidget(PIVXGUI *parent = nullptr);
    ~SettingsFaqWidget();

    void showEvent(QShowEvent *event) override;

public Q_SLOTS:
   void windowResizeEvent(QResizeEvent* event);
   void setSection(int num);
private Q_SLOTS:
    void onFaq1Clicked();
    void onFaq2Clicked();
    void onFaq3Clicked();
    void onFaq4Clicked();
    void onFaq5Clicked();
    void onFaq6Clicked();
    void onFaq7Clicked();
    void onFaq8Clicked();
    void onFaq9Clicked();
    void onFaq10Clicked();
private:
    Ui::SettingsFaqWidget *ui;
    int pos = 0;

    std::vector<QPushButton*> getButtons();
};

#endif // SETTINGSFAQWIDGET_H
