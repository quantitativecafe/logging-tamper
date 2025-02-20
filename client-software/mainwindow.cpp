#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <algorithm>

#include <QMessageBox>
#include <QSettings>

#include "optionsdialog.h"

#define SERVICE_UUID        "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID "6e400002-b5a3-f393-e0a9-e50e24dcca9e"

MainWindow::MainWindow(QWidget *parent):
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    minDisplacement(1000),
    maxDisplacement(-1000),
    savedMinDisplacement(0),
    savedMaxDisplacement(0),
    discoveryAgent(new QBluetoothDeviceDiscoveryAgent(this)),
    bleController(nullptr),
    bleService(nullptr),
    dataCharacteristic(),
    triggerForceLow(0.1),
    triggerForceHigh(1.0),
    triggered(false),
    logFolder("trials"),
    trialNumber(1)
{
    ui->setupUi(this);

    // Get persistent settings
    QSettings settings("QuantitativeCafe", "Tamper");
    triggerForceLow = settings.value("triggerForce", triggerForceLow).toFloat();
    triggerForceHigh = settings.value("triggerForceHigh", triggerForceHigh).toFloat();
    logFolder = settings.value("logFolder", logFolder).toString();
    trialNumber = settings.value("trialNumber", trialNumber).toInt();

    // Initialize plot limits
    maxForce = triggerForceLow;
    savedMaxForce = triggerForceLow;

    // Configure plots
    liveCurve = configurePlot(ui->livePlot);
    savedCurve = configurePlot(ui->savedPlot);

    // Configure saved plot
    ui->savedPlot->setBackground(QBrush(QColor(0, 0, 0, 0)));
    ui->savedPlot->axisRect()->setBackground(QBrush(QColor(255, 255, 255, 255)));

    // Configure BLE
    connect(discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered, this, &MainWindow::onDeviceDiscovered);
    connect(discoveryAgent, &QBluetoothDeviceDiscoveryAgent::errorOccurred, this, &MainWindow::onDeviceDiscoveryError);

    ui->status->append("Scanning for BLE devices...");
    discoveryAgent->start();

    // Update filename
    setTrialNumber(trialNumber);
}

MainWindow::~MainWindow()
{
    if (bleController) {
        bleController->disconnectFromDevice();
        delete bleController;
    }
    delete discoveryAgent;
    delete ui;
}

void MainWindow::onDeviceDiscovered(const QBluetoothDeviceInfo &device)
{
    if (device.name().contains("ESP32_Tamper")) { // Replace with your BLE device name
        ui->status->append("Found BLE device: " + device.name());
        discoveryAgent->stop();

        bleController = QLowEnergyController::createCentral(device, this);
        connect(bleController, &QLowEnergyController::connected, this, &MainWindow::onConnected);
        connect(bleController, &QLowEnergyController::errorOccurred, this, &MainWindow::onErrorOccurred);
        connect(bleController, &QLowEnergyController::disconnected, this, &MainWindow::onDisconnected);
        connect(bleController, &QLowEnergyController::serviceDiscovered, this, &MainWindow::onServiceDiscovered);
        connect(bleController, &QLowEnergyController::discoveryFinished, this, &MainWindow::onServiceDiscoveryFinished);

        ui->status->append("Connecting to BLE device...");
        bleController->connectToDevice();
    }
}

void MainWindow::onDeviceDiscoveryError(QBluetoothDeviceDiscoveryAgent::Error error)
{
    Q_UNUSED(error);
    ui->status->append("BLE device discovery error: " + discoveryAgent->errorString());
}

void MainWindow::onConnected()
{
    ui->status->append("Connected to BLE device");
    bleController->discoverServices();
}

void MainWindow::onDisconnected()
{
    ui->status->append("Disconnected from BLE device");
    bleService = nullptr;
    dataCharacteristic = QLowEnergyCharacteristic();
}

void MainWindow::onServiceDiscovered(const QBluetoothUuid &serviceUuid)
{
    if (serviceUuid == QBluetoothUuid(QString(SERVICE_UUID))) {
        ui->status->append("Found service: " + serviceUuid.toString());
        bleService = bleController->createServiceObject(serviceUuid, this);
        if (bleService) {
            connect(bleService, &QLowEnergyService::stateChanged, this, &MainWindow::onServiceStateChanged);
            connect(bleService, &QLowEnergyService::characteristicChanged, this, &MainWindow::onCharacteristicChanged);
            bleService->discoverDetails();
        }
    }
}

void MainWindow::onServiceDiscoveryFinished()
{
    ui->status->append("Service discovery finished");
}

void MainWindow::onServiceStateChanged(QLowEnergyService::ServiceState newState)
{
    if (newState == QLowEnergyService::RemoteServiceDiscovered) {
        dataCharacteristic = bleService->characteristic(QBluetoothUuid(QString(CHARACTERISTIC_UUID)));
        if (dataCharacteristic.isValid()) {
            ui->status->append("Found data characteristic");
            bleService->writeDescriptor(dataCharacteristic.descriptor(QBluetoothUuid(QString("00002902-0000-1000-8000-00805f9b34fb"))),
                                        QByteArray::fromHex("0100")); // Enable notifications
        }
    }
}

void MainWindow::onCharacteristicChanged(const QLowEnergyCharacteristic &characteristic, const QByteArray &newValue)
{
    if (characteristic.uuid() == QBluetoothUuid(QString(CHARACTERISTIC_UUID))) {
        QString data = QString::fromUtf8(newValue);
        processData(data);
    }
}

void MainWindow::processData(const QString &data)
{
    QStringList cols = data.split(",");

    // Skip if the wrong number of columns
    if (cols.count() != 3) return;

    // Get values
    double time = cols[0].toDouble();
    double force = cols[1].toDouble();
    double displacement = cols[2].toDouble();

    // Add to samples
    liveData.time.push_back(time);
    liveData.force.push_back(force);
    liveData.displacement.push_back(displacement);

    // Set current data
    ui->currentTime->setText(QString::number(time, 'f', 1));
    ui->currentForce->setText(QString::number(force, 'f', 3));
    ui->currentDisplacement->setText(QString::number(displacement, 'f', 2));

    // Update trigger
    updateTrigger();

    // Update plot limits
    maxForce = std::max(maxForce, force);
    minDisplacement = std::min(minDisplacement, displacement);
    maxDisplacement = std::max(maxDisplacement, displacement);

    // Update plots
    updatePlots();
}

void MainWindow::onErrorOccurred(QLowEnergyController::Error error)
{
    Q_UNUSED(error);
    ui->status->append("BLE error: " + bleController->errorString());
}

QCPCurve *MainWindow::configurePlot(QCustomPlot *plot)
{
    plot->setBackground(QBrush(QColor(0, 0, 0, 0)));
    plot->axisRect()->setBackground(QBrush(QColor(255, 255, 255, 255)));

    QCPCurve *curve = new QCPCurve(plot->xAxis, plot->yAxis);
    curve->setPen(QPen(QColor(40, 110, 255)));

    plot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    QSharedPointer<QCPAxisTickerLog> logTicker(new QCPAxisTickerLog);
    plot->xAxis->setTicker(logTicker);

    plot->xAxis->setLabel("Force (kg)");
    plot->yAxis->setLabel("Displacement (mm)");

    plot->xAxis->setRange(triggerForceLow, maxForce);
    plot->yAxis->setRange(minDisplacement, maxDisplacement);
    plot->yAxis->setRangeReversed(true);

    return curve;
}

void MainWindow::updateTrigger()
{
    // Check number of samples
    int n = liveData.time.length();
    if (n < 2) return;

    // Get last two samples
    double y1 = liveData.force[n - 2];
    double y2 = liveData.force[n - 1];

    // Check start trigger condition
    float a = (triggerForceHigh - y1) / (y2 - y1);
    if (!triggered && (0 < a) && (a <= 1)) {
        if (y2 > y1) {
            // Set trigger
            triggered = true;

            // Update interface
            updateInterface();
        }
    }

    // Check end trigger condition
    a = (triggerForceLow - y1) / (y2 - y1);
    if (triggered && (0 < a) && (a <= 1)) {
        if (y1 > y2) {
            // Save to disk
            saveData();

            // Clear trigger
            triggered = false;

            // Update interface
            updateInterface();
        }
    }
}

void MainWindow::updatePlots()
{
    // Set live data
    liveCurve->setData(liveData.time, liveData.force, liveData.displacement);

    // Set saved data
    savedCurve->setData(savedData.time, savedData.force, savedData.displacement);

    // Update plot ranges
    ui->livePlot->xAxis->setRange(triggerForceLow, maxForce);
    ui->livePlot->yAxis->setRange(minDisplacement, maxDisplacement);

    // Update plot ranges
    ui->savedPlot->xAxis->setRange(triggerForceLow, savedMaxForce);
    ui->savedPlot->yAxis->setRange(savedMinDisplacement, savedMaxDisplacement);

    // Replot
    ui->livePlot->replot();
    ui->savedPlot->replot();
}

void MainWindow::saveData()
{
    const int n = liveData.time.length();

    int start;
    for (start = n - 2; start > 0; --start) {
        if (liveData.force[start] < triggerForceLow) break;
    }

    // Return now if sample is too short
    if (n - start < 5) return;

    // Copy everything since the trigger to saved data
    savedData.time = liveData.time.mid(start);
    savedData.force = liveData.force.mid(start);
    savedData.displacement = liveData.displacement.mid(start);

    // Write data to disk
    writeData();

    // Clear samples
    liveData.time = liveData.time.mid(liveData.time.length() - 1);
    liveData.force = liveData.force.mid(liveData.force.length() - 1);
    liveData.displacement = liveData.displacement.mid(liveData.displacement.length() - 1);

    // Update plot limits
    savedMaxForce = triggerForceLow;
    savedMinDisplacement = 1000;
    savedMaxDisplacement = -1000;

    for (int i = 0; i < savedData.time.length(); ++i) {
        savedMaxForce = std::max(savedMaxForce, savedData.force[i]);
        savedMinDisplacement = std::min(savedMinDisplacement, savedData.displacement[i]);
        savedMaxDisplacement = std::max(savedMaxDisplacement, savedData.displacement[i]);
    }

    maxForce = triggerForceLow;
    minDisplacement = 1000;
    maxDisplacement = -1000;
}

void MainWindow::writeData()
{
    QFile file(ui->fileName->text());

    if (file.open(QFile::WriteOnly | QFile::NewOnly)) {
        QTextStream stream(&file);

        stream << "time,force,displacement" << Qt::endl;

        int n = savedData.time.length();
        for (int i = 0; i < n; ++i) {
            double time = savedData.time[i];
            double force = savedData.force[i];
            double displacement = savedData.displacement[i];

            stream << QString::number(time, 'f', 1) << "," <<
                      QString::number(force, 'f', 3) << "," <<
                      QString::number(displacement, 'f', 2) << Qt::endl;
        }

        file.close();

        // Increment trial number
        setTrialNumber(trialNumber + 1);
    } else {
        QMessageBox::critical(this, tr("Error"), file.errorString());
    }
}

void MainWindow::on_options_clicked()
{
    OptionsDialog dialog(this);

    // Copy current settings to dialog
    dialog.setTriggerForceLow(triggerForceLow);
    dialog.setTriggerForceHigh(triggerForceHigh);
    dialog.setLogFolder(logFolder);

    if (dialog.exec()) {
        // Update current settings
        triggerForceLow = dialog.triggerForceLow();
        triggerForceHigh = dialog.triggerForceHigh();
        logFolder = dialog.logFolder();

        // Update persistent settings
        QSettings settings("QuantitativeCafe", "Tamper");
        settings.setValue("triggerForce", triggerForceLow);
        settings.setValue("triggerForceHigh", triggerForceHigh);
        settings.setValue("logFolder", logFolder);

        // Update filename
        setTrialNumber(trialNumber);
    }
}

void MainWindow::updateInterface()
{
    if (triggered) {
        ui->saveCancel->setText("Cancel");
        ui->saveCancel->setEnabled(true);
    } else {
        ui->saveCancel->setText("Save");
        ui->saveCancel->setEnabled(!savedData.time.isEmpty());
    }

    ui->trialNumber->setEnabled(!triggered);
    ui->fileName->setEnabled(!triggered);
}

void MainWindow::setTrialNumber(int val)
{
    // Update local value
    trialNumber = val;

    // Update UI
    ui->trialNumber->setValue(trialNumber);
    ui->fileName->setText(logFolder + "/trial-" + QString::number(trialNumber) + ".csv");

    // Update persistent settings
    QSettings settings("QuantitativeCafe", "Tamper");
    settings.setValue("trialNumber", trialNumber);
}

void MainWindow::on_saveCancel_clicked()
{
    if (triggered) {
        // Clear trigger
        triggered = false;

        // Update interface
        updateInterface();

        // Update plots
        updatePlots();
    } else {
        // Write data to disk
        writeData();
    }

}

void MainWindow::on_trialNumber_valueChanged(int value)
{
    setTrialNumber(value);
}
