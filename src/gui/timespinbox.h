// This file is part of RSS Guard.
//
// Copyright (C) 2011-2017 by Martin Rotter <rotter.martinos@gmail.com>
//
// RSS Guard is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// RSS Guard is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with RSS Guard. If not, see <http://www.gnu.org/licenses/>.

#ifndef TIMESPINBOX_H
#define TIMESPINBOX_H

#include <QDoubleSpinBox>


class TimeSpinBox : public QDoubleSpinBox {
		Q_OBJECT

	public:
		explicit TimeSpinBox(QWidget* parent = 0);
		virtual ~TimeSpinBox();

		double valueFromText(const QString& text) const;
		QString textFromValue(double val) const;
		void fixup(QString& input) const;
		QValidator::State validate(QString& input, int& pos) const;
};

#endif // TIMESPINBOX_H
