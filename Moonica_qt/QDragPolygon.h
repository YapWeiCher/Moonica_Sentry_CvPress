#ifndef QDRAGPOLYGON_H
#define QDRAGPOLYGON_H


#include <QtWidgets>
#include <QPropertyAnimation>
#include <memory> // Required for unique_ptr


class QGrabber;


class QDragPolygon : public QObject, public QGraphicsItem
{
	Q_OBJECT
		Q_INTERFACES(QGraphicsItem)
		Q_PROPERTY(QColor animatedColor READ animatedColor WRITE setAnimatedColor)


public:
	QDragPolygon();
	void setup(QPolygon polygon, QColor borderColor = Qt::green, QString name = QString(""), int textSize = 20, qreal grabSize = 20);
	void setGeometry(QRectF rect);
	void setAngle(qreal angle);
	qreal getAngle();
	void setName(QString name);
	QString getName();
	void setBorderColor(QColor color);
	QColor getBorderColor();
	void moveTo(QPointF pos);
	QRectF getGeometry();
	QPolygon getPolygon();
	void setDragable(bool flag);
	bool getDragable();
	void setDrawCross(bool flag);
	bool getDrawCross();
	void setDrawText(bool flag);
	bool getDrawText();
	void setOutterBarrier(const QRectF& rect);
	QRectF getOutterBarrier();
	bool getSelected();

	void setColorAnimation(bool enable);
	void setPenWidth(int width);

private:

	virtual QRectF boundingRect() const override;
	virtual QPainterPath shape() const override; // this will change hover enter event (recognize polygon region)
	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
	virtual void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
	virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

	void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
	void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
	void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
	void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
	QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

	virtual bool sceneEventFilter(QGraphicsItem* watched, QEvent* event) override;

	void setCornerPositions();
	void adjustSize(int x, int y);
	bool isGrabberMoving();

	qreal _width;
	qreal _height;
	qreal _left;
	qreal _top;
	qreal _scale;
	qreal _angle = 0;
	qreal _grabSize;
	qreal _minWidth;
	qreal _minHeight;
	bool _isDragable;
	bool _isSelected;
	bool _isDrawCross;
	bool _isDrawText;
	int _penWidth;


	QFont _font;
	QColor _borderColor;
	QVector<QGrabber*> _corners;

	QString _name;
	QRectF _outterBarrier;

	// polygon
	QRectF _rect;
	QPolygon _polygon;

	bool _isAnimateColor = false;
	QColor _animatedColor;
	std::unique_ptr<QPropertyAnimation> _colorAnimation;
	QColor animatedColor() const { return _animatedColor; }
	void setAnimatedColor(const QColor& color);

signals:
	void dragBoxCreated(QRectF);
	void dragBoxMouseReleased(QDragPolygon*, QString, QPointF);
	void dragBoxMousePressed(QDragPolygon*, QString, QPointF);
	void dragBoxContextMenuEvent(QDragPolygon*, QString, QPointF);
	void dragBoxResized(QDragPolygon*, QString);
	void dragBoxMouseHoverEntered(QDragPolygon*);
	void dragBoxMouseHoverLeaved(QDragPolygon*, QString);
	void dragBoxMouseMoved(QDragPolygon*);
	void dragBoxResizing(QDragPolygon*);
	void dragBoxPositionChanged(QDragPolygon*);
};

#endif // QDRAGBOX_H