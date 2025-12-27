#include "bluecurvestyleplugin.h"
#include "bluecurvestyle.h"

QStyle *BluecurveStylePlugin::create(const QString &key)
{
	if (key.compare(QStringLiteral("Bluecurve"), Qt::CaseInsensitive) == 0) {
		return new BluecurveStyle();
	}

	return nullptr;
}
