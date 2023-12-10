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

#include "DeviceListener.h"
#include <QTimer>

DeviceListener::DeviceListener(QWidget* parent)
    : QObject(parent)
    , m_platformImpl(new DEVICELISTENER_IMPL(parent))
{
    connect(impl(), &DEVICELISTENER_IMPL::devicePlugged, this, [&](bool state, void* ctx, void* device) {
        // Wait a few ms to prevent USB device access conflicts
        QTimer::singleShot(50, [&] { emit devicePlugged(state, ctx, device); });
    });
}

DeviceListener::~DeviceListener()
{
}

DEVICELISTENER_IMPL* DeviceListener::impl()
{
    return qobject_cast<DEVICELISTENER_IMPL*>(m_platformImpl.data());
}

void DeviceListener::registerHotplugCallback(bool arrived,
                                             bool left,
                                             int vendorId,
                                             int productId,
                                             const QUuid* deviceClass)
{
    qWarning("Register");
    impl()->registerHotplugCallback(arrived, left, vendorId, productId, deviceClass);
}

void DeviceListener::deregisterHotplugCallback()
{
    qWarning("Unregister");
    impl()->deregisterHotplugCallback();
}
