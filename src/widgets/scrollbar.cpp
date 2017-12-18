#include "widgets/scrollbar.hpp"
#include "colorscheme.hpp"
#include "widgets/helper/channelview.hpp"

#include <QDebug>
#include <QMouseEvent>
#include <QPainter>

#define MIN_THUMB_HEIGHT 10

namespace chatterino {
namespace widgets {

ScrollBar::ScrollBar(ChannelView *parent)
    : BaseWidget(parent)
    , _currentValueAnimation(this, "_currentValue")
    , _highlights(nullptr)
    , smoothScrollingSetting(SettingsManager::getInstance().enableSmoothScrolling)
{
    resize(16, 100);
    _currentValueAnimation.setDuration(250);
    _currentValueAnimation.setEasingCurve(QEasingCurve(QEasingCurve::OutCubic));

    setMouseTracking(true);
}

ScrollBar::~ScrollBar()
{
    auto highlight = _highlights;

    while (highlight != nullptr) {
        auto tmp = highlight->next;
        delete highlight;
        highlight = tmp;
    }
}

void ScrollBar::removeHighlightsWhere(std::function<bool(ScrollBarHighlight &)> func)
{
    _mutex.lock();

    ScrollBarHighlight *last = nullptr;
    ScrollBarHighlight *current = _highlights;

    while (current != nullptr) {
        if (func(*current)) {
            if (last == nullptr) {
                _highlights = current->next;
            } else {
                last->next = current->next;
            }

            auto oldCurrent = current;

            current = current->next;
            last = current;

            delete oldCurrent;
        }
    }

    _mutex.unlock();
}

void ScrollBar::addHighlight(ScrollBarHighlight *highlight)
{
    _mutex.lock();

    if (_highlights == nullptr) {
        _highlights = highlight;
    } else {
        highlight->next = _highlights->next;
        _highlights->next = highlight;
    }

    _mutex.unlock();
}

void ScrollBar::scrollToBottom()
{
    this->setDesiredValue(this->_maximum - this->getLargeChange());
}

bool ScrollBar::isAtBottom() const
{
    return this->atBottom;
}

void ScrollBar::setMaximum(qreal value)
{
    _maximum = value;

    updateScroll();
}

void ScrollBar::setMinimum(qreal value)
{
    _minimum = value;

    updateScroll();
}

void ScrollBar::setLargeChange(qreal value)
{
    _largeChange = value;

    updateScroll();
}

void ScrollBar::setSmallChange(qreal value)
{
    _smallChange = value;

    updateScroll();
}

void ScrollBar::setDesiredValue(qreal value, bool animated)
{
    animated &= this->smoothScrollingSetting.getValue();
    value = std::max(_minimum, std::min(_maximum - _largeChange, value));

    if (_desiredValue + _smoothScrollingOffset != value) {
        if (animated) {
            _currentValueAnimation.stop();
            _currentValueAnimation.setStartValue(_currentValue + _smoothScrollingOffset);

            //            if (((this->getMaximum() - this->getLargeChange()) - value) <= 0.01) {
            //                value += 1;
            //            }
            _currentValueAnimation.setEndValue(value);
            _smoothScrollingOffset = 0;
            _currentValueAnimation.start();
        } else {
            if (_currentValueAnimation.state() != QPropertyAnimation::Running) {
                _smoothScrollingOffset = 0;
                _desiredValue = value;
                _currentValueAnimation.stop();
                setCurrentValue(value);
            }
        }
    }

    this->atBottom = ((this->getMaximum() - this->getLargeChange()) - value) <= 0.01;

    _smoothScrollingOffset = 0;
    _desiredValue = value;
}

qreal ScrollBar::getMaximum() const
{
    return _maximum;
}

qreal ScrollBar::getMinimum() const
{
    return _minimum;
}

qreal ScrollBar::getLargeChange() const
{
    return _largeChange;
}

qreal ScrollBar::getSmallChange() const
{
    return _smallChange;
}

qreal ScrollBar::getDesiredValue() const
{
    return _desiredValue + _smoothScrollingOffset;
}

qreal ScrollBar::getCurrentValue() const
{
    return _currentValue;
}

void ScrollBar::offset(qreal value)
{
    if (_currentValueAnimation.state() == QPropertyAnimation::Running) {
        this->_smoothScrollingOffset += value;
    } else {
        this->setDesiredValue(this->getDesiredValue() + value);
    }
}

boost::signals2::signal<void()> &ScrollBar::getCurrentValueChanged()
{
    return _currentValueChanged;
}

void ScrollBar::setCurrentValue(qreal value)
{
    value =
        std::max(_minimum, std::min(_maximum - _largeChange, value + this->_smoothScrollingOffset));

    if (_currentValue != value) {
        _currentValue = value;

        updateScroll();
        _currentValueChanged();

        update();
    }
}

void ScrollBar::printCurrentState(const QString &prefix) const
{
    qDebug() << prefix                                         //
             << "Current value: " << this->getCurrentValue()   //
             << ". Maximum: " << this->getMaximum()            //
             << ". Minimum: " << this->getMinimum()            //
             << ". Large change: " << this->getLargeChange();  //
}

void ScrollBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), this->colorScheme.ScrollbarBG);

    painter.fillRect(QRect(0, 0, width(), _buttonHeight), this->colorScheme.ScrollbarArrow);
    painter.fillRect(QRect(0, height() - _buttonHeight, width(), _buttonHeight),
                     this->colorScheme.ScrollbarArrow);

    // mouse over thumb
    if (this->_mouseDownIndex == 2) {
        painter.fillRect(_thumbRect, this->colorScheme.ScrollbarThumbSelected);
    }
    // mouse not over thumb
    else {
        painter.fillRect(_thumbRect, this->colorScheme.ScrollbarThumb);
    }

    //    ScrollBarHighlight *highlight = highlights;

    _mutex.lock();

    //    do {
    //        painter.fillRect();
    //    } while ((highlight = highlight->next()) != nullptr);

    _mutex.unlock();
}

void ScrollBar::mouseMoveEvent(QMouseEvent *event)
{
    if (_mouseDownIndex == -1) {
        int y = event->pos().y();

        auto oldIndex = _mouseOverIndex;

        if (y < _buttonHeight) {
            _mouseOverIndex = 0;
        } else if (y < _thumbRect.y()) {
            _mouseOverIndex = 1;
        } else if (_thumbRect.contains(2, y)) {
            _mouseOverIndex = 2;
        } else if (y < height() - _buttonHeight) {
            _mouseOverIndex = 3;
        } else {
            _mouseOverIndex = 4;
        }

        if (oldIndex != _mouseOverIndex) {
            update();
        }
    } else if (_mouseDownIndex == 2) {
        int delta = event->pos().y() - _lastMousePosition.y();

        setDesiredValue(_desiredValue + (qreal)delta / _trackHeight * _maximum);
    }

    _lastMousePosition = event->pos();
}

void ScrollBar::mousePressEvent(QMouseEvent *event)
{
    int y = event->pos().y();

    if (y < _buttonHeight) {
        _mouseDownIndex = 0;
    } else if (y < _thumbRect.y()) {
        _mouseDownIndex = 1;
    } else if (_thumbRect.contains(2, y)) {
        _mouseDownIndex = 2;
    } else if (y < height() - _buttonHeight) {
        _mouseDownIndex = 3;
    } else {
        _mouseDownIndex = 4;
    }
}

void ScrollBar::mouseReleaseEvent(QMouseEvent *event)
{
    int y = event->pos().y();

    if (y < _buttonHeight) {
        if (_mouseDownIndex == 0) {
            setDesiredValue(_desiredValue - _smallChange, true);
        }
    } else if (y < _thumbRect.y()) {
        if (_mouseDownIndex == 1) {
            setDesiredValue(_desiredValue - _smallChange, true);
        }
    } else if (_thumbRect.contains(2, y)) {
        // do nothing
    } else if (y < height() - _buttonHeight) {
        if (_mouseDownIndex == 3) {
            setDesiredValue(_desiredValue + _smallChange, true);
        }
    } else {
        if (_mouseDownIndex == 4) {
            setDesiredValue(_desiredValue + _smallChange, true);
        }
    }

    _mouseDownIndex = -1;
    update();
}

void ScrollBar::leaveEvent(QEvent *)
{
    _mouseOverIndex = -1;

    update();
}

void ScrollBar::updateScroll()
{
    _trackHeight = height() - _buttonHeight - _buttonHeight - MIN_THUMB_HEIGHT - 1;

    _thumbRect = QRect(0, (int)(_currentValue / _maximum * _trackHeight) + 1 + _buttonHeight,
                       width(), (int)(_largeChange / _maximum * _trackHeight) + MIN_THUMB_HEIGHT);

    update();
}

}  // namespace widgets
}  // namespace chatterino
