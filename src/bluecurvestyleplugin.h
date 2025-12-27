#pragma once

#include <QStylePlugin>
#include <QStyle>
#include <QString>

class BluecurveStylePlugin : public QStylePlugin
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QStyleFactoryInterface" FILE "bluecurve.json")

public:
	QStyle *create(const QString &key) override;
};
