#pragma once

#include <QCommonStyle>
#include <QCache>

class BluecurveStyle : public QCommonStyle
{
	Q_OBJECT
	QStyle *basestyle;

public:
	BluecurveStyle();
    virtual ~BluecurveStyle();

	void polish(QWidget *widget);

	void drawPrimitive(PrimitiveElement pe, const QStyleOption *opt,
					   QPainter *p, const QWidget *widget) const override;

	void drawControl(ControlElement control, const QStyleOption *opt,
					 QPainter *p, const QWidget *widget) const override;

	int styleHint(StyleHint sh, const QStyleOption *opt, const QWidget *widget,
				  QStyleHintReturn *hret) const override;

private:
	struct BluecurveColorData {
		QRgb buttonColor;
		QRgb spotColor;

		QColor shades[8];
		QColor spots[3];

		QPixmap *radioPix[8];
		QPixmap *radioMask;

		QPixmap *checkPix[6];
      
		QPixmap *checkMark[2];

		~BluecurveColorData();
	  
		bool isGroup (const QPalette &palette) {
			return palette.button().color().rgb() == buttonColor && palette.highlight().color().rgb() == spotColor;
		}
	};

	QCache<long, BluecurveColorData> m_dataCache;
	static const double shadeFactors[8];

	BluecurveColorData *realizeData (const QPalette &palette) const;
	const BluecurveColorData *lookupData (const QPalette &palette) const;

	void getShade (const QPalette &palette, int shadenr, QColor &res) const;

	void drawTextRect(QPainter *p, const QStyleOption *opt,
					  const QBrush *fill) const;

	void drawLightBevel(QPainter *p, const QStyleOption *opt,
						const QBrush *fill = 0, bool dark = false) const;	

	void drawGradient(QPainter *p, QRect const &rect, const QPalette &palette,
					  double shade1, double shade2, bool horiz) const;

	void drawGradientBox(QPainter *p, QRect const &r, const QPalette &palette,
						 const BluecurveColorData *cdata, bool horiz,
						 double shade1, double shade2) const;	
	
};
