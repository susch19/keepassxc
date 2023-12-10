/*
 * Copyright (C) 2023 KeePassXC Team <team@keepassxc.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 or (at your option)
 * version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DEVICELISTENER_WIN_H
#define DEVICELISTENER_WIN_H

#define DEVICELISTENER_IMPL DeviceListenerWin

#include <QAbstractNativeEventFilter>
#include <QUuid>
#include <QWidget>

class DeviceListenerWin : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    static constexpr QUuid DEV_CLS_CCID =
        QUuid(0x50dd5230L, 0xba8a, 0x11d1, 0xbf, 0x5d, 0x00, 0x00, 0xf8, 0x05, 0xf5, 0x30);
    static constexpr QUuid DEV_CLS_HID =
        QUuid(0x745a17a0L, 0x74d3, 0x11d0, 0xb6, 0xfe, 0x00, 0xa0, 0xc9, 0x0f, 0x57, 0xda);
    static constexpr QUuid DEV_CLS_USB =
        QUuid(0x88bae032L, 0x5a81, 0x49f0, 0xbc, 0x3d, 0xa4, 0xff, 0x13, 0x82, 0x16, 0xd6);

    explicit DeviceListenerWin(QWidget* parent);
    DeviceListenerWin(const DeviceListenerWin&) = delete;
    ~DeviceListenerWin() override;

    void registerHotplugCallback(bool arrived,
                                 bool left,
                                 int vendorId = -1,
                                 int productId = -1,
                                 const QUuid* deviceClass = nullptr);
    void deregisterHotplugCallback();

    bool nativeEventFilter(const QByteArray& eventType, void* message, long*) override;

signals:
    void devicePlugged(bool state, void* ctx, void* device);

private:
    void* m_deviceNotifyHandle = nullptr;
    bool m_handleArrival = false;
    bool m_handleRemoval = false;
    QString m_deviceIdPrefix;
};

#endif // DEVICELISTENER_WIN_H
