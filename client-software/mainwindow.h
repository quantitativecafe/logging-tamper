#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QtBluetooth/QBluetoothDeviceDiscoveryAgent>
#include <QtBluetooth/QLowEnergyController>
#include <QtBluetooth/QLowEnergyService>
#include <QtBluetooth/QLowEnergyCharacteristic>

class QCustomPlot;
class QCPCurve;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    struct Data {
        QVector<double> time;
        QVector<double> force;
        QVector<double> displacement;
    };

public slots:
    void onDeviceDiscovered(const QBluetoothDeviceInfo &device);
    void onDeviceDiscoveryError(QBluetoothDeviceDiscoveryAgent::Error error);
    void onConnected();
    void onDisconnected();
    void onServiceDiscovered(const QBluetoothUuid &serviceUuid);
    void onServiceDiscoveryFinished();
    void onServiceStateChanged(QLowEnergyService::ServiceState newState);
    void onCharacteristicChanged(const QLowEnergyCharacteristic &characteristic, const QByteArray &newValue);
    void onErrorOccurred(QLowEnergyController::Error error);

private slots:
    void on_options_clicked();
    void on_saveCancel_clicked();
    void on_trialNumber_valueChanged(int value);

private:
    Ui::MainWindow *ui;

    double maxForce;
    double minDisplacement;
    double maxDisplacement;

    double savedMaxForce;
    double savedMinDisplacement;
    double savedMaxDisplacement;

    QBluetoothDeviceDiscoveryAgent *discoveryAgent;
    QLowEnergyController *bleController;
    QLowEnergyService *bleService;
    QLowEnergyCharacteristic dataCharacteristic;

    double triggerForceLow;
    double triggerForceHigh;

    Data liveData;
    Data savedData;

    QCPCurve *liveCurve;
    QCPCurve *savedCurve;

    bool triggered;

    QString logFolder;
    int trialNumber;

    QCPCurve *configurePlot(QCustomPlot *plot);
    void updateTrigger();
    void updatePlots();
    void updateInterface();
    void saveData();
    void writeData();
    void setTrialNumber(int val);
    void processData(const QString &data);
};
#endif // MAINWINDOW_H
