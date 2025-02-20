#ifndef OPTIONSDIALOG_H
#define OPTIONSDIALOG_H

#include <QDialog>

#include <mainwindow.h>

namespace Ui {
class OptionsDialog;
}

class OptionsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OptionsDialog(QWidget *parent = nullptr);
    ~OptionsDialog();

    void setTriggerForceLow(float force);
    float triggerForceLow() const;

    void setTriggerForceHigh(float force);
    float triggerForceHigh() const;

    void setLogFolder(const QString &folder);
    QString logFolder() const;

private slots:
    void on_chooseLogFolder_clicked();

private:
    Ui::OptionsDialog *ui;
};

#endif // OPTIONSDIALOG_H
