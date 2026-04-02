/*
  Qt 5/6 plugin interface for Bluecurve style.
  Copyright (c) 2025-2026 neeeeow (https://github.com/neeeeow/Bluecurve-Qt).
  
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "bluecurvestyleplugin.h"
#include "bluecurvestyle.h"

QStyle *BluecurveStylePlugin::create(const QString &key)
{
	if (key.compare(QStringLiteral("Bluecurve"), Qt::CaseInsensitive) == 0) {
		return new BluecurveStyle();
	}

	return nullptr;
}
