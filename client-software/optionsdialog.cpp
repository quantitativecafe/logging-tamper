#include "optionsdialog.h"
#include "ui_optionsdialog.h"

#include <QFileDialog>

OptionsDialog::OptionsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::OptionsDialog)
{
    ui->setupUi(this);
}

OptionsDialog::~OptionsDialog()
{
    delete ui;
}

void OptionsDialog::setTriggerForceLow(float force)
{
    ui->triggerForceLow->setText(QString::number(force));
}

float OptionsDialog::triggerForceLow() const
{
    return ui->triggerForceLow->text().toFloat();
}

void OptionsDialog::setTriggerForceHigh(float force)
{
    ui->triggerForceHigh->setText(QString::number(force));
}

float OptionsDialog::triggerForceHigh() const
{
    return ui->triggerForceHigh->text().toFloat();
}

void OptionsDialog::setLogFolder(const QString &folder)
{
    ui->logFolder->setText(folder);
}

QString OptionsDialog::logFolder(void) const
{
    return ui->logFolder->text();
}

void OptionsDialog::on_chooseLogFolder_clicked()
{
    // Pick a folder
    QString dir = QFileDialog::getExistingDirectory(this, tr("Log Folder"), ui->logFolder->text());

    // If the user didn't click cancel
    if (!dir.isEmpty()) {
        ui->logFolder->setText(dir);
    }
}

