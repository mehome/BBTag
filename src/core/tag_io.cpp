#include <core/tag_io.h>

#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QTextCodec>


const QString TagIO::DATASET = "dataset";
const QString TagIO::NAME = "name";
const QString TagIO::COMMENT = "comment";
const QString TagIO::TAGS = "tags";
const QString TagIO::SINGLE_TAG = "tag";
const QString TagIO::LABEL = "label";
const QString TagIO::COLOR = "color";
const QString TagIO::IMAGES = "images";
const QString TagIO::SINGLE_IMAGE = "image";
const QString TagIO::PATH = "path";
const QString TagIO::BOX = "box";
const QString TagIO::TOP = "top";
const QString TagIO::LEFT = "left";
const QString TagIO::WIDTH = "width";
const QString TagIO::HEIGHT = "height";


void TagIO::write(
        QIODevice* out,
        const QHash<QString, QColor>& tag_color_dict,
        const QHash< QString, QList<TagItem::Elements> >& elts
    )
{
    if( !out ) {
        return;
    }

    QXmlStreamWriter xml;
    xml.setAutoFormatting( true );
    xml.setDevice( out );

    xml.writeStartDocument();
    xml.writeStartElement( DATASET );
    xml.writeTextElement( NAME, "dataset containing bounding box labels on images" );
    xml.writeTextElement( COMMENT, "created by BBTag" );

    xml.writeStartElement( TAGS );
    for( QHash<QString, QColor>::const_iterator c_itr = tag_color_dict.begin(); c_itr != tag_color_dict.end(); ++c_itr ) {
        xml.writeEmptyElement( SINGLE_TAG );
        xml.writeAttribute( NAME, c_itr.key() );
        xml.writeAttribute( COLOR, c_itr.value().name() );
    }
    xml.writeEndElement();

    xml.writeStartElement( IMAGES );

    for( QHash< QString, QList<TagItem::Elements> >::const_iterator img_itr = elts.begin(); img_itr != elts.end(); ++img_itr ) {
        QString fullpath = img_itr.key();
        const QList<TagItem::Elements>& tags = img_itr.value();

        xml.writeStartElement( SINGLE_IMAGE );
        xml.writeAttribute( PATH, fullpath );

        for( QList<TagItem::Elements>::const_iterator tag_itr = tags.begin(); tag_itr != tags.end(); ++tag_itr ) {
            const TagItem::Elements& elt = *tag_itr;
            const QList<QRect>& bboxes = elt._bbox;

            for( QList<QRect>::const_iterator bbox_itr = bboxes.begin(); bbox_itr != bboxes.end(); ++bbox_itr ) {
                const QRect& bbox = *bbox_itr;

                xml.writeStartElement( BOX );
                xml.writeAttribute( TOP, QString::number( bbox.top() ) );
                xml.writeAttribute( LEFT, QString::number( bbox.left() ) );
                xml.writeAttribute( WIDTH, QString::number( bbox.width() ) );
                xml.writeAttribute( HEIGHT, QString::number( bbox.height() ) );
                xml.writeTextElement( LABEL, elt._label );
                xml.writeEndElement();
            }
        }

        xml.writeEndElement();
    }

    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndDocument();
}

bool TagIO::read(
        QIODevice* in,
        QHash< QString, QList<TagItem::Elements> >& elts
    )
{
    if( !in ) {
        return false;
    }

    QXmlStreamReader xml;
    xml.setDevice( in );

    if( xml.readNext() == QXmlStreamReader::StartDocument && !xml.isStartDocument() ) {
        return false;
    }

    if( !xml.readNextStartElement() ) {
        return false;
    }

    if( xml.name() != DATASET ) {
        return false;
    }

    // skip info for user
    while( xml.readNextStartElement() && ( xml.name() != IMAGES && xml.name() != TAGS ) ) {
        xml.skipCurrentElement();
    }

    QHash<QString, QColor> tag_color_dict;
    if( xml.name() == TAGS ) {
        while( xml.readNextStartElement() && xml.name() == SINGLE_TAG ) {
            tag_color_dict[ xml.attributes().value( NAME ).toString() ] = QColor( xml.attributes().value( COLOR ).toString() );
            // empty elements are directly followed by endElement
            // so readNext must be called
            xml.readNextStartElement();
        }
    }

    // within the "images" element
    while( !xml.atEnd() ) {
        if( xml.name() == SINGLE_IMAGE ) {
            QString fullpath = xml.attributes().value( PATH ).toString();
            if( fullpath.isEmpty() ) {
                xml.skipCurrentElement();
                continue;
            }

            if( !xml.readNextStartElement() ) {
                xml.skipCurrentElement();
                continue;
            }

            while( xml.name() == BOX ) {
                QXmlStreamAttributes att =  xml.attributes();
                QStringRef top = att.value( TOP );
                QStringRef left = att.value( LEFT );
                QStringRef width = att.value( WIDTH );
                QStringRef height = att.value( HEIGHT );

                bool skip = ( top.isEmpty() || left.isEmpty() || width.isEmpty() || height.isEmpty() );
                xml.readNextStartElement();
                skip = skip || ( xml.name() != LABEL );
                QString label = xml.readElementText();
                skip = skip || ( label.isEmpty() );

                if( skip ) {
                    xml.skipCurrentElement();
                    xml.readNextStartElement();
                    continue;
                }

                TagItem::Elements elt;
                elt._fullpath = fullpath;
                elt._label = label;
                elt._color = tag_color_dict.value( label );
                elt._bbox.append( QRect( left.toInt(), top.toInt(), width.toInt(), height.toInt() ) );
                elts[ fullpath ].append( elt );

                xml.readNextStartElement();
                while( xml.isEndElement() ) {
                    xml.readNextStartElement();
                }
            }
        } else {
            xml.readNextStartElement();
        }
    }

    return true;
}
