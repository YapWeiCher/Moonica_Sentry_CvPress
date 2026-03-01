#include "QDragPolygon.h"
#include "QGrabber.h"
#include <QStyleOptionGraphicsItem>

QDragPolygon::QDragPolygon() :_width(10),
_height(10),
_left(0),
_top(0),
_angle(0),
_grabSize(7),
_minWidth(10),
_minHeight(10),
_isDrawCross(false),
_isDrawText(false)
{
	setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemSendsScenePositionChanges);
	setAcceptHoverEvents(true);

	
	_colorAnimation = std::make_unique<QPropertyAnimation>(this, "animatedColor");
	_colorAnimation->setDuration(2000); // Animation duration (in ms)
	_colorAnimation->setLoopCount(-1); // Infinite looping
	_colorAnimation->setEasingCurve(QEasingCurve::SineCurve);
	
}

void QDragPolygon::setup(QPolygon polygon, QColor borderColor, QString name, int textSize, qreal grabSize)
{
	_polygon = polygon;
	_rect = _polygon.boundingRect();


	_left = 0;
	_top = 0;
	_width = _rect.width();
	_height = _rect.height();
	_isDragable = true;

	_borderColor = borderColor;
	_grabSize = grabSize;
	_name = name;

	setToolTip(name);

	setPos(_rect.topLeft());
	for (int i = 0; i < _polygon.size(); i++)
	{
		QPoint newPoint = _polygon.point(i) - this->scenePos().toPoint();
		_polygon.setPoint(i, newPoint);
	}


	_font.setPixelSize(textSize);

	emit dragBoxCreated(_rect);




}

void QDragPolygon::setColorAnimation(bool enable)
{
	_isAnimateColor = enable;
}

void QDragPolygon::setGeometry(QRectF rect)
{
	_left = 0;
	_top = 0;
	_width = rect.width();
	_height = rect.height();

	setPos(rect.topLeft());
	prepareGeometryChange();
}

void QDragPolygon::setAngle(qreal angle)
{
	_angle = angle;
}

qreal QDragPolygon::getAngle()
{
	return _angle;
}

void QDragPolygon::setName(QString name)
{
	_name = name;
	setToolTip(_name);
}

QString QDragPolygon::getName()
{
	return _name;
}

void QDragPolygon::setBorderColor(QColor color)
{
	_borderColor = color;
}

QColor QDragPolygon::getBorderColor()
{
	return _borderColor;
}

void QDragPolygon::moveTo(QPointF pos)
{
	setPos(QPointF(pos.x() - _width / 2, pos.y() - _height / 2));
}

QRectF QDragPolygon::getGeometry()
{
	return QRectF(pos().x(), pos().y(), _width, _height);
}

QPolygon QDragPolygon::getPolygon()
{
	QPolygon newPoly; 
	for (auto p : _polygon)
	{
		QPoint nPoint(p.x() + pos().x(), p.y() + pos().y());
		newPoly << nPoint;
	}

	return newPoly;
}

void QDragPolygon::setDragable(bool flag)
{
	_isDragable = flag;
	setFlag(ItemIsMovable, flag);
}

bool QDragPolygon::getDragable()
{
	return _isDragable;
}

void QDragPolygon::setDrawCross(bool flag)
{
	_isDrawCross = flag;
}

bool QDragPolygon::getDrawCross()
{
	return _isDrawCross;
}

void QDragPolygon::setDrawText(bool flag)
{
	_isDrawText = flag;
}

bool QDragPolygon::getDrawText()
{
	return _isDrawText;
}

void QDragPolygon::setOutterBarrier(const QRectF& rect)
{
	_outterBarrier = rect;
}

QRectF QDragPolygon::getOutterBarrier()
{
	return _outterBarrier;
}

bool QDragPolygon::getSelected()
{
	return _isSelected;
}

void QDragPolygon::adjustSize(int x, int y)
{
	_width += x;
	_height += y;
}

bool QDragPolygon::isGrabberMoving()
{
	bool flag = false;

	for (int i = 0; i < _corners.size(); i++)
	{
		if (_corners[i] == nullptr) break;

		if (_corners[i]->getMouseState() == GrabberMouseState::kMouseMoving)
		{
			flag = true;
			break;
		}
	}
	return flag;
}

bool QDragPolygon::sceneEventFilter(QGraphicsItem* watched, QEvent* event)
{
	// will be trggered when u hover mouse to grabber position

	QGrabber* corner = dynamic_cast<QGrabber*>(watched);
	if (corner == NULL) return false; // not expected to get here

	QGraphicsSceneMouseEvent* mevent = dynamic_cast<QGraphicsSceneMouseEvent*>(event);
	if (mevent == NULL)
	{
		// this is not one of the mouse events we are interrested in
		return false;
	}

	switch (event->type())
	{
		// if the mouse went down, record the x,y coords of the press, record it inside the corner object
	case QEvent::GraphicsSceneMousePress:
	{
		corner->setMouseState(GrabberMouseState::kMouseDown);
		corner->mouseDownX = mevent->pos().x();
		corner->mouseDownY = mevent->pos().y();



	}
	break;

	case QEvent::GraphicsSceneMouseRelease:
	{
		corner->setMouseState(GrabberMouseState::kMouseReleased);
		emit dragBoxResized(this, _name);
	}
	break;

	case QEvent::GraphicsSceneMouseMove:
	{
		corner->setMouseState(GrabberMouseState::kMouseMoving);
	}
	break;

	default:
		// we dont care about the rest of the events
		return false;
		break;
	}


	if (corner->getMouseState() == GrabberMouseState::kMouseMoving)
	{
		qreal xScene = mevent->scenePos().x();
		qreal yScene = mevent->scenePos().y();

		if (xScene < _outterBarrier.x()) xScene = _outterBarrier.x();
		if (yScene < _outterBarrier.y()) yScene = _outterBarrier.y();
		if (xScene > _outterBarrier.x() + _outterBarrier.width()) xScene = _outterBarrier.x() + _outterBarrier.width();
		if (yScene > _outterBarrier.y() + _outterBarrier.height()) yScene = _outterBarrier.y() + _outterBarrier.height();

		qreal x = xScene - this->scenePos().x();
		qreal y = yScene - this->scenePos().y();

		

		// depending on which corner has been grabbed, we want to move the position
		// of the item as it grows/shrinks accordingly. so we need to either add
		// or subtract the offsets based on which corner this is.
		
		int cornerID = corner->getCorner();

		qreal offset = _grabSize / 2 * _scale;

		_polygon.setPoint(cornerID, x, y);

		_width = _polygon.boundingRect().width();
		_height = _polygon.boundingRect().height();

		
	

		// if the mouse is being dragged, calculate a new size and also re-position
		// the box to give the appearance of dragging the corner out/in to resize the box

		setCornerPositions();

		prepareGeometryChange();
	}
	else if (corner->getMouseState() == GrabberMouseState::kMouseReleased)
	{
		QPoint translatePoint = _polygon.boundingRect().topLeft();
		//_polygon.translate(translatePoint);
		for (int i = 0; i < _polygon.size(); i++)
		{
			QPoint newPoint(_polygon.point(i).x() - translatePoint.x(), _polygon.point(i).y() - translatePoint.y());
			_polygon.setPoint(i, newPoint);
		}
		QPoint newPosPoint(scenePos().toPoint().x() + translatePoint.x(), scenePos().toPoint().y() + translatePoint.y());
		setPos(newPosPoint);
		setCornerPositions();

	}

	return true;// true => do not send event to watched - we are finished with this event
}

void QDragPolygon::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
	QGraphicsItem::mousePressEvent(event);

	emit dragBoxMousePressed(this, _name, pos());
}

void QDragPolygon::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
	QGraphicsItem::mouseReleaseEvent(event);

	emit dragBoxMouseReleased(this, _name, pos());
}

void QDragPolygon::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
	QGraphicsItem::contextMenuEvent(event);

	emit dragBoxContextMenuEvent(this, _name, pos());
}

QVariant QDragPolygon::itemChange(GraphicsItemChange change, const QVariant& value)
{
	if (change == ItemPositionChange && scene())
	{
		emit dragBoxPositionChanged(this);

		if (isGrabberMoving() == false)
		{

			bool flag = false;
			QPointF newTopLeft = value.toPointF();

			newTopLeft.rx() = (newTopLeft.x() < _outterBarrier.left()) ? _outterBarrier.left() : newTopLeft.x();
			newTopLeft.ry() = (newTopLeft.y() < _outterBarrier.top()) ? _outterBarrier.top() : newTopLeft.y();
			newTopLeft.rx() = ((newTopLeft.x() + _width) > _outterBarrier.right()) ? (_outterBarrier.right() - _width) : newTopLeft.x();
			newTopLeft.ry() = ((newTopLeft.y() + _height) > _outterBarrier.bottom()) ? (_outterBarrier.bottom() - _height) : newTopLeft.y();

			return newTopLeft;
		}
	}

	return QGraphicsItem::itemChange(change, value);
}

void QDragPolygon::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
	QGraphicsSceneMouseEvent* mevent = dynamic_cast<QGraphicsSceneMouseEvent*>(event);

	emit dragBoxMouseMoved(this);
	QGraphicsItem::mouseMoveEvent(event);
}

void QDragPolygon::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
	for (int i = 0; i < _corners.size(); i++)
	{
		_corners[i]->setParentItem(NULL);
		delete _corners[i];
		_corners[i] = nullptr;
	}
	_corners.clear();

	QRectF dragBox = QRectF(pos().x(), pos().y(), _width, _height);

	if (!_outterBarrier.contains(dragBox))
	{
		QRectF rect = _outterBarrier.intersected(dragBox);

		prepareGeometryChange();

		setPos(rect.topLeft());
		_width = rect.width();
		_height = rect.height();
	}
	

	emit dragBoxMouseHoverLeaved(this, _name);
}

void QDragPolygon::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{

	_corners.clear();
	if (_isDragable)
	{
		for (int i = 0; i < _polygon.size(); i++)
		{
			QGrabber* corner = new QGrabber(this, i, _grabSize);
			corner->installSceneEventFilter(this);
			_corners.append(corner);
		}
		

		setCornerPositions();
	}

	emit dragBoxMouseHoverEntered(this);
}

void QDragPolygon::setCornerPositions()
{
	if (_polygon.size() == _corners.size())
	{
		qreal offset = _grabSize / 2 * _scale;
		for (int i = 0; i < _corners.size(); i++)
		{
			_corners[i]->setPos(_polygon[i].x() - offset, _polygon[i].y() - offset);
			_corners[i]->setScale(_scale);

		}
	}
	/*emit dragBoxResizing(this);*/
}

QRectF QDragPolygon::boundingRect() const
{
	return QRectF(_polygon.boundingRect().x(), _polygon.boundingRect().y(), _width, _height);
}

QPainterPath QDragPolygon::shape() const
{
	// Define the precise shape of the item as the polygon
	QPainterPath path;
	path.addPolygon(_polygon);
	return path;
}

void QDragPolygon::setAnimatedColor(const QColor& color)
{
	_animatedColor = color;
	update(); // Request a repaint
}

void QDragPolygon::setPenWidth(int width)
{
	_penWidth = width;
	update();
}

void QDragPolygon::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	
	painter->setRenderHints(QPainter::Qt4CompatiblePainting);

	if (option->state & QStyle::State_Selected)
	{
		
		_isSelected = true;

		if (_isAnimateColor)
		{
			
			// Start animation when selected
			_colorAnimation->setStartValue(QColor(200, 200, 200, 128)); // white
			_colorAnimation->setEndValue(QColor(_borderColor.red(), _borderColor.green(), _borderColor.blue(), 128)); // Blue
			_colorAnimation->start();
		}
		else
		{
		
			_animatedColor = QColor(_borderColor.red(), _borderColor.green(), _borderColor.blue(), 128);
		}
	
	
		painter->setBrush(_animatedColor);
		//painter->setBrush(QColor(_borderColor.red(), _borderColor.green(), _borderColor.blue(), 128));
	}
	else
	{
	
		_isSelected = false;
		if (_isAnimateColor) _colorAnimation->stop(); // Stop animation when not selected
		painter->setBrush(Qt::NoBrush);
	}


	painter->setPen(QPen(_borderColor, _penWidth));


	painter->drawPolygon(_polygon);



	_scale = scale() / painter->transform().m11();
}