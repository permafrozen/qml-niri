#include <algorithm>
#include <limits>
#include <QDebug>
#include <QJsonArray>
#include "icon.h"
#include "logging.h"
#include "windowmodel.h"

WindowModel::WindowModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

WindowModel::~WindowModel()
{
    qDeleteAll(m_windows);
}

int WindowModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_windows.count();
}

QVariant WindowModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_windows.count())
        return QVariant();

    const Window *win = m_windows.at(index.row());

    switch (role) {
    case IdRole:
        return QVariant::fromValue(win->id);
    case TitleRole:
        return win->title;
    case AppIdRole:
        return win->appId;
    case PidRole:
        return win->pid;
    case WorkspaceIdRole:
        return QVariant::fromValue(win->workspaceId);
    case IsFocusedRole:
        return win->isFocused;
    case IsFloatingRole:
        return win->isFloating;
    case IsUrgentRole:
        return win->isUrgent;
    case ColumnIndexRole:
        return win->columnIndex;
    case TileIndexRole:
        return win->tileIndex;
    case TileWidthRole:
        return win->tileWidth;
    case TileHeightRole:
        return win->tileHeight;
    case WindowWidthRole:
        return win->windowWidth;
    case WindowHeightRole:
        return win->windowHeight;
    case TilePosXRole:
        return win->tilePosX;
    case TilePosYRole:
        return win->tilePosY;
    case WindowOffsetXRole:
        return win->windowOffsetX;
    case WindowOffsetYRole:
        return win->windowOffsetY;
    case IconPathRole:
        return win->iconPath;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> WindowModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[TitleRole] = "title";
    roles[AppIdRole] = "appId";
    roles[PidRole] = "pid";
    roles[WorkspaceIdRole] = "workspaceId";
    roles[IsFocusedRole] = "isFocused";
    roles[IsFloatingRole] = "isFloating";
    roles[IsUrgentRole] = "isUrgent";
    roles[ColumnIndexRole] = "columnIndex";
    roles[TileIndexRole] = "tileIndex";
    roles[TileWidthRole] = "tileWidth";
    roles[TileHeightRole] = "tileHeight";
    roles[WindowWidthRole] = "windowWidth";
    roles[WindowHeightRole] = "windowHeight";
    roles[TilePosXRole] = "tilePosX";
    roles[TilePosYRole] = "tilePosY";
    roles[WindowOffsetXRole] = "windowOffsetX";
    roles[WindowOffsetYRole] = "windowOffsetY";
    roles[IconPathRole] = "iconPath";
    return roles;
}

Window* WindowModel::focusedWindow() const
{
    for (Window *win : m_windows) {
        if (win->isFocused)
            return win;
    }
    return nullptr;
}

void WindowModel::handleEvent(const QJsonObject &event)
{
    if (event.contains("WindowsChanged")) {
        QJsonArray windows = event["WindowsChanged"].toObject()["windows"].toArray();
        handleWindowsChanged(windows);
    }
    else if (event.contains("WindowOpenedOrChanged")) {
        QJsonObject window = event["WindowOpenedOrChanged"].toObject()["window"].toObject();
        handleWindowOpenedOrChanged(window);
    }
    else if (event.contains("WindowClosed")) {
        quint64 id = event["WindowClosed"].toObject()["id"].toInteger();
        handleWindowClosed(id);
    }
    else if (event.contains("WindowFocusChanged")) {
        QJsonValue id = event["WindowFocusChanged"].toObject()["id"];
        handleWindowFocusChanged(id);
    }
    else if (event.contains("WindowUrgencyChanged")) {
        QJsonObject data = event["WindowUrgencyChanged"].toObject();
        quint64 id = data["id"].toInteger();
        bool urgent = data["urgent"].toBool();
        handleWindowUrgencyChanged(id, urgent);
    }
    else if (event.contains("WindowLayoutsChanged")) {
        QJsonArray changes = event["WindowLayoutsChanged"].toObject()["changes"].toArray();
        handleWindowLayoutsChanged(changes);
    }
}

void WindowModel::handleWindowsChanged(const QJsonArray &windows)
{
    beginResetModel();
    qDeleteAll(m_windows);
    m_windows.clear();

    for (const QJsonValue &value : windows) {
        if (value.isObject()) {
            m_windows.append(parseWindow(value.toObject()));
        }
    }

    // Sort by window ID for consistent ordering
    std::sort(m_windows.begin(), m_windows.end(),
              [](const Window *a, const Window *b) {
                return a->id < b->id;
              });

    endResetModel();
    emit countChanged();
    emit focusedWindowChanged();
}

void WindowModel::handleWindowOpenedOrChanged(const QJsonObject &windowObj)
{
    Window *window = parseWindow(windowObj);
    int idx = findWindowIndex(window->id);

    if (idx == -1) {
        // New window
        beginInsertRows(QModelIndex(), m_windows.count(), m_windows.count());
        m_windows.append(window);
        endInsertRows();
        emit countChanged();
    } else {
        // Replace existing window pointer. This deallocates and reallocates even for
        // minor changes (e.g. title updates), but avoids property-by-property copying.
        delete m_windows[idx];
        m_windows[idx] = window;
        QModelIndex modelIdx = index(idx);
        emit dataChanged(modelIdx, modelIdx);
    }

    // If this window is focused, update all other windows
    if (window->isFocused) {
        for (int i = 0; i < m_windows.count(); ++i) {
            if (m_windows[i]->id != window->id && m_windows[i]->isFocused) {
                m_windows[i]->isFocused = false;
                QModelIndex modelIdx = index(i);
                emit dataChanged(modelIdx, modelIdx, {IsFocusedRole});
            }
        }
        emit focusedWindowChanged();
    }
}

void WindowModel::handleWindowClosed(quint64 id)
{
    int idx = findWindowIndex(id);
    if (idx == -1) {
        qCWarning(niriLog) << "Window not found for close:" << id;
        return;
    }

    bool wasFocused = m_windows[idx]->isFocused;

    beginRemoveRows(QModelIndex(), idx, idx);
    delete m_windows.takeAt(idx);
    endRemoveRows();

    emit countChanged();

    if (wasFocused) {
        emit focusedWindowChanged();
    }
}

void WindowModel::handleWindowFocusChanged(const QJsonValue &idValue)
{
    quint64 newFocusedId = idValue.isNull() ? 0 : idValue.toInteger();

    for (int i = 0; i < m_windows.count(); ++i) {
        bool shouldBeFocused = (m_windows[i]->id == newFocusedId);
        if (m_windows[i]->isFocused != shouldBeFocused) {
            m_windows[i]->isFocused = shouldBeFocused;
            QModelIndex modelIdx = index(i);
            emit dataChanged(modelIdx, modelIdx, {IsFocusedRole});
        }
    }

    emit focusedWindowChanged();
}

void WindowModel::handleWindowUrgencyChanged(quint64 id, bool urgent)
{
    int idx = findWindowIndex(id);
    if (idx == -1) {
        qCWarning(niriLog) << "Window not found for urgency change:" << id;
        return;
    }

    if (m_windows[idx]->isUrgent != urgent) {
        m_windows[idx]->isUrgent = urgent;
        QModelIndex modelIdx = index(idx);
        emit dataChanged(modelIdx, modelIdx, {IsUrgentRole});
    }
}

void WindowModel::handleWindowLayoutsChanged(const QJsonArray &changes)
{
    static const QList<int> layoutRoles = {
        ColumnIndexRole,
        TileIndexRole,
        TileWidthRole,
        TileHeightRole,
        WindowWidthRole,
        WindowHeightRole,
        TilePosXRole,
        TilePosYRole,
        WindowOffsetXRole,
        WindowOffsetYRole
    };

    for (const QJsonValue &changeValue : changes) {
        const QJsonArray change = changeValue.toArray();
        const quint64 id = change.at(0).toInteger();
        const QJsonObject layoutObj = change.at(1).toObject();

        const int idx = findWindowIndex(id);
        if (idx == -1) {
            qCWarning(niriLog) << "Window not found for layout change:" << id;
            continue;
        }

        parseWindowLayout(m_windows[idx], layoutObj);
        const QModelIndex modelIdx = index(idx);
        emit dataChanged(modelIdx, modelIdx, layoutRoles);
    }
}

Window* WindowModel::parseWindow(const QJsonObject &obj)
{
    Window *win = new Window(this);
    win->id = obj["id"].toInteger();
    win->title = obj["title"].toString();
    win->appId = obj["app_id"].toString();

    QJsonValue pidValue = obj["pid"];
    win->pid = pidValue.isNull() ? -1 : pidValue.toInt();

    QJsonValue workspaceIdValue = obj["workspace_id"];
    win->workspaceId = workspaceIdValue.isNull() ? 0 : workspaceIdValue.toInteger();

    win->isFocused = obj["is_focused"].toBool();
    win->isFloating = obj["is_floating"].toBool();
    win->isUrgent = obj["is_urgent"].toBool();
    parseWindowLayout(win, obj["layout"].toObject());
    win->iconPath = IconLookup::lookup(win->appId);

    return win;
}

void WindowModel::parseWindowLayout(Window *window, const QJsonObject &layoutObj)
{
    const QJsonArray scrollingPos = layoutObj.value("pos_in_scrolling_layout").toArray();
    window->columnIndex = scrollingPos.at(0).toInt();
    window->tileIndex = scrollingPos.at(1).toInt();

    const QJsonArray tileSize = layoutObj.value("tile_size").toArray();
    window->tileWidth = tileSize.at(0).toDouble();
    window->tileHeight = tileSize.at(1).toDouble();

    const QJsonArray windowSize = layoutObj.value("window_size").toArray();
    window->windowWidth = windowSize.at(0).toInt();
    window->windowHeight = windowSize.at(1).toInt();

    const qreal noTilePos = std::numeric_limits<qreal>::quiet_NaN();
    const QJsonArray tilePos = layoutObj.value("tile_pos_in_workspace_view").toArray();
    window->tilePosX = tilePos.at(0).toDouble(noTilePos);
    window->tilePosY = tilePos.at(1).toDouble(noTilePos);

    const QJsonArray windowOffset = layoutObj.value("window_offset_in_tile").toArray();
    window->windowOffsetX = windowOffset.at(0).toDouble();
    window->windowOffsetY = windowOffset.at(1).toDouble();
}

int WindowModel::findWindowIndex(quint64 id) const
{
    for (int i = 0; i < m_windows.count(); ++i) {
        if (m_windows[i]->id == id)
            return i;
    }
    return -1;
}
