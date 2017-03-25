// This file is part of RSS Guard.
//
// Copyright (C) 2011-2016 by Martin Rotter <rotter.martinos@gmail.com>
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

#include "core/atomparser.h"

#include "miscellaneous/textfactory.h"
#include "network-web/webfactory.h"

#include <QDomDocument>


AtomParser::AtomParser() {
}

AtomParser::~AtomParser() {
}

QList<Message> AtomParser::parseXmlData(const QString &data) {
  QList<Message> messages;
  QDomDocument xml_file;
  QDateTime current_time = QDateTime::currentDateTime();
  const QString atom_ns = QSL("http://www.w3.org/2005/Atom");

  xml_file.setContent(data, true);

  // Pull out all messages.
  QDomNodeList messages_in_xml = xml_file.elementsByTagName(QSL("entry"));

  for (int i = 0; i < messages_in_xml.size(); i++) {
    QDomNode message_item = messages_in_xml.item(i);
    Message new_message;

    // Deal with titles & descriptions.
    QString elem_title = message_item.namedItem(QSL("title")).toElement().text().simplified();
    QString elem_summary = message_item.namedItem(QSL("summary")).toElement().text();

    if (elem_summary.isEmpty()) {
      elem_summary = message_item.namedItem(QSL("content")).toElement().text();
    }

    // Now we obtained maximum of information for title & description.
    if (elem_title.isEmpty()) {
      if (elem_summary.isEmpty()) {
        // BOTH title and description are empty, skip this message.
        continue;
      }
      else {
        // Title is empty but description is not.
        new_message.m_title = WebFactory::instance()->stripTags(elem_summary.simplified());
        new_message.m_contents = elem_summary;
      }
    }
    else {
      // Title is not empty, description does not matter.
      new_message.m_title = WebFactory::instance()->stripTags(elem_title);
      new_message.m_contents = elem_summary;
    }

    // Deal with link.
    QDomNodeList elem_links = message_item.toElement().elementsByTagName(QSL("link"));

    for (int i = 0; i < elem_links.size(); i++) {
      QDomElement link = elem_links.at(i).toElement();

      if (link.attribute(QSL("rel")) == QSL("enclosure")) {
        new_message.m_enclosures.append(Enclosure(link.attribute(QSL("href")), link.attribute(QSL("type"))));

        qDebug("Adding enclosure '%s' for the message.", qPrintable(new_message.m_enclosures.last().m_url));
      }
      else {
        new_message.m_url = link.attribute(QSL("href"));
      }
    }

    if (new_message.m_url.isEmpty() && !new_message.m_enclosures.isEmpty()) {
      new_message.m_url = new_message.m_enclosures.first().m_url;
    }

    // Deal with authors.
    new_message.m_author = WebFactory::instance()->escapeHtml(message_item.namedItem(QSL("author")).namedItem(QSL("name")).toElement().text());

    // Deal with creation date.
    new_message.m_created = TextFactory::parseDateTime(message_item.namedItem(QSL("updated")).toElement().text());
    new_message.m_createdFromFeed = !new_message.m_created.isNull();

    if (!new_message.m_createdFromFeed) {
      // Date was NOT obtained from the feed, set current date as creation date for the message.
      new_message.m_created = current_time;
    }

    // WARNING: There is a difference between "" and QString() in terms of nullptr SQL values!
    // This is because of difference in QString::isNull() and QString::isEmpty(), the "" is not null
    // while QString() is.
    if (new_message.m_author.isNull()) {
      new_message.m_author = "";
    }

    if (new_message.m_url.isNull()) {
      new_message.m_url = "";
    }

    messages.append(new_message);
  }

  return messages;
}