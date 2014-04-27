/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * UartDmxDevice.h
 * The DMX through a UART plugin for ola
 * Copyright (C) 2011 Rui Barreiros
 * Copyright (C) 2014 Richard Ash
 */

#ifndef PLUGINS_UARTDMX_UARTDMXDEVICE_H_
#define PLUGINS_UARTDMX_UARTDMXDEVICE_H_

#include <string>
#include <sstream>
#include <memory>
#include "ola/DmxBuffer.h"
#include "olad/Device.h"
#include "olad/Preferences.h"
#include "plugins/uartdmx/UartWidget.h"

namespace ola {
namespace plugin {
namespace uartdmx {

class UartDmxDevice : public Device {
 public:
  UartDmxDevice(AbstractPlugin *owner,
                const std::string &name,
                const std::string &path,
                unsigned int device_id,
                unsigned int breakt,
                unsigned int malft);
  ~UartDmxDevice();

  std::string DeviceId() const {
    return static_cast<std::ostringstream*>
    (&(std::ostringstream() << m_widget->Number()))->str();
  }
  std::string Description() const {
     return m_path + ", " + DeviceId();
  }
  UartWidget* GetWidget() { return m_widget.get(); }

 protected:
  bool StartHook();

 private:
  std::auto_ptr<UartWidget> m_widget;
  std::string m_name;
  std::string m_path;
  unsigned int m_device_id;
  unsigned int m_breakt;
  unsigned int m_malft;
};
}  // namespace uartdmx
}  // namespace plugin
}  // namespace ola
#endif  // PLUGINS_UARTDMX_UARTDMXDEVICE_H_
