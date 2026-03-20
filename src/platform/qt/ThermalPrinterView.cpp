#include "ThermalPrinterView.h"

#include <QFile>
#include <QTimer>
#include <cstring>
#include <vector>

#include "ConfigController.h"
#include "CoreController.h"

using namespace QGBA;

ThermalPrinterView::ThermalPrinterView(ConfigController* config, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
	, m_config(config)
{
	m_ui.setupUi(this);

	// Load saved settings
	m_ui.autoprint->setChecked(m_config->getQtOption("thermalPrinter/enabled", "").toBool());
	m_ui.pipePath->setText(m_config->getQtOption("thermalPrinter/pipePath", "").toString().isEmpty()
		? "/tmp/DEVTERM_PRINTER_IN"
		: m_config->getQtOption("thermalPrinter/pipePath", "").toString());
	m_ui.density->setValue(m_config->getQtOption("thermalPrinter/density", "").isNull()
		? 8
		: m_config->getQtOption("thermalPrinter/density", "").toInt());
	m_ui.contrast->setValue(m_config->getQtOption("thermalPrinter/contrast", "").isNull()
		? 20
		: m_config->getQtOption("thermalPrinter/contrast", "").toInt());
	m_ui.feedLines->setValue(m_config->getQtOption("thermalPrinter/feedLines", "").isNull()
		? 32
		: m_config->getQtOption("thermalPrinter/feedLines", "").toInt());
	m_ui.invert->setChecked(m_config->getQtOption("thermalPrinter/invert", "").toBool());
	m_ui.instantReturn->setChecked(m_config->getQtOption("thermalPrinter/instantReturn", "").toBool());
	m_ui.scale->setCurrentIndex(m_config->getQtOption("thermalPrinter/scale").isNull()
		? 0
		: m_config->getQtOption("thermalPrinter/scale").toInt());
	m_ui.dither->setCurrentIndex(m_config->getQtOption("thermalPrinter/dither").isNull()
		? 0
		: m_config->getQtOption("thermalPrinter/dither").toInt());

	connect(m_ui.buttonBox, &QDialogButtonBox::rejected, this, &ThermalPrinterView::close);
	connect(m_ui.buttonBox, &QDialogButtonBox::accepted, this, &ThermalPrinterView::saveSettings);

	connect(m_ui.density, &QSlider::valueChanged, m_ui.densityValue, [this](int v) {
		m_ui.densityValue->setText(QString::number(v));
	});
	connect(m_ui.contrast, &QSlider::valueChanged, m_ui.contrastValue, [this](int v) {
		m_ui.contrastValue->setText(QString::number(v / 10.0, 'f', 1));
	});
}

ThermalPrinterView::~ThermalPrinterView() {
}


void ThermalPrinterView::saveSettings() {
	m_config->setQtOption("thermalPrinter/enabled", m_ui.autoprint->isChecked());
	m_config->setQtOption("thermalPrinter/pipePath", m_ui.pipePath->text());
	m_config->setQtOption("thermalPrinter/density", m_ui.density->value());
	m_config->setQtOption("thermalPrinter/contrast", m_ui.contrast->value());
	m_config->setQtOption("thermalPrinter/feedLines", m_ui.feedLines->value());
	m_config->setQtOption("thermalPrinter/invert", m_ui.invert->isChecked());
	m_config->setQtOption("thermalPrinter/instantReturn", m_ui.instantReturn->isChecked());
	m_config->setQtOption("thermalPrinter/scale", m_ui.scale->currentIndex());
	m_config->setQtOption("thermalPrinter/dither", m_ui.dither->currentIndex());
	m_config->write();
	close();
}

void ThermalPrinterView::printToThermal(const QImage& image, ConfigController* config, CoreController* controller) {
	if (!config->getQtOption("thermalPrinter/enabled").toBool()) {
		controller->endPrint();
		return;
	}

	const QString pipePath = config->getQtOption("thermalPrinter/pipePath").toString().isEmpty()
		? "/tmp/DEVTERM_PRINTER_IN"
		: config->getQtOption("thermalPrinter/pipePath").toString();
	const int density = config->getQtOption("thermalPrinter/density").isNull()
		? 8 : config->getQtOption("thermalPrinter/density").toInt();
	const float contrast = config->getQtOption("thermalPrinter/contrast").isNull()
		? 2.0f : config->getQtOption("thermalPrinter/contrast").toInt() / 10.0f;
	const bool invert = config->getQtOption("thermalPrinter/invert").toBool();
	const int feedLines = config->getQtOption("thermalPrinter/feedLines").isNull()
		? 32 : config->getQtOption("thermalPrinter/feedLines").toInt();

	const bool instantReturn = config->getQtOption("thermalPrinter/instantReturn").toBool();
	const int dither = config->getQtOption("thermalPrinter/dither").isNull()
		? 0 : config->getQtOption("thermalPrinter/dither").toInt();
	const int scaleMode = config->getQtOption("thermalPrinter/scale").isNull()
		? 0 : config->getQtOption("thermalPrinter/scale").toInt();

	int lines = sendImage(image, pipePath, density, contrast, invert, feedLines, dither, scaleMode);
	if (instantReturn) {
		controller->endPrint();
	} else {
		int delayMs = (lines * 3) + 200;
		QTimer::singleShot(delayMs, controller, &CoreController::endPrint);
	}
}

int ThermalPrinterView::sendImage(const QImage& source, const QString& pipePath, int density, float contrast, bool invert, int feedLines, int dither, int scaleMode) {
	const int printerWidth = 384;

	QImage gray;
	if (scaleMode == 0) {
		// Integer 2× scale, centred on paper width
		int scale = printerWidth / source.width();  // = 2 for 160px input
		if (scale < 1) scale = 1;
		int scaledW = source.width() * scale;
		int scaledH = source.height() * scale;
		QImage scaled = source.scaled(scaledW, scaledH, Qt::IgnoreAspectRatio, Qt::FastTransformation)
			.convertToFormat(QImage::Format_Grayscale8);
		gray = QImage(printerWidth, scaledH, QImage::Format_Grayscale8);
		gray.fill(255);
		int xOffset = (printerWidth - scaledW) / 2;
		for (int y = 0; y < scaledH; ++y) {
			memcpy(gray.scanLine(y) + xOffset, scaled.scanLine(y), scaledW);
		}
	} else {
		// Stretch to full printer width
		int newHeight = source.height() * printerWidth / source.width();
		gray = source.scaled(printerWidth, newHeight, Qt::IgnoreAspectRatio, Qt::FastTransformation)
			.convertToFormat(QImage::Format_Grayscale8);
	}
	for (int y = 0; y < gray.height(); ++y) {
		uint8_t* line = gray.scanLine(y);
		for (int x = 0; x < printerWidth; ++x) {
			float pixel = line[x] / 255.0f;
			pixel = (pixel - 0.5f) * contrast + 0.5f;
			if (pixel < 0.0f) pixel = 0.0f;
			if (pixel > 1.0f) pixel = 1.0f;
			line[x] = static_cast<uint8_t>(pixel * 255);
		}
	}

	QImage mono;
	if (dither == 3) {
		// Stucki error diffusion (kernel sum = 42)
		//         * 8 4
		//   2 4 8 4 2
		//   1 2 4 2 1
		const int w = gray.width();
		const int h = gray.height();
		std::vector<float> pixels(w * h);
		for (int y = 0; y < h; ++y) {
			const uint8_t* line = gray.constScanLine(y);
			for (int x = 0; x < w; ++x)
				pixels[y * w + x] = line[x] / 255.0f;
		}
		mono = QImage(w, h, QImage::Format_Mono);
		mono.fill(1);
		for (int y = 0; y < h; ++y) {
			uint8_t* dst = mono.scanLine(y);
			for (int x = 0; x < w; ++x) {
				float old = pixels[y * w + x];
				float newVal = old < 0.5f ? 0.0f : 1.0f;
				if (newVal < 0.5f)
					dst[x / 8] &= ~(0x80 >> (x % 8));
				float err = old - newVal;
				auto spread = [&](int nx, int ny, float factor) {
					if (nx >= 0 && nx < w && ny < h)
						pixels[ny * w + nx] = qBound(0.0f, pixels[ny * w + nx] + err * factor / 42.0f, 1.0f);
				};
				spread(x+1, y,   8); spread(x+2, y,   4);
				spread(x-2, y+1, 2); spread(x-1, y+1, 4); spread(x, y+1, 8); spread(x+1, y+1, 4); spread(x+2, y+1, 2);
				spread(x-2, y+2, 1); spread(x-1, y+2, 2); spread(x, y+2, 4); spread(x+1, y+2, 2); spread(x+2, y+2, 1);
			}
		}
	} else {
		Qt::ImageConversionFlags ditherFlag;
		switch (dither) {
		case 1:  ditherFlag = Qt::OrderedDither;  break;
		case 2:  ditherFlag = Qt::DiffuseDither;  break;
		default: ditherFlag = Qt::ThresholdDither; break;
		}
		mono = gray.convertToFormat(QImage::Format_Mono, ditherFlag);
	}
	if (invert) {
		mono.invertPixels();
	}

	QFile pipe(pipePath);
	if (!pipe.open(QIODevice::WriteOnly)) {
		return 0;
	}

	// DC2 # density
	QByteArray densityCmd;
	densityCmd.append(static_cast<char>(0x12));
	densityCmd.append(static_cast<char>(0x23));
	densityCmd.append(static_cast<char>(density));
	pipe.write(densityCmd);

	// ESC/POS GS v 0 header
	const int widthBytes = printerWidth / 8;
	const int height = mono.height();
	QByteArray header;
	header.append('\x1d');
	header.append('\x76');
	header.append('\x30');
	header.append('\x00');
	header.append(static_cast<char>(widthBytes & 0xFF));
	header.append(static_cast<char>((widthBytes >> 8) & 0xFF));
	header.append(static_cast<char>(height & 0xFF));
	header.append(static_cast<char>((height >> 8) & 0xFF));
	pipe.write(header);

	for (int y = 0; y < height; ++y) {
		pipe.write(reinterpret_cast<const char*>(mono.scanLine(y)), widthBytes);
	}

	if (feedLines > 0) {
		QByteArray feedCmd;
		feedCmd.append(static_cast<char>(0x1b));
		feedCmd.append(static_cast<char>(0x64));
		feedCmd.append(static_cast<char>(feedLines));
		pipe.write(feedCmd);
	}

	pipe.close();
	return height + feedLines;
}
