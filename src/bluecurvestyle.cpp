#include "bluecurvestyle.h"

#include <algorithm>

#include <QStyleFactory>
#include <QPainter>
#include <QStyleOption>
#include <QPushButton>
#include <QPointer>
#include <QEvent>
#include <QMouseEvent>
#include <QMenu>
#include <QComboBox>
#include <QScrollBar>

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define RADIO_SIZE 13
#define CHECK_SIZE 13

#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
#define CLAMP_UCHAR(v) ((unsigned char) (CLAMP (((int)v), (int)0, (int)255)))

#include "bits.cpp"

const double BluecurveStyle::shadeFactors[8] = {1.065, 0.963, 0.896, 0.85, 0.768, 0.665, 0.4, 0.205};

static void
rgb_to_hls (double *r, double *g, double *b) {
	double min;
	double max;
	double red;
	double green;
	double blue;
	double h, l, s;
	double delta;

	red = *r;
	green = *g;
	blue = *b;

	if (red > green) {
		if (red > blue)
			max = red;
		else
			max = blue;

		if (green < blue)
			min = green;
		else
			min = blue;
	} else {
		if (green > blue)
			max = green;
		else
			max = blue;

		if (red < blue)
			min = red;
		else
			min = blue;
	}

	l = (max + min) / 2;
	s = 0;
	h = 0;

	if (max != min) {
		if (l <= 0.5)
			s = (max - min) / (max + min);
		else
			s = (max - min) / (2 - max - min);

		delta = max -min;
		if (red == max)
			h = (green - blue) / delta;
		else if (green == max)
			h = 2 + (blue - red) / delta;
		else if (blue == max)
			h = 4 + (red - green) / delta;

		h *= 60;
		if (h < 0.0)
			h += 360;
	}

	*r = h;
	*g = l;
	*b = s;
}

static void
hls_to_rgb (double *h, double *l, double *s) {
	double hue;
	double lightness;
	double saturation;
	double m1, m2;
	double r, g, b;

	lightness = *l;
	saturation = *s;

	if (lightness <= 0.5)
		m2 = lightness * (1 + saturation);
	else
		m2 = lightness + saturation - lightness * saturation;
	m1 = 2 * lightness - m2;

	if (saturation == 0) {
		*h = lightness;
		*l = lightness;
		*s = lightness;
	} else {
		hue = *h + 120;
		while (hue > 360)
			hue -= 360;
		while (hue < 0)
			hue += 360;

		if (hue < 60)
			r = m1 + (m2 - m1) * hue / 60;
		else if (hue < 180)
			r = m2;
		else if (hue < 240)
			r = m1 + (m2 - m1) * (240 - hue) / 60;
		else
			r = m1;

		hue = *h;
		while (hue > 360)
			hue -= 360;
		while (hue < 0)
			hue += 360;

		if (hue < 60)
			g = m1 + (m2 - m1) * hue / 60;
		else if (hue < 180)
			g = m2;
		else if (hue < 240)
			g = m1 + (m2 - m1) * (240 - hue) / 60;
		else
			g = m1;

		hue = *h - 120;
		while (hue > 360)
			hue -= 360;
		while (hue < 0)
			hue += 360;

		if (hue < 60)
			b = m1 + (m2 - m1) * hue / 60;
		else if (hue < 180)
			b = m2;
		else if (hue < 240)
			b = m1 + (m2 - m1) * (240 - hue) / 60;
		else
			b = m1;

		*h = r;
		*l = g;
		*s = b;
	}
}

static void
shade (const QColor &ca, QColor &cb, double k) {
	int r,g,b;
	double red;
	double green;
	double blue;

	ca.getRgb(&r, &g, &b);

	red = (double) r / 255.0;
	green = (double) g / 255.0;
	blue = (double) b / 255.0;

	rgb_to_hls (&red, &green, &blue);

	green *= k;
	if (green > 1.0)
		green = 1.0;
	else if (green < 0.0)
		green = 0.0;

	blue *= k;
	if (blue > 1.0)
		blue = 1.0;
	else if (blue < 0.0)
		blue = 0.0;

	hls_to_rgb (&red, &green, &blue);

	cb.setRgb ((int)(red*255.0), (int)(green*255.0), (int)(blue*255.0));
}

static QImage *
generate_bit (unsigned char alpha[], const QColor &color, double mult)
{
	 unsigned int r, g, b;
	 QImage *image;
	 QRgb *pixels;
	 int w, h;
	 int x, y;
  
	 r = (int) (color.red() * mult);
	 r = MIN(r, 255);
	 g = (int) (color.green() * mult);
	 g = MIN(g, 255);
	 b = (int) (color.blue() * mult);
	 b = MIN(b, 255);
  
	 image = new QImage (RADIO_SIZE, RADIO_SIZE, QImage::Format_ARGB32);

	 w = image->width();
	 h = image->height();

	 for (y=0; y < h; y++)
	 {
		  pixels = (QRgb *)image->scanLine(y);
		  for (x=0; x < w; x++)
			   pixels[x] = qRgba (r, g, b, alpha?alpha[y*w+x]:255);
	 }

	 return image;
}

static QImage *
colorize_bit (unsigned char *bit,
              unsigned char *alpha,
              const QColor  &new_color)
{
	 QImage *image;
	 double intensity;
	 int x, y;
	 const unsigned char *src, *asrc;
	 QRgb *dest;
	 int dest_rowstride;
	 int width, height;
	 unsigned char *dest_pixels;
  
	 image = new QImage (RADIO_SIZE, RADIO_SIZE, QImage::Format_ARGB32);

	 if (image == NULL)
		  return NULL;
  
	 dest_rowstride = image->bytesPerLine();
	 width = image->width();
	 height = image->height();
	 dest_pixels = image->bits();
  
	 for (y = 0; y < RADIO_SIZE; y++)
	 {
		  src = bit + y * RADIO_SIZE;
		  asrc = alpha + y * RADIO_SIZE;
		  dest = (QRgb *)image->scanLine (y);

		  for (x = 0; x < RADIO_SIZE; x++)
		  {
			   int dr, dg, db;
          
			   intensity = src[x]/ 255.0;

			   if (intensity <= 0.5)
			   {
					/* Go from black at intensity = 0.0 to new_color at intensity = 0.5 */
					dr = int((new_color.red() * intensity * 2.0));
					dg = int((new_color.green() * intensity * 2.0));
					db = int((new_color.blue() * intensity * 2.0));
			   }
			   else
			   {
					/* Go from new_color at intensity = 0.5 to white at intensity = 1.0 */
					dr = int((new_color.red() + (255 - new_color.red()) * (intensity - 0.5) * 2.0));
					dg = int((new_color.green() + (255 - new_color.green()) * (intensity - 0.5) * 2.0));
					db = int((new_color.blue() + (255 - new_color.blue()) * (intensity - 0.5) * 2.0));
			   }

			   dest[x] = qRgba (CLAMP_UCHAR (dr), CLAMP_UCHAR (dg), CLAMP_UCHAR (db), asrc[x]);
		  }
	 }

	 return image;
}

static void
composeImage (QImage *destImg, QImage *srcImg)
{
	 int w, h, x, y;
	 QRgb *src, *dest;
	 unsigned int a;
	 QRgb s, d;

	 w = destImg->width();
	 h = destImg->height();

	 for (y = 0; y < h; y++)
	 {
		  src = (QRgb *)srcImg->scanLine(y);
		  dest = (QRgb *)destImg->scanLine(y);

		  for (x = 0; x < w; x++)
		  {
			   s = src[x];
			   d = dest[x];
          
			   a = qAlpha(s);

			   dest[x] = qRgba ((qRed(s) * a + (255-a)*qRed(d)) / 255,
								(qGreen(s) * a + (255-a)*qGreen(d)) / 255,
								(qBlue(s) * a + (255-a)*qBlue(d)) / 255,
								a + ((255-a)*qAlpha(d)) / 255);
		  }
	 }
}

/* We assume this seldom collides, since we can only cache one at a time */
static long
hashColorGroup (const QPalette &palette)
{
	return palette.button().color().rgb() << 8 ^ palette.highlight().color().rgb();
}

BluecurveStyle::BluecurveColorData::~BluecurveColorData()
{
	int i;

	for (i = 0; i < 8; i++) {
		if (radioPix[i] != 0)
			delete radioPix[i];
	}
	
	if (radioMask != 0)
		delete radioMask;

	for (i = 0; i < 6; i++) {
		if (checkPix[i] != 0)
			delete checkPix[i];
	}
}

BluecurveStyle::BluecurveStyle() : QCommonStyle(), m_dataCache ()
{	
	basestyle = QStyleFactory::create("Windows"); // Original theme used MotifPlus, but this is no longer available, so we use Windows.
	if (!basestyle)
		qFatal( "BluecurveStyle: couldn't find a base style!" );
}

BluecurveStyle::~BluecurveStyle()
{
	delete basestyle;
}

void
BluecurveStyle::polish(QWidget *widget)
{
	if (qobject_cast<QAbstractButton *>(widget) ||
		qobject_cast<QComboBox *>(widget))
		widget->setAttribute(Qt::WA_Hover, true);

	if (qobject_cast<QScrollBar *>(widget) ||
		qobject_cast<QAbstractSlider *>(widget)) {
		widget->setMouseTracking(true);
		widget->setAttribute(Qt::WA_Hover, true);
	}
	
	QCommonStyle::polish(widget);
}

BluecurveStyle::BluecurveColorData *
BluecurveStyle::realizeData (const QPalette &palette) const
{
	BluecurveColorData *cdata;
	int i, j;

	cdata = new BluecurveColorData;
	cdata->buttonColor = palette.button().color().rgb();
	cdata->spotColor = palette.highlight().color().rgb();

	for (i = 0; i < 8; i++) {
		shade (palette.button().color(), cdata->shades[i], shadeFactors[i]);
	}

	shade (palette.highlight().color(), cdata->spots[0], 1.62);
	shade (palette.highlight().color(), cdata->spots[1], 1.05);
	shade (palette.highlight().color(), cdata->spots[2], 0.72);

	QImage *dot, *inconsistent, *outline, *circle, *check, *base;

	dot = colorize_bit (dot_intensity, dot_alpha, palette.highlight().color());
	outline = generate_bit (outline_alpha, cdata->shades[6], 1.0);

	QImage composite (RADIO_SIZE, RADIO_SIZE, QImage::Format_ARGB32);

	for (i = 0; i < 2; i++) {
		for (j = 0; j < 2; j++) {
			if (i == 0) {
				composite.fill (palette.button().color().rgb());
			} else {
				composite.fill (palette.midlight().color().rgb());
			}
			composeImage (&composite, outline);

			if (j == 0) {
				circle = generate_bit (circle_alpha, QColor(Qt::white), 1.0);
			} else {
				circle = generate_bit (circle_alpha, cdata->shades[1], 1.0);
			}

			composeImage (&composite, circle);
			delete circle;

			cdata->radioPix[i*4+j*2+0] = new QPixmap (QPixmap::fromImage(composite));

			composeImage (&composite, dot);
			cdata->radioPix[i*4+j*2+1] = new QPixmap (QPixmap::fromImage(composite));
		}
	}

	QImage mask = outline->createAlphaMask();
	cdata->radioMask = new QPixmap (QPixmap::fromImage(mask));

	check = generate_bit (check_alpha, palette.highlight().color(), 1.0);
	inconsistent = generate_bit (check_inconsistent_alpha, palette.highlight().color(), 1.0);

	for (i = 0; i < 2; i++) {
		if (i == 0) {
			base = generate_bit (check_base_alpha, QColor(Qt::white), 1.0);
		} else {
			base = generate_bit (check_base_alpha, cdata->shades[1], 1.0);
		}

		composite.fill (cdata->shades[6].rgb());
		composeImage (&composite, base);
		cdata->checkPix[i*3+0] = new QPixmap (QPixmap::fromImage(composite));

		composeImage (&composite, check);
		cdata->checkPix[i*3+1] = new QPixmap (QPixmap::fromImage(composite));

		composite.fill (cdata->shades[6].rgb());
		composeImage (&composite, base);
		composeImage (&composite, inconsistent);
		cdata->checkPix[i*3+2] = new QPixmap (QPixmap::fromImage(composite));

		delete base;
	}	

    // GTK check marks - 0 is highlighted, 1 is normal 
	check = generate_bit (checkmark, palette.highlightedText().color(), 1.0);
	cdata->checkMark[0] = new QPixmap (QPixmap::fromImage(*check));
	check = generate_bit (checkmark, palette.buttonText().color(), 1.0);
	cdata->checkMark[1] = new QPixmap (QPixmap::fromImage(*check));

	delete dot;
	delete inconsistent;
	delete outline;
	delete check;
	
	return cdata;
}

const BluecurveStyle::BluecurveColorData *
BluecurveStyle::lookupData (const QPalette &palette) const
{

	BluecurveColorData *cdata;
	long h;
	QCache<long, BluecurveColorData> *cache;

	h = hashColorGroup(palette);

	// Doing this cast seems very wrong but the original theme does it this way,
	// and it doesn't affect compilation... so who cares, right?
	cache = (QCache<long, BluecurveColorData> *)&m_dataCache;

	cdata = cache->object(h);

	if (cdata == 0 || !cdata->isGroup(palette)) {
		if (cdata != 0) {
			cache->remove (h);
		}

		cdata = realizeData (palette);
		cache->insert (h, cdata);
	}

	return cdata;	
	
}

void
BluecurveStyle::getShade (const QPalette &palette, int shadenr, QColor &res) const
{
	const BluecurveColorData *cdata = lookupData(palette);

	res = cdata->shades[shadenr];
}

void
BluecurveStyle::drawTextRect(QPainter *p, const QStyleOption *opt,
							 const QBrush *fill) const
{
	const QRect &r = opt->rect;
	QRect br = r;

	const BluecurveColorData *cdata = lookupData(opt->palette);

	p->setPen(cdata->shades[5]);
	p->drawRect(r);

	// button bevel
	p->setPen(opt->palette.light().color());
	p->drawLine(r.x() + r.width() - 2, r.y() + 2,
				r.x() + r.width() - 2, r.y() + r.height() - 3); // right
	p->drawLine(r.x() + 2, r.y() + r.height() - 2,
				r.x() + r.width() - 2, r.y() + r.height() - 2); // bottom

	p->setPen(cdata->shades[1]);
	p->drawLine(r.x() + 1, r.y() + 2,
				r.x() + 1, r.y() + r.height() - 2); // left
	p->drawLine(r.x() + 1, r.y() + 1,
				r.x() + r.width() - 2, r.y() + 1); // top

	br.adjust(2, 2, -2, -2);

	// fill
	if (fill) {
		p->fillRect(br, *fill);
	}
}

void
BluecurveStyle::drawLightBevel(QPainter *p, const QStyleOption *opt,
							   const QBrush *fill, bool dark) const
{
	const QRect &r = opt->rect;
	QRect br = r;
	QColor col;

	bool sunken = (opt->state & (QStyle::State_On | QStyle::State_Sunken));

	const BluecurveColorData *cdata = lookupData(opt->palette);

	p->setPen(dark ? cdata->shades[6] : cdata->shades[5]);
    p->drawRect(r.adjusted(0, 0, -1, -1));

	if (opt->state & (QStyle::State_On |
					  QStyle::State_Sunken | QStyle::State_Raised)) {
		// button bevel
		p->setPen(sunken ? Qt::white : cdata->shades[2]);
		p->drawLine(r.x() + r.width() - 2, r.y() + 2,
					r.x() + r.width() - 2, r.y() + r.height() - 3); // right
		p->drawLine(r.x() + 1, r.y() + r.height() - 2,
					r.x() + r.width() - 2, r.y() + r.height() - 2); // bottom

		p->setPen(sunken ? cdata->shades[2] : Qt::white);
		p->drawLine(r.x() + 1, r.y() + 2,
					r.x() + 1, r.y() + r.height() - 2); // left
		p->drawLine(r.x() + 1, r.y() + 1,
					r.x() + r.width() - 2, r.y() + 1); // top

		br.adjust(2, 2, -2, -2);
	} else {
		br.adjust(1, 1, -1, -1);
	}

	// fill
	if (fill) {
		p->fillRect(br, *fill);
	}
}

void
BluecurveStyle::drawPrimitive(PrimitiveElement pe, const QStyleOption *opt,
							  QPainter *p, const QWidget *widget) const
{

	// this isn't necessary, but it cleans up the code, and makes copy/pasting in Qt3 code easier...
	const QRect &r = opt->rect;

	const BluecurveColorData *cdata = lookupData(opt->palette);
	
	switch (pe) {

	case PE_IndicatorHeaderArrow: {
		// Little hack to always force State_Enabled, per the Qt3 theme
		QStyleOption optCopy(*opt);
		optCopy.state |= QStyle::State_Enabled;
		drawPrimitive((opt->state & State_UpArrow) ? PE_IndicatorArrowUp : PE_IndicatorArrowDown, &optCopy, p, widget);
		break;
	}
		
	case PE_PanelButtonCommand:
	case PE_PanelButtonBevel:
	case PE_PanelButtonTool: {
		const QBrush *fill;

		if (opt->state & QStyle::State_Enabled) {
			if (opt->state & (QStyle::State_On | QStyle::State_Sunken)) {
				fill = &opt->palette.brush(QPalette::Dark);
			} else {
				if (opt->state & QStyle::State_MouseOver)
					fill = &opt->palette.brush(QPalette::Midlight);
				else
					fill = &opt->palette.brush(QPalette::Button);
			}
			 
		} else
			fill = &opt->palette.brush(QPalette::Window);

		drawLightBevel(p, opt, fill, true);
		break;
	}

	case PE_FrameButtonBevel:
	case PE_FrameButtonTool: {
		drawLightBevel(p, opt, 0, true);
		break;
	}

	case PE_IndicatorMenuCheckMark: {
		QPoint qp = QPoint(r.center().x() - RADIO_SIZE/4, 
						   r.center().y() - RADIO_SIZE/2);
		if (opt->state & (State_Selected | State_Open))
			p->drawPixmap(qp, *(cdata->checkMark[0]));
		else
			p->drawPixmap(qp, *(cdata->checkMark[1]));

		break;		
	}

	case PE_IndicatorCheckBox: {
		int pix = 0;
		if (opt->state & State_Sunken)
			pix += 3;
			
		if (opt->state & State_On)
			pix += 1;
		else if (opt->state & State_NoChange)
			pix += 2;

		p->drawPixmap(r.topLeft(), *cdata->checkPix[pix]);
		break;
	}

	case PE_IndicatorRadioButton: {
		int pix = 0;
		if (opt->state & State_MouseOver) {
			pix += 4;
			p->fillRect(r, opt->palette.brush(QPalette::Midlight));
		} else {
			p->fillRect(r, opt->palette.brush(QPalette::Window));
		}

		if (opt->state & State_Sunken)
			pix += 2;
		if (opt->state & State_On)
			pix += 1;

		p->drawPixmap(r.topLeft(), *cdata->radioPix[pix]);

		break;			
	}

	case PE_IndicatorToolBarHandle: {
		QString title;
		if ( p && p->device()->devType() == QInternal::Widget ) {
			QWidget *w = (QWidget *) p->device();
			QWidget *p = w->parentWidget();
		}

		QStyle::State state = opt->state;
		state |= State_Raised;

		if (state & State_Horizontal) {
			p->fillRect(r, opt->palette.button().color());

			int yy = r.top() + 3;
			int nLines = (r.height() - 6) / 5;

			for (int i = 0; i < nLines; yy += 5, i++) {
				p->setPen(cdata->shades[5]);
				p->drawLine(1, yy + 3, 4, yy);
				p->setPen(opt->palette.light().color());
				p->drawLine(1, yy + 4, 4, yy + 1);
			}			
		} else {
			p->fillRect(r, opt->palette.button().color());

			int xx = r.left() + 3;
			int nLines = (r.width() - 5) / 4;

			for (int i = 0; i < nLines; xx += 4, i++) {
				p->setPen(cdata->shades[5]);
				p->drawLine(xx + 3, 1,
							xx, 1 + 3);
				p->setPen(opt->palette.light().color());
				p->drawLine(xx + 3, 2,
							xx + 1, 1 + 3);
			}
		}

		break;
	}

	case PE_IndicatorToolBarSeparator: {
		if (r.width() > 20 || r.height() > 20) {
			if (opt->state & State_Horizontal) {
				p->setPen(cdata->shades[5]);
				p->drawLine(r.left() + 1, r.top() + 6, r.left() + 1, r.bottom() - 6);
				p->setPen(cdata->shades[3]);
				p->drawLine(r.left() + 2, r.top() + 6, r.left() + 2, r.bottom() - 6);
			} else {
				p->setPen(cdata->shades[5]);
				p->drawLine(r.left() + 6, r.top() + 1, r.right() - 6, r.top() + 1);
				p->setPen(cdata->shades[3]);
				p->drawLine(r.left() + 6, r.top() + 2, r.right() - 6, r.top() + 2);
			}
		} else {
		    QCommonStyle::drawPrimitive(pe, opt, p, widget);
		}
		break;		
	}

	case PE_IndicatorDockWidgetResizeHandle: {
		p->fillRect(r, opt->palette.window().color());
		if (opt->state & State_Horizontal) {
			p->setPen(opt->palette.highlight().color().lighter());
			p->drawLine(r.left() + 1, r.top() + 1, r.right() - 1, r.top() + 1);
			p->setPen(opt->palette.highlight().color());
			p->drawLine(r.left() + 1, r.top() + 2, r.right() - 1, r.top() + 2);
			p->setPen(opt->palette.highlight().color().darker());
			p->drawLine(r.left() + 1, r.top() + 3, r.right() - 1, r.top() + 3);
		} else {
			p->setPen(opt->palette.highlight().color().lighter());
			p->drawLine(r.left() + 1, r.top() + 1, r.left() + 1, r.bottom() - 1);
			p->setPen(opt->palette.highlight().color());
			p->drawLine(r.left() + 2, r.top() + 1, r.left() + 2, r.bottom() - 1);
			p->setPen(opt->palette.highlight().color().darker());
			p->drawLine(r.left() + 3, r.top() + 1, r.left() + 3, r.bottom() - 1);
		}
		break;		
	}

		/*case PE_PanelLineEdit: {  // TODO: fix this
		drawTextRect(p, r, palette, flags);
		break;
		}*/		

	case PE_FrameTabWidget: {
		p->setPen(cdata->shades[6]);
		p->drawLine(r.left(),r.top(),r.left(),r.bottom());
		p->drawLine(r.left(),r.bottom(),r.right(),r.bottom());
		p->drawLine(r.right(),r.bottom(),r.right(),r.top());
		p->drawLine(r.left(),r.top(),r.right(),r.top());
		p->setPen(opt->palette.light().color());
		p->drawLine(r.left()+1,r.top(),r.left()+1,r.bottom()-1);
		p->drawLine(r.left()+1,r.bottom()-1,r.right()-1,r.bottom()-1);
		p->drawLine(r.right()-1,r.bottom()-1,r.right()-1,r.top()+1);
		p->drawLine(r.left(),r.top()+1,r.right()-1,r.top()+1);
		break;        
	}		

	case PE_Frame:
	case PE_FrameWindow: {
		QStyleOption optCopy(*opt);
		if ( ! (optCopy.state & State_Sunken ) )
			optCopy.state |= State_Raised;
			
		drawLightBevel(p, &optCopy);
		break;
	}

	case PE_FrameMenu: {
		drawLightBevel(p, opt);
		break;
	}

	case PE_FrameDockWidget:
	case PE_PanelMenuBar: {
		p->fillRect(r, opt->palette.button().color());
		p->setPen(opt->palette.mid().color());
		p->drawLine(r.left(), r.bottom(), r.right(), r.bottom());
		break;
	}

	case PE_FrameFocusRect: {
		p->setPen(Qt::black);
		int rw = r.width(), rh = r.height(), rx = r.x(), ry = r.y();
		for (int x = 0; x < rw; x += 2) { // Top line
			p->drawPoint(rx + x, ry);
		}
		for (int y = 0; y < rh; y += 2) { // Left line
			p->drawPoint(rx, ry + y);
		}
		if (rw & 1) { // Odd width, top line one pixel short
			for (int y = 2; y < rh; y += 2) { // Right line
				p->drawPoint(rx + rw - 1, ry + y);
			}
		} else { // Even width, top line symmetrical
			for (int y = 1; y < rh; y += 2) { // Right line
				p->drawPoint(rx + rw - 1, ry + y);
			}
		}
		if (rh & 1) { // Odd height
			for (int x = 0; x < rw; x += 2) { // Bottom line
				p->drawPoint(rx + x, ry + rh - 1);
			}
		} else {
			for (int x = 1; x < rw; x += 2) { // Bottom line
				p->drawPoint(rx + x, ry + rh - 1);
			}
		}
		break;
	}

	case PE_IndicatorProgressChunk: {
		drawGradientBox(p, r, opt->palette, cdata, false, 0.92,1.66);
		break;
	}

	case PE_IndicatorArrowUp:
	case PE_IndicatorArrowDown:
	case PE_IndicatorArrowRight:
	case PE_IndicatorArrowLeft: {
		QPolygon a;

		switch ( pe ) {
		case PE_IndicatorArrowUp: {
			a.setPoints(11,   3,1,  0,-2,  -3,1,  -2,0,  -2,2,  0,-1,  2,1,  2,2,  0,0,  -1,1,  1,1);
			break;
		}

		case PE_IndicatorArrowDown: {
			a.setPoints(11,   3,-1,  0,2,  -3,-1,  -2,0,  -2,-2,  0,1,  2,-1,  2,-2,  0,0,  -1,-1,  1,-1);
			break;
		}

		case PE_IndicatorArrowRight: {
			a.setPoints(13,  0,-3,  -1,-2,  1,-2,  2,-1,  0,-1,  1,0,  3,0,  2,1,  0,1,  -1,2,  1,2,  0,3,  0,0);
			break;
		}

		case PE_IndicatorArrowLeft: {
			a.setPoints(13,  0,-3,  1,-2,  -1,-2,  -2,-1,  0,-1,  -1,0,  -3,0,  -2,1,  0,1,  1,2,  -1,2,  0,3,  0,0);
			break;
		}

		default: {
			break;
		}
		}

		if (a.isEmpty())
			return;

		p->save();
		a.translate( (r.x() + r.width() / 2), 
					 (r.y() + r.height() / 2));
		
		if ( opt->state & State_Enabled )
			p->setPen( opt->state & (State_Selected | State_Open | State_Sunken | State_MouseOver) ? opt->palette.highlightedText().color() : opt->palette.buttonText().color());
		else
			p->setPen(cdata->shades[7]);

		p->drawPolyline(a);
		p->restore();		
		
		break;
	}

	case PE_IndicatorSpinUp:
	case PE_IndicatorSpinDown: {
		p->setPen(cdata->shades[7]);
		p->setBrush(Qt::NoBrush);
		QPolygon a;
		if (pe==PE_IndicatorSpinUp)
			a.setPoints(8,  -3,0,  -1,-2,  1,0,  1,1,  -1,-1,  -3,1,  -3,0,  1,0);
		else
			a.setPoints(8,  -3,-1,  -1,1,  1,-1,  1,-2,  -1,0,  -3,-2,  -3,-1,  1,-1);
		
		a.translate(r.x() + r.width() / 2, r.y() + r.height() / 2 + 1);
		p->drawPolyline(a);
		break;
		}		
		
	default: {
		QCommonStyle::drawPrimitive(pe, opt, p, widget);
		break;
	}
	}
}

void
BluecurveStyle::drawControl(ControlElement control, const QStyleOption *opt,
							QPainter *p, const QWidget *widget) const
{
	// Quick Qt6 > Qt3 argument conversion
	const QRect &r = opt->rect;
	const QPalette &palette = opt->palette;
	QStyle::State flags = opt->state;
	
	const BluecurveColorData *cdata = lookupData(opt->palette);
	
	switch (control) {
		/*case CE_PushButtonLabel: {
		const QPushButton *button = (const QPushButton *) widget;
		const QStyleOptionButton *buttonOpt = (const QStyleOptionButton *) opt; // Needed to check button options
		QRect ir = r;

		if (button->isDown() || button->isChecked()) {
			flags |= State_Sunken;
			ir.translate(pixelMetric(PM_ButtonShiftHorizontal, opt, widget), pixelMetric(PM_ButtonShiftVertical, opt, widget));
		}

		if (buttonOpt->features & QStyleOptionButton::HasMenu) {
			int mbi = pixelMetric(PM_MenuButtonIndicator, opt, widget);
			QRect ar(ir.right() - mbi, ir.y() + 2, mbi - 4, ir.height() - 4);

			QStyleOption arrowOpt; // need to create a new styleoption for the arrow
			arrowOpt.rect = ar;
			arrowOpt.state = flags;
			arrowOpt.palette = palette;
			
		    drawPrimitive(PE_IndicatorArrowDown, &arrowOpt, p, widget);
			ir.setWidth(ir.width() - mbi);
		}

		int tf = Qt::AlignVCenter | Qt::TextShowMnemonic;
		//if (!styleHint(SH_UnderlineAccelerator, widget, QStyleOption::Default, 0))
		//tf |= NoAccel; seems to be entirely deprecated in Qt6, so we ignore it...

		if (!button->icon().isNull()) {
			QIcon::Mode mode =
				button->isEnabled() ? QIcon::Normal : QIcon::Disabled;
			if ( mode == QIcon::Normal && button->hasFocus() )
				mode = QIcon::Active;

			QIcon::State state = QIcon::Off;
			if ( button->isCheckable() && button->isChecked() )
				state = QIcon::On;

			QPixmap pixmap = button->icon().pixmap( pixelMetric(PM_SmallIconSize, opt, widget), mode, state );
			int pixw = pixmap.width();
			int pixh = pixmap.height();
			
                // content rectangle is pixmap + spacer + text bounding rectangle

			int csp = -2; // content spacer (between pixmap and text)
			QFontMetrics fm = p->fontMetrics();
			int contw;
			if (!button->text().isEmpty())
				contw = pixw + fm.horizontalAdvance(button->text()) + csp; // width of pixmap + text
			else
				contw = pixw;
			int contx = ir.x() + ir.width()/2 - contw/2; // upper left corner of content rectangle

			//Center the icon if there is no text
			if ( button->text().isEmpty() )
				p->drawPixmap( ir.x() + ir.width() / 2 - pixw / 2, ir.y() + ir.height() / 2 - pixh / 2, pixmap );
			else 
				p->drawPixmap( contx, ir.y() + ir.height() / 2 - pixh / 2, pixmap );
			ir.setLeft(contx);
			ir.setWidth(contw);
			
			// right-align text in the content rectangle, if there is any
			if (!button->text().isEmpty())
				tf |= Qt::AlignRight;		
		} else {
			tf |= Qt::AlignHCenter;
		}

		drawItemText(p, ir, tf, palette, (flags & State_Enabled), button->text(), QPalette::ButtonText);

		if (flags & State_HasFocus) {
			QStyleOption focusRectOpt;
			focusRectOpt.rect = subElementRect(SE_PushButtonFocusRect, opt, widget);
			focusRectOpt.state = flags;
			focusRectOpt.palette = palette;
			drawPrimitive(PE_FrameFocusRect, &focusRectOpt, p, widget);
		}
		
		break;
		}*/

	case CE_TabBarTabShape: {
		const QTabBar* tb = static_cast<const QTabBar*>(widget);
		bool below = false;
		QRect tr(r);
		QRect fr(r);

		tr.adjust(0, 0,  0, -1); // NB: adjust replaces addCoords
		fr.adjust(1, 2, -2, -2);

		if ( tb->shape() == QTabBar::RoundedSouth || tb->shape() == QTabBar::TriangularSouth) {
			tr = r; tr.adjust(0, 1, 0, 0);
			fr = r; fr.adjust(2, 2,-2, -2);
			below=true;
		}

		if (tr.left() != 0) {
			// ensure tabs borders overlap
			tr.adjust(-1,0,0,0);
			fr.adjust(-1,0,0,0);
		}

		if (! (opt->state & State_Selected)) {
			if (below) {
				tr.adjust(0, 0, 0, -1);
				fr.adjust(0, 0, 0, -1);
			} else {
				tr.adjust(0, 1, 0, 0);
				fr.adjust(0, 1, 0, 0);
			}
			
			if (tr.left() == 0)
				fr.adjust(1,0,0,0);

			p->setPen(cdata->shades[6]);
			p->drawRect(tr.adjusted(0,0,-1,-1));

			if (tr.left() == 0)
				if (below)
					p->drawPoint(tr.left(), tr.top() - 1);
				else
					p->drawPoint(tr.left(), tr.bottom() + 1);

			p->setPen(palette.light().color());
			if (!below)
				p->drawLine(tr.left() + 1, tr.top() + 1, tr.right() - 1, tr.top() + 1);
			if (tr.left() == 0)
				p->drawLine(tr.left() + 1, tr.top() + 1, tr.left() + 1, tr.bottom() - 1);

			p->setPen(cdata->shades[2]);
			p->drawLine(tr.right() - 1, tr.top() + 1, tr.right() - 1, tr.bottom() - 1);
			if (below)
				p->drawLine(tr.left() + 2, tr.bottom() - 1, tr.right() - 1, tr.bottom() - 1);

		} else {

			fr.adjust(1,0,0,2); // ensure tab goes over the tab contents
			if (tr.left() != 0) {
				// selected tab borders move over to the left by 1px
				tr.adjust(-1,0,0,0);
				fr.adjust(-1,0,0,0);
			}
			
			p->setPen(cdata->shades[6]);
			if (below) {
				p->drawLine(tr.left(), tr.bottom() - 1,	tr.left(), tr.top() - 1);
				p->drawLine(tr.left(), tr.bottom(),	tr.right(), tr.bottom());
				p->drawLine(tr.right(), tr.top(), tr.right(), tr.bottom() - 1);
			} else {
				p->drawLine(tr.left(), tr.bottom() + 1, tr.left(), tr.top() + 1);
				p->drawLine(tr.left(), tr.top(), tr.right(), tr.top());
				p->drawLine(tr.right(), tr.top() + 1, tr.right(), tr.bottom());
			}

			p->setPen(palette.light().color());
			if (below) {
				p->drawLine(tr.left() + 1, tr.bottom() - 1,	tr.left() + 1, tr.top() - 2);
				p->drawPoint(tr.right(), tr.top() - 1);
			} else {
				p->drawLine(tr.left() + 1, tr.bottom() + 2,	tr.left() + 1, tr.top() + 2);
				if (tr.left() != 0)
					p->drawPoint(tr.left(), tr.bottom() + 1);

				p->drawLine(tr.left() + 1, tr.top() + 1, tr.right() - 2, tr.top() + 1);
			}

			p->setPen(cdata->shades[2]);
			if (below) {
				if (tr.left() != 0)
					p->drawPoint(tr.left(), tr.top() - 1);
				p->drawLine(tr.left() + 2, tr.bottom() - 1,	tr.right() - 1, tr.bottom() - 1);
				p->drawLine(tr.right() - 1, tr.top() - 1, tr.right() - 1, tr.bottom() - 2);
			} else {
				p->drawLine(tr.right() - 1, tr.top() + 1, tr.right() - 1, tr.bottom() + 1);
			}
		}		

		p->fillRect(fr, ((opt->state & State_Selected) ?	palette.window().color() : palette.dark().color()));
		break;
	}

	case CE_TabBarTabLabel: {
		const QStyleOptionTab *tabOpt = qstyleoption_cast<const QStyleOptionTab *>(opt);
		
		QRect tr = r;

		int alignment = Qt::AlignLeft | Qt::TextShowMnemonic;

		// Qt3 stylehint relied only on the style hint itself, so use nullptr for the other arguments
		if (!styleHint(SH_UnderlineShortcut, nullptr, nullptr, nullptr))
			alignment |= Qt::TextHideMnemonic;

		// Move the text bounding rectangle to place the text correctly
		tr.translate(5, 2);
		drawItemText(p, tr, alignment, opt->palette, opt->state & State_Enabled, tabOpt->text, QPalette::ButtonText);
		// Now move it back for the focus rectangle (yes, it's a a hack)
		tr.translate(-2, -2);		
		break;
	}

	case CE_MenuItem: {
		const QStyleOptionMenuItem *miOpt = qstyleoption_cast<const QStyleOptionMenuItem *>(opt);
		if (!widget)
			break;

		const QMenu* menu = static_cast<const QMenu*>(widget);
		int tab = miOpt->reservedShortcutWidth;
		int maxpmw = miOpt->maxIconWidth;

		if ( miOpt && miOpt->menuItemType == QStyleOptionMenuItem::Separator ) {
			// draw separator
			p->fillRect(r, opt->palette.brush(QPalette::Button));
			p->setPen(cdata->shades[2]);
			p->drawLine(r.left() + 6, r.top() + 4, r.right() - 6, r.top() + 4);
			p->setPen(opt->palette.light().color());
			p->drawLine(r.left() + 6,  r.top() + 5, r.right() - 6, r.top() + 5);
			break;
		}

		if ((opt->state & (State_Selected | State_Open)) && (opt->state & State_Enabled)) {
			drawGradientBox(p, r, opt->palette, cdata, false, 0.9, 1.2);
		} else {
			p->fillRect(r, opt->palette.brush(QPalette::Button));
		}

		if (!miOpt)
			break;

		maxpmw = std::max(maxpmw, 22);

		QRect cr, ir, tr, sr;
		// check column
		cr.setRect(r.left(), r.top(), maxpmw, r.height());
		// submenu indicator column
		sr.setCoords(r.right() - 12, r.top(), r.right(), r.bottom());
		// tab/accelerator column
		tr.setCoords(r.right() - tab - 4, r.top(), r.right() - 4, r.bottom());
		// item column
		ir.setCoords(cr.right() + 4, r.top(), tr.right() - 4, r.bottom());

		bool reverse = QGuiApplication::isRightToLeft();
		if ( reverse ) {
			cr = visualRect( opt->direction, r, cr );
			sr = visualRect( opt->direction, r, sr );
			tr = visualRect( opt->direction, r, tr );
			ir = visualRect( opt->direction, r, ir );
		}

		if (!miOpt->icon.isNull()) {
			QIcon::Mode mode =
				(opt->state & State_Enabled) ? QIcon::Normal : QIcon::Disabled;
			if ((opt->state & (State_Selected | State_Open)) && (opt->state & State_Enabled))
				mode = QIcon::Active;
			QPixmap pixmap;
			if (miOpt->menuHasCheckableItems && miOpt->checked)
			    pixmap = miOpt->icon.pixmap(pixelMetric(PM_SmallIconSize), mode, QIcon::On);
			else
				pixmap = miOpt->icon.pixmap(pixelMetric(PM_SmallIconSize), mode);
			QRect pmr(QPoint(0, 0), pixmap.size());
			pmr.moveCenter(cr.center());
			p->setPen(opt->palette.text().color());
			p->drawPixmap(pmr.topLeft(), pixmap);
		} else if (miOpt->menuHasCheckableItems && miOpt->checked) {
			QStyleOption checkOpt;
			checkOpt.state = opt->state & (State_Enabled|State_Selected|State_Open);
			checkOpt.rect = cr;
			checkOpt.palette = opt->palette;
			drawPrimitive(PE_IndicatorMenuCheckMark, &checkOpt, p, widget);
		}
		QColor textcolor;
		QColor embosscolor;
		if (opt->state & (QStyle::State_Selected | QStyle::State_Open)) {
			if (! (opt->state & State_Enabled)) {
				textcolor = opt->palette.text().color();
				embosscolor = opt->palette.light().color();
			} else {
				textcolor = opt->palette.highlightedText().color();
				embosscolor = opt->palette.midlight().color().lighter();
			}
		} else if (! (opt->state & State_Enabled)) {
			textcolor = palette.text().color();
			embosscolor = palette.light().color();
		} else
			textcolor = embosscolor = palette.buttonText().color();			
		p->setPen(textcolor);

		// mi->custom() no longer exists in Qt6, I don't believe there to be a modern equivalent
		// but if there is one, I will add the code here, in line with the Qt3 theme.

		QString text = miOpt->text;
		if (! text.isNull()) {
			int t = (int)text.indexOf('\t');

			// draw accelerator/tab-text
			if (t >= 0) {
				int alignFlag = Qt::AlignVCenter | Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine;
				alignFlag |= ( reverse ? Qt::AlignLeft : Qt::AlignRight );
				if (! (opt->state & State_Enabled)) {
					p->setPen(embosscolor);
					tr.translate(1, 1);
					p->drawText(tr, alignFlag, text.mid(t + 1));
					tr.translate(-1, -1);
					p->setPen(textcolor);
				}

				p->drawText(tr, alignFlag, text.mid(t + 1));
			}

			int alignFlag = Qt::AlignVCenter | Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine;
			alignFlag |= ( reverse ? Qt::AlignLeft : Qt::AlignRight );

			if (! (opt->state & State_Enabled)) {
				p->setPen(embosscolor);
				ir.translate(1, 1);
				p->drawText(ir, alignFlag, text);
				ir.translate(-1, -1);
				p->setPen(textcolor);
			}

			p->drawText(ir, alignFlag, text);
		}
		
	}
		
	default: {
		QCommonStyle::drawControl(control, opt, p, widget);
		break;
	}
	}
	
}

int
BluecurveStyle::styleHint(StyleHint sh, const QStyleOption *opt,
						  const QWidget *widget,
						  QStyleHintReturn *hret) const
{
	int ret;
	
	switch(sh) {
	case SH_EtchDisabledText:
	case SH_Slider_SnapToValue:
	case SH_PrintDialog_RightAlignButtons:
	case SH_FontDialog_SelectAssociatedText:
	case SH_MenuBar_AltKeyNavigation:
	case SH_MenuBar_MouseTracking:
	case SH_Menu_MouseTracking:
	case SH_Menu_SpaceActivatesItem:
	case SH_ComboBox_ListMouseTracking:
	case SH_ScrollBar_MiddleClickAbsolutePosition: {
		ret = 1;
		break;
	}

	case SH_MainWindow_SpaceBelowMenuBar:
	case SH_Menu_AllowActiveAndDisabled: {
		ret = 0;
		break;
	}
	default: {
		ret = QCommonStyle::styleHint(sh, opt, widget, hret);
		break;
	}
	}

	return ret;
}

void
BluecurveStyle::drawGradient(QPainter *p, QRect const &rect,
							 const QPalette &palette,
							 double shade1, double shade2,
							 bool horiz) const
{
	QColor c, c1, c2;
	int r, g, b;
	int c2r, c2g, c2b;
	int dr, dg, db, size;
	int start, end, left, right, top, bottom;

	left = rect.left();
	top = rect.top();
	bottom = rect.bottom();
	right = rect.right();

	start = horiz ? left : top;
	end = horiz ? right : bottom;

	if (end == start)
		return;

	shade (palette.highlight().color(), c1, shade1);
	shade (palette.highlight().color(), c2, shade2);

	c1.getRgb(&r, &g, &b);
	c2.getRgb(&c2r, &c2g, &c2b);

	size = end - start;
	dr = (c2r - r) / size;
	dg = (c2g - g) / size;
	db = (c2b - b) / size;

	for (int i = start; i <= end; i++) {
		c.setRgb (r, g, b);
		p->setPen(c);
		if (horiz)
			p->drawLine(i, top, i, bottom);
		else
			p->drawLine(left, i, right, i);

		r += dr;
		g += dg;
		b += db;
	}	

}

void
BluecurveStyle::drawGradientBox(QPainter *p, QRect const &r,
								const QPalette &palette,
								const BluecurveColorData *cdata,
								bool horiz,
								double shade1, double shade2) const
{
	QRect grad(r.left()+2, r.top()+2, r.width()-3, r.height()-3);
	drawGradient(p, grad, palette, shade1, shade2, horiz);

// 3d border effect...
	p->setPen(cdata->spots[2]);
	p->setBrush(Qt::NoBrush);
	p->drawRect(r.adjusted(0,0,-1,-1));

//	We draw the bottom and right lines first ...
	p->setPen(cdata->spots[1]);
	p->drawLine(r.left()+1, r.bottom()-1, r.right()-1, r.bottom()-1);
	p->drawLine(r.right()-1, r.top()+1, r.right()-1, r.bottom()-1);

//	Because the lighter lines should overlap them on the corner pixels
	p->setPen(cdata->spots[0]);
	p->drawLine(r.left()+1, r.top()+1, r.right()-1, r.top()+1);
	p->drawLine(r.left()+1, r.top()+1, r.left()+1, r.bottom()-1);
}
