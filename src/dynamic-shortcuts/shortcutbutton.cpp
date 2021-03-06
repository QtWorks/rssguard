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

/******************************************************************************
Copyright (c) 2010, Artem Galichkin <doomer3d@gmail.com>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

#include "dynamic-shortcuts/shortcutbutton.h"

#include "dynamic-shortcuts/shortcutcatcher.h"

#include <QKeyEvent>


ShortcutButton::ShortcutButton(ShortcutCatcher* catcher, QWidget* parent)
	: QPushButton(parent), m_catcher(catcher) {
	setMinimumWidth(100);
}

ShortcutButton::~ShortcutButton() {
}

void ShortcutButton::keyPressEvent(QKeyEvent* event) {
	int pressed_key = event->key();

	if (pressed_key == -1) {
		m_catcher->doneRecording();
	}

	const Qt::KeyboardModifiers new_modifiers = event->modifiers() & (Qt::SHIFT | Qt::CTRL | Qt::ALT | Qt::META);

	if (!m_catcher->m_isRecording && (pressed_key == Qt::Key_Return || pressed_key == Qt::Key_Space)) {
		return;
	}

	if (!m_catcher->m_isRecording) {
		QPushButton::keyPressEvent(event);
		return;
	}

	event->accept();
	m_catcher->m_modifierKeys = new_modifiers;

	switch (pressed_key) {
		case Qt::Key_AltGr:
			return;

		case Qt::Key_Shift:
		case Qt::Key_Control:
		case Qt::Key_Alt:
		case Qt::Key_Meta:
		case Qt::Key_Menu:
			m_catcher->controlModifierlessTimout();
			m_catcher->updateDisplayShortcut();
			break;

		default:

			// We now have a valid key press.
			if (pressed_key) {
				if ((pressed_key == Qt::Key_Backtab) && (m_catcher->m_modifierKeys & Qt::SHIFT)) {
					pressed_key = Qt::Key_Tab | m_catcher->m_modifierKeys;
				}

				else {
					pressed_key |= m_catcher->m_modifierKeys;
				}

				if (m_catcher->m_numKey == 0) {
					m_catcher->m_currentSequence = QKeySequence(pressed_key);
				}

				m_catcher->m_numKey++;

				if (m_catcher->m_numKey >= 4) {
					m_catcher->doneRecording();
					return;
				}

				m_catcher->controlModifierlessTimout();
				m_catcher->updateDisplayShortcut();
			}
	}
}

void ShortcutButton::keyReleaseEvent(QKeyEvent* event) {
	if (event->key() == -1) {
		return;
	}

	if (!m_catcher->m_isRecording) {
		QPushButton::keyReleaseEvent(event);
		return;
	}

	event->accept();
	const Qt::KeyboardModifiers new_modifiers = event->modifiers() &
	                                            (Qt::SHIFT | Qt::CTRL | Qt::ALT | Qt::META);

	if (((uint) new_modifiers & m_catcher->m_modifierKeys) < m_catcher->m_modifierKeys) {
		m_catcher->m_modifierKeys = new_modifiers;
		m_catcher->controlModifierlessTimout();
		m_catcher->updateDisplayShortcut();
	}
}
