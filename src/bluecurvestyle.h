#pragma once

#include <QCommonStyle>
#include <QCache>
#include <QPainter>
#include <QStyleOptionTab>

class BluecurveStyle : public QCommonStyle
{
	Q_OBJECT

public:
	BluecurveStyle() = default;
    ~BluecurveStyle() override = default;

	void polish(QWidget *widget) override;

	void drawItemText(QPainter *painter, const QRect &rect,
					  int flags, const QPalette &pal, bool enabled,
					  const QString &text, QPalette::ColorRole textRole = QPalette::NoRole) const override;

	void drawPrimitive(PrimitiveElement pe, const QStyleOption *opt,
					   QPainter *p, const QWidget *widget = nullptr) const override;

	void drawControl(ControlElement control, const QStyleOption *opt,
					 QPainter *p, const QWidget *widget = nullptr) const override;

	QRect subElementRect(SubElement element, const QStyleOption *opt,
						 const QWidget *widget = nullptr) const override;

	void drawComplexControl(ComplexControl control, const QStyleOptionComplex *opt,
							QPainter *p, const QWidget *widget = nullptr) const override;

	QRect subControlRect(ComplexControl control, const QStyleOptionComplex *opt,
						 SubControl sc, const QWidget *widget = nullptr) const override;

	int pixelMetric(PixelMetric metric, const QStyleOption *opt = nullptr,
					const QWidget *widget = nullptr) const override;

	QSize sizeFromContents(ContentsType contents, const QStyleOption *opt,
						   const QSize &contentsSize,
						   const QWidget *widget = nullptr) const override;

	int styleHint(StyleHint sh, const QStyleOption *opt = nullptr,
				  const QWidget *widget = nullptr,
				  QStyleHintReturn *hret = nullptr) const override;

private:
	// Contains computed shade colors and radio/check pixmaps
	struct BluecurveColorData {
		QRgb buttonColor;
		QRgb spotColor;

		QColor btnShades[8]; // Shades computed using button color
		QColor bgShades[8]; // Shades computed using window color
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

	// Draws a Bluecurve style text entry rectangle
	void drawTextRect(QPainter *p, const QStyleOption *opt,
					  const QBrush *fill = nullptr) const;

	// Draws a raised/sunken Bluecurve style bevel rectangle
	void drawLightBevel(QPainter *p, const QStyleOption *opt,
						const QBrush *fill = nullptr, bool btnPal = false, bool dark = false) const;	

	// Draws a Bluecurve style gradient rectangle
	void drawGradient(QPainter *p, QRect const &rect, const QPalette &palette,
					  double shade1, double shade2, bool horiz) const;
	void drawGradientBox(QPainter *p, const QStyleOption *opt,
						 const BluecurveColorData *cdata, bool horiz,
						 double shade1, double shade2) const;

	// Adjusts tab rectangle (taken from qwindowsstyle.cpp)
	void tabLayout(const QStyleOptionTab *opt, const QWidget *widget,
				   QRect *textRect, QRect *iconRect) const; 

	// Bluecurve GTK+ 2.0 engine arrow drawing functions
	static void calculate_arrow_geometry(PrimitiveElement pe, int &x, int &y,
								  int &width, int &height);

	static void drawArrow(QPainter *p, PrimitiveElement pe, int x,
				   int y, int width, int height);

	static void arrow_draw_hline(QPainter *p, int x1, int x2,
						  int y, bool last);

	static void arrow_draw_vline(QPainter *p, int y1, int y2,
						  int x, bool last);
	
};
