#pragma once

#include <QDialog>
#include <QImage>

#include <memory>

#include "ui_ThermalPrinterView.h"

namespace QGBA {

class ConfigController;
class CoreController;

class ThermalPrinterView : public QDialog {
Q_OBJECT

public:
	ThermalPrinterView(ConfigController* config, QWidget* parent = nullptr);
	~ThermalPrinterView();

	static void printToThermal(const QImage&, ConfigController*, CoreController*);

private slots:
	void saveSettings();

private:
	static int sendImage(const QImage&, const QString& pipePath, int density, float contrast, bool invert, int feedLines, int dither, int scaleMode);

	Ui::ThermalPrinterView m_ui;
	ConfigController* m_config;
};

}
